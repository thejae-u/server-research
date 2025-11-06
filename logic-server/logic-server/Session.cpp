#include "Session.h"
#include "Server.h"

#include "LockstepGroup.h"
#include "Utility.h"

Session::Session(const std::shared_ptr<ContextManager>& contextManager, const std::shared_ptr<ContextManager>& rpcContextManager)
    : _normalCtxManager(contextManager), _rpcCtxManager(rpcContextManager),
    _tcpSocketPtr(std::make_shared<TcpSocket>(_normalCtxManager->GetContext())),
    //_udpSocketPtr(std::make_shared<UdpSocket>(_rpcCtxManager->GetContext(), udp::endpoint(udp::v4(), 0))),
    _lastRtt(0),
    _normalPrivateStrand(_normalCtxManager->GetContext()),
    _rpcPrivateStrand(_rpcCtxManager->GetContext())
{
}

void Session::Start()
{
    auto self(shared_from_this());
    _pingTimer = std::make_shared<Scheduler>(_normalPrivateStrand, std::chrono::milliseconds(_pingDelay), [self](CompletionHandler onComplete) {
        self->SendPingPacket(onComplete);
        }
    );

    // Async Functions Start
    TcpAsyncReadSize();
    //UdpAsyncRead();
    SerializeRpcPacketAndEnqueueData();

    _pingTimer->Start();
    _isConnected = true;

    spdlog::info("session {} started", _sessionInfo.uid());
}

void Session::Stop()
{
    spdlog::info("{} session stopped", _sessionInfo.uid());
    _isConnected = false;

    _tcpSocketPtr->close();
    //_udpSocketPtr->close(); // close udp socket for aysnc function exit

    _pingTimer->Stop();
    _onStopCallbackByGroup(shared_from_this());
    _onStopCallbackByServer(shared_from_this());
}

// wrapping ExchangeUdpPort, Using Blocking Pool
void Session::AsyncExchangeUdpPortWork(std::uint16_t udpPort, std::function<void(bool success)> onComplete)
{
    auto self(shared_from_this());

    boost::asio::post(_normalCtxManager->GetBlockingPool(), [self, onComplete, udpPort]() {
        bool success = self->ExchangeUdpPort(udpPort);
        boost::asio::post(self->_normalPrivateStrand, [self, success, onComplete]() {
            onComplete(success);
            });
        }
    );
}

bool Session::ExchangeUdpPort(std::uint16_t udpPort)
{
    const std::string connectedIp = _tcpSocketPtr->remote_endpoint().address().to_string();
    //auto netUdpPort = htons(_udpSocketPtr->local_endpoint().port()); // Session Udp Socket Port
    auto netUdpPort = htons(udpPort); // Server Main Udp Socket Port
    std::string sendUdpByte(reinterpret_cast<char*>(&netUdpPort), sizeof(netUdpPort));

    spdlog::info("port {} try exchange", ntohs(netUdpPort));

    RpcPacket packet;
    packet.set_method(UDP_PORT);
    packet.set_data(sendUdpByte);

    std::string udpPortPacket;
    if (!packet.SerializeToString(&udpPortPacket))
    {
        spdlog::error("failed to serialize UDP port packet {}", connectedIp);
        return false;
    }

    const auto sendDataSize = static_cast<uint32_t>(udpPortPacket.size());
    auto sendNetSize = htonl(sendDataSize);

    std::vector<char> receiveBuffer;
    uint32_t receiveNetSize = 0;

    // 예외 처리를 위한 system::error_code
    error_code ec;
    boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)), ec); // send size
    if (!ec) boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(udpPortPacket), ec); // send data
    if (!ec) boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(&receiveNetSize, sizeof(uint32_t)), ec); // receive size

    // recive size calculate
    const auto receiveDataSize = ntohl(receiveNetSize);
    if (!ec && receiveDataSize > 0)
    {
        receiveBuffer.resize(receiveDataSize);
        if (!ec) boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(receiveBuffer), ec); // receive data
    }

    // 어느 지점에서든 error_code가 있을 때 -> 비정상 작동 종료
    if (ec || receiveDataSize == 0)
    {
        spdlog::error("{} : network error occured ({})", connectedIp, ec.message());
        return false;
    }

    // parse receive data
    RpcPacket receiveUdpPortPacket;
    if (!receiveUdpPortPacket.ParseFromArray(receiveBuffer.data(), static_cast<int>(receiveDataSize)))
    {
        spdlog::error("{} : failed to parse UDP port packet", connectedIp);
        return false;
    }

    if (receiveUdpPortPacket.method() != UDP_PORT)
    {
        // Invalid Method 확인
        const auto* descriptor = RpcMethod_descriptor();
        const auto& methodName = descriptor->FindValueByNumber(receiveUdpPortPacket.method())->name();
        spdlog::error("{} : invalid method ({})", connectedIp, methodName);
        return false;
    }

    // network to host
    std::uint16_t netClientPort;
    std::memcpy(&netClientPort, receiveUdpPortPacket.data().data(), sizeof(netClientPort));
    auto clientPort = ntohs(netClientPort);

    // Set the UDP endpoint
    _udpSendEp = udp::endpoint(_tcpSocketPtr->remote_endpoint().address(), clientPort);
    spdlog::info("client port get {}", clientPort);
    return true;
}

bool Session::ReceiveUserInfo()
{
    const std::string connectedIp = _tcpSocketPtr->remote_endpoint().address().to_string();

    RpcPacket packet;
    packet.set_method(USER_INFO);

    auto serializedData = packet.SerializeAsString();
    const uint32_t sendDataSize = static_cast<uint32_t>(serializedData.size());
    const uint32_t sendNetSize = htonl(sendDataSize);

    uint32_t receiveNetSize = 0;
    std::vector<char> receiveBuffer;

    // 순차적으로 error_code가 없는 경우 다음 스텝을 진행
    error_code ec;
    boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)), ec);
    if (!ec) boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(serializedData), ec);
    if (!ec) boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(&receiveNetSize, sizeof(uint32_t)), ec);

    const auto receiveDataSize = ntohl(receiveNetSize);
    if (!ec && receiveNetSize > 0)
    {
        receiveBuffer.resize(receiveDataSize);
        boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(receiveBuffer), ec);
    }

    // 어느 시점이든 오류가 나면 처리
    if (ec || receiveNetSize == 0)
    {
        spdlog::error("{} : network error occured ({}) in receive user info", connectedIp, ec.message());
        return false;
    }

    RpcPacket receivePacket;
    if (!receivePacket.ParseFromArray(receiveBuffer.data(), static_cast<int>(receiveDataSize)))
    {
        spdlog::error("{} : parsing error occured in receive user info", connectedIp);
        return false;
    }

    if (receivePacket.method() != USER_INFO)
    {
        // Invalid Method Check
        const auto* descriptor = RpcMethod_descriptor();
        const auto& methodName = descriptor->FindValueByNumber(receivePacket.method())->name();
        spdlog::error("{} : invalid method ({})", connectedIp, methodName);
        return false;
    }

    _sessionInfo.set_uid(receivePacket.uid());
    _sessionInfo.set_username(receivePacket.data().data());

    spdlog::info("{} : session user info exchanged complete", _sessionInfo.uid());
    return true;
}

// warpping ReceiveUserInfo, Using BlockingPool
void Session::AsyncReceiveUserInfo(std::function<void(bool success)> onComplete)
{
    auto self(shared_from_this());
    boost::asio::post(_normalCtxManager->GetBlockingPool(), [self, onComplete]() {
        bool success = self->ReceiveUserInfo();

        boost::asio::post(self->_normalPrivateStrand, [self, success, onComplete]() {
            onComplete(success);
            });
        }
    );
}

bool Session::ReceiveGroupInfo(std::shared_ptr<GroupDto>& groupInfo)
{
    const auto connectedIp = _tcpSocketPtr->remote_endpoint().address().to_string();
    RpcPacket packet;
    packet.set_method(GROUP_INFO);

    const auto serializedData = packet.SerializeAsString();
    const auto sendDataSize = static_cast<std::uint32_t>(serializedData.size());
    const auto sendNetSize = htonl(sendDataSize);

    std::uint32_t receiveNetSize = 0;
    std::vector<char> receiveBuffer;

    error_code ec;
    boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)), ec);
    if (!ec) boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(serializedData), ec);
    if (!ec) boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(&receiveNetSize, sizeof(std::uint32_t)), ec);

    const auto receiveDataSize = ntohl(receiveNetSize);
    if (!ec && receiveDataSize > 0)
    {
        receiveBuffer.resize(receiveDataSize);
        boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(receiveBuffer), ec);
    }

    if (ec || receiveDataSize == 0)
    {
        spdlog::error("{} : network error occured ({}) in receive group info", connectedIp, ec.message());
        return false;
    }

    RpcPacket receiveGroupInfo;
    if (!receiveGroupInfo.ParseFromArray(receiveBuffer.data(), static_cast<int>(receiveDataSize)))
    {
        spdlog::error("{} : parsing error occured in receive group info", connectedIp);
        return false;
    }

    // Group Dto Json Parsing
    auto status = google::protobuf::util::JsonStringToMessage(receiveGroupInfo.data().data(), &_groupDto);
    if (status.message().size() > 0)
    {
        spdlog::error("{} : protobuf json parsing error occured ({}) in receive group info", connectedIp, status.message());
        return false;
    }

    spdlog::info("{} : session exchange group info complete", _sessionInfo.uid());
    groupInfo = std::make_shared<GroupDto>(_groupDto);
    return true;
}

void Session::AysncReceiveGroupInfo(std::function<void(bool success, std::shared_ptr<GroupDto> groupInfo)> onComplete)
{
    auto self(shared_from_this());
    boost::asio::post(_normalCtxManager->GetBlockingPool(), [self, onComplete]() {
        std::shared_ptr<GroupDto> group = nullptr;
        bool success = self->ReceiveGroupInfo(group);

        boost::asio::post(self->_normalPrivateStrand, [self, success, group, onComplete]() {
            onComplete(success, group);
            });
        }
    );
}

void Session::SetStopCallbackByGroup(StopCallback stopCallback)
{
    _onStopCallbackByGroup = std::move(stopCallback);
}

void Session::SetStopCallbackByServer(StopCallback stopCallback)
{
    _onStopCallbackByServer = std::move(stopCallback);
}

void Session::SetCollectInputAction(SessionInput inputAction)
{
    _inputAction = std::move(inputAction);
}

void Session::SerializeRpcPacketAndEnqueueData()
{
    if (_sendDataByUdp == nullptr)
    {
        spdlog::error("send callback is not set");
        return;
    }

	auto self(shared_from_this());

	// lock for check empty queue
	{
		std::lock_guard<std::mutex> lock(_sendQueueMutex);
		if (_sendPacketQueue.empty())
		{
            boost::asio::post(_rpcPrivateStrand, [self]() { self->SerializeRpcPacketAndEnqueueData(); }); // restart this function
			return;
		}
	}

	auto nextPacket = DequeueSendPacket(); // this function will lock queue
    std::string serializedData = "";
	if(!nextPacket.SerializeToString(&serializedData))
	{
		spdlog::error("{} : rpc packet serialize failed", _sessionInfo.uid());
		boost::asio::post(_rpcPrivateStrand, [self]() { self->SerializeRpcPacketAndEnqueueData(); }); // restart this function
		return;
	}

	//UdpAsyncWrite(std::make_shared<std::string>(serializedData));
     auto sendDataPair = std::make_shared<std::pair<udp::endpoint, std::string>>(_udpSendEp, serializedData);
    _sendDataByUdp(std::move(sendDataPair));

	boost::asio::post(_rpcPrivateStrand, [self]() { self->SerializeRpcPacketAndEnqueueData(); }); // restart this function
}

void Session::SetSendDataByUdpAction(SendDataByUdp sendDataFunction)
{
    _sendDataByUdp = std::move(sendDataFunction);
}

void Session::CollectInput(std::shared_ptr<RpcPacket> receivePacket)
{
    auto self(shared_from_this());
    boost::asio::post(_rpcPrivateStrand.wrap([self, receivePacket] {
        if (self->_inputAction == nullptr)
        {
            spdlog::error("collect action is not set");
            return;
        }

        const auto rpcRequest =
            std::make_shared<std::pair<uuid, std::shared_ptr<RpcPacket>>>(self->GetSessionUuid(), std::move(receivePacket));
        self->_inputAction(rpcRequest);

        spdlog::info("collect by session complete");
        })
    );
}

void Session::EnqueueSendPackets(const std::list<std::shared_ptr<SSendPacket>> sendPackets)
{
	for (const auto& packet : sendPackets)
	{
        // rpc packet deep copy
        RpcPacket listupPacket;
        listupPacket.CopyFrom(*packet->packet);

        // copied RpcPacket push
	    std::lock_guard<std::mutex> lock(_sendQueueMutex);
		_sendPacketQueue.push(listupPacket);
	}
}

RpcPacket Session::DequeueSendPacket()
{
	std::lock_guard<std::mutex> lock(_sendQueueMutex);
	auto nextPacket = _sendPacketQueue.front();
	_sendPacketQueue.pop();
	return nextPacket;
}

void Session::SendPingPacket(CompletionHandler onComplete)
{
    RpcPacket packet;
    packet.set_method(PING);
    const auto serializePingPacket = std::make_shared<std::string>(packet.SerializeAsString());
    const uint32_t sendDataSize = static_cast<uint32_t>(serializePingPacket->size());
    const uint32_t sendNetSize = htonl(sendDataSize);

    _pingTime = Utility::StartStopwatch();

    error_code ec;
    boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)), ec);
    if (!ec) boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(*serializePingPacket));

    // error occured log
    if (ec)
    {
        spdlog::error("{} : network error occured ({})", _sessionInfo.uid(), ec.message());
    }

    onComplete();
}

void Session::ProcessTcpRequest(const std::shared_ptr<RpcPacket> packet)
{
	if (packet->method() == PONG)
	{
		const auto rtt = Utility::StopStopwatch(_pingTime);
		_lastRtt = rtt;

		RpcPacket rttPacket;
		rttPacket.set_method(LAST_RTT);

		std::string rttData = std::to_string(rtt);
		rttPacket.set_data(rttData);
		const auto serializedRttPacket = std::make_shared<std::string>(rttPacket.SerializeAsString());

		TcpAsyncWrite(serializedRttPacket); // Send the last RTT back to the client
	}
    else
    {
        spdlog::error("{} : invalid packet method ({})", _sessionInfo.uid(), Utility::MethodToString(packet->method()));
    }
}

void Session::TcpAsyncWrite(const std::shared_ptr<std::string> data)
{
    auto self(shared_from_this());
    const std::uint32_t dataSize = static_cast<std::uint32_t>(data->size());
    const std::uint32_t netSize = htonl(dataSize);

    boost::asio::async_write(*_tcpSocketPtr, boost::asio::buffer(&netSize, sizeof(netSize)),
        _normalPrivateStrand.wrap([self, data](const boost::system::error_code& sizeEc, std::size_t) {
            if (sizeEc)
            {
                spdlog::error("{} : TCP error sending size ({})", self->_sessionInfo.uid(), sizeEc.message());
                return;
            }

            boost::asio::async_write(*self->_tcpSocketPtr, boost::asio::buffer(*data),
                self->_normalPrivateStrand.wrap([self, data](const boost::system::error_code& dataEc, std::size_t) {
                    if (dataEc)
                    {
                        spdlog::error("TCP error sending data to {} : {}", self->_sessionInfo.uid(), dataEc.message());
                        return;
                    }}
                ));
            }
        )
    );
}

void Session::TcpAsyncReadSize()
{
    _tcpDataSize = 0;
    _tcpNetSize = 0;

    auto self(shared_from_this());
    boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(&_tcpNetSize, sizeof(_tcpNetSize)),
        _normalPrivateStrand.wrap([self] (const boost::system::error_code& sizeEc, std::size_t) {
            if (sizeEc)
            {
                if (sizeEc == boost::asio::error::eof
                    || sizeEc == boost::asio::error::connection_reset
                    || sizeEc == boost::asio::error::connection_aborted
                    || sizeEc == boost::asio::error::operation_aborted)
                {
                    spdlog::info("session {} : TcpaSyncRead aborted", self->_sessionInfo.uid());
                    self->Stop();
                    return;
                }

                spdlog::error("{} : TCP error receiving size ({})", self->_sessionInfo.uid(), sizeEc.message());
                return;
            }

            // Convert network byte order to host byte order
            self->_tcpDataSize = ntohl(self->_tcpNetSize);

            if (self->_tcpDataSize > MAX_PACKET_SIZE)
            {
                spdlog::error("{} : TCP error receiving data size ({})", self->_sessionInfo.uid(), self->_tcpDataSize);
                self->TcpAsyncReadSize();
                return;
            }

            const auto dataBuffer = std::make_shared<std::vector<char>>(self->_tcpDataSize, 0);
            self->TcpAsyncReadData(dataBuffer);
            }
        )
    );
}

void Session::TcpAsyncReadData(std::shared_ptr<std::vector<char>> dataBuffer)
{
    auto self(shared_from_this());
    boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(*dataBuffer),
        _normalPrivateStrand.wrap([self, dataBuffer](const boost::system::error_code& dataEc, std::size_t) {
            if (dataEc)
            {
                if (dataEc == boost::asio::error::eof || dataEc == boost::asio::error::connection_reset)
                {
                    self->Stop();
                    return;
                }

                spdlog::error("{} : TCP error receiving data ({})", self->_sessionInfo.uid(), dataEc.message());
                self->TcpAsyncReadSize();
                return;
            }

            RpcPacket deserializeRpcPacket;
            if (!deserializeRpcPacket.ParseFromArray(dataBuffer->data(), static_cast<int>(dataBuffer->size())))
            {
                spdlog::error("{} : error parsing rpc packet", self->_sessionInfo.uid());
                self->TcpAsyncReadSize();
                return;
            }

            const auto packetPtr = std::make_shared<RpcPacket>(deserializeRpcPacket);
            self->TcpAsyncReadSize();
            self->ProcessTcpRequest(packetPtr);
            }
        )
    );
}

void Session::UdpAsyncRead()
{
    if (!_udpSocketPtr || !_udpSocketPtr->is_open())
        return;

    // max size buffer for udp packet
    const auto buf = std::make_shared<std::vector<char>>(MAX_PACKET_SIZE);
    const auto senderEndpoint = std::make_shared<udp::endpoint>();
    auto self(shared_from_this());

    _udpSocketPtr->async_receive_from(boost::asio::buffer(*buf), *senderEndpoint,
        _rpcPrivateStrand.wrap([self, buf](const boost::system::error_code& ec, const std::size_t bytesRead) {
            if (ec)
            {
                if (ec == boost::asio::error::eof || ec == boost::asio::error::operation_aborted)
                {
                    spdlog::info("UDP receive operation aborted or EOF");
                    return;
                }

                spdlog::error("{} : UDP read error ({})", self->_sessionInfo.uid(), ec.message());
                self->UdpAsyncRead();
                return;
            }

            if (bytesRead < sizeof(std::uint16_t))
            {
                spdlog::error("{} : invalid packet received (bytes read under 2bytes)", self->_sessionInfo.uid());
                self->UdpAsyncRead();
                return;
            }

            // payload size
            std::uint16_t payloadSize = 0;
            std::memcpy(&payloadSize, buf->data(), sizeof(std::uint16_t));
            payloadSize = ntohs(payloadSize);

            if (payloadSize == 0 || payloadSize + sizeof(std::uint16_t) > bytesRead)
            {
                spdlog::error("{} : invalid payload size (payloadSize is 0 or over bytesRead)", self->_sessionInfo.uid());
                self->UdpAsyncRead();
                return;
            }

            RpcPacket packet;
            if (!packet.ParseFromArray(buf->data() + sizeof(std::uint16_t), payloadSize))
            {
                spdlog::error("{} : error parsing rpc packet", self->_sessionInfo.uid());
                self->UdpAsyncRead();
                return;
            }

            // Input Send to Lockstep Group
            if (self->_inputAction)
            {
                const auto rpcRequest =
                    std::make_shared<std::pair<uuid, std::shared_ptr<RpcPacket>>>(self->_toUuid(self->_sessionInfo.uid()), std::make_shared<RpcPacket>(packet));
                self->_inputAction(rpcRequest);
            }

            self->UdpAsyncRead();
            }
        )
    );
}

void Session::UdpAsyncWrite(std::shared_ptr<std::string> data)
{
    // make udp packet
    const std::uint16_t payloadSize = static_cast<std::uint16_t>(data->size());
    const std::uint16_t netPayloadSize = htons(payloadSize);

    const auto payload = std::make_shared<std::string>();
    payload->append(reinterpret_cast<const char*>(&netPayloadSize), sizeof(std::uint16_t));
    payload->append(*data);

    auto self(shared_from_this());
    // Send packet to client
    _udpSocketPtr->async_send_to(boost::asio::buffer(*payload), _udpSendEp,
        _rpcPrivateStrand.wrap([self, payload](const boost::system::error_code& ec, std::size_t) {
            if (ec)
            {
                spdlog::error("{} : UDP error sending data ({})", self->_sessionInfo.uid(), ec.message());
                return;
            }

            spdlog::info("{} : send packet to {} {}", self->_sessionInfo.uid(), self->_udpSendEp.address().to_string(), self->_udpSendEp.port());
            }
        )
    );
}