#include "Session.h"
#include "Server.h"

#include "LockstepGroup.h"
#include "Util.h"
#include "Monitor.h"

Session::Session(const std::shared_ptr<ContextManager>& contextManager, const std::shared_ptr<ContextManager>& rpcContextManager)
    : _normalCtxManager(contextManager), _rpcCtxManager(rpcContextManager),
    _tcpSocketPtr(std::make_shared<TcpSocket>(_normalCtxManager->GetContext())),
    _lastRtt(0),
    _normalPrivateStrand(_normalCtxManager->GetContext()),
    _rpcPrivateStrand(_rpcCtxManager->GetContext())
{
    _isTcpSending = false;
    _isSerializingUdp = false;
    _userState = {};
}

void Session::Start()
{
    auto weakSelf(weak_from_this());

	// Set Timers
    _pingTimer = std::make_shared<Scheduler>(_normalPrivateStrand, std::chrono::milliseconds(_pingDelay), [weakSelf](CompletionHandler onComplete) {
        if (auto self = weakSelf.lock())
            self->SendPingPacket(onComplete);
        }
    );

    _sendStateTimer = std::make_shared<Scheduler>(_normalPrivateStrand, std::chrono::milliseconds(_sendStateDelay), [weakSelf](CompletionHandler onComplete) {
        if (auto self = weakSelf.lock())
            self->SendGameStatePacket(onComplete);
        }
    );

    // Async Functions Start
    TcpAsyncReadSize();

    _pingTimer->Start();
    _sendStateTimer->Start();
    _isConnected = true;

    spdlog::info("session {} started", _sessionInfo.uid());
}

void Session::Stop(bool forceStop)
{
    spdlog::info("{} session stopped", _sessionInfo.uid());
    _isConnected = false;

    _tcpSocketPtr->close();

    _pingTimer->Stop(1);
    _sendStateTimer->Stop(1);

    if (forceStop)
        return;

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
        spdlog::error("{} : network error occured ({}) in receive group info", _sessionInfo.uid(), ec.message());
        return false;
    }

    // parsing packet to RpcPacket
    RpcPacket receiveGroupInfo;
    if (!receiveGroupInfo.ParseFromArray(receiveBuffer.data(), static_cast<int>(receiveDataSize)))
    {
        spdlog::error("{} : parsing error occured in receive group info", _sessionInfo.uid());
        return false;
    }

    // parsing data to group dto
    if (!_groupDto.ParseFromString(receiveGroupInfo.data()))
    {
        spdlog::error("{} : parsing error occured in receive group info", _sessionInfo.uid());
        return false;
    }

    // parsing complete
    spdlog::info("{} : session exchange group info complete", _sessionInfo.uid());
    groupInfo = std::make_shared<GroupDto>(_groupDto);
    return true;
}

void Session::AsyncReceiveGroupInfo(std::function<void(bool success, std::shared_ptr<GroupDto> groupInfo)> onComplete)
{
    auto self(shared_from_this());
    boost::asio::post(_normalCtxManager->GetBlockingPool(), [self, onComplete]() {
        std::shared_ptr<GroupDto> group = nullptr;
        bool success = self->ReceiveGroupInfo(group); // exchange and set group reference

        boost::asio::post(self->_normalPrivateStrand, [self, success, group, onComplete]() {
            onComplete(success, group);
            });
        }
    );
}

// set by LockstepGroup.cpp
void Session::SetStopCallbackByGroup(StopCallback stopCallback)
{
    _onStopCallbackByGroup = std::move(stopCallback);
}

// set by Server.cpp
void Session::SetStopCallbackByServer(StopCallback stopCallback)
{
    _onStopCallbackByServer = std::move(stopCallback);
}

// set by Server.cpp
void Session::SetCollectInputAction(SessionInput inputAction)
{
    _inputAction = std::move(inputAction);
}

// called per frame
void Session::SerializeRpcPacketAndEnqueueData()
{
    if (_sendDataByUdp == nullptr)
    {
        spdlog::error("send callback is not set");
        _isSerializingUdp = false; // Stop the loop if callback is not set
        return;
    }

	auto self(shared_from_this());

    std::string serializedData;
    // Dequeue and serialize within the lock to ensure consistency
    {
        {
            std::lock_guard<std::mutex> lock(_sendUdpQueueMutex);
            if (_sendUdpPacketQueue.empty())
            {
                _isSerializingUdp = false; // Stop the loop
                return;
            }
        }

        auto nextPacket = DequeueSendUdpPackets(); // This is called within the lock
        if (!nextPacket.SerializeToString(&serializedData))
        {
            spdlog::error("{} : rpc packet serialize failed", _sessionInfo.uid());
            // Post again to continue the loop for other packets.
            boost::asio::post(_rpcPrivateStrand, [self]() { self->SerializeRpcPacketAndEnqueueData(); });
            return;
        }
    }

    auto sendDataPair = std::make_shared<std::pair<udp::endpoint, std::string>>(_udpSendEp, serializedData);
    _sendDataByUdp(std::move(sendDataPair));

    // Post again to process the next item in the queue.
    boost::asio::post(_rpcPrivateStrand, [self]() { self->SerializeRpcPacketAndEnqueueData(); });
}

// set by server.cpp
void Session::SetSendDataByUdpAction(SendDataByUdp sendDataFunction)
{
    _sendDataByUdp = std::move(sendDataFunction);
}

// called by server.cpp
void Session::CollectInput(std::shared_ptr<RpcPacket> receivePacket)
{
    auto self(shared_from_this());
    boost::asio::post(_rpcPrivateStrand.wrap([self, receivePacket] {
        if (self->_inputAction == nullptr)
        {
            spdlog::error("collect action is not set");
            return;
        }

        auto rpcRequest =
            std::make_shared<std::pair<uuid, std::shared_ptr<RpcPacket>>>(self->GetSessionUuid(), std::move(receivePacket));

        boost::asio::post(self->_rpcPrivateStrand.wrap([self, rpcRequest]() { self->_inputAction(std::move(rpcRequest)); }));
        boost::asio::post(self->_rpcPrivateStrand.wrap([self, rpcRequest]() {
            if (rpcRequest->second->method() == RpcMethod::Atk)
                return;

            {
                std::lock_guard<std::mutex> updateQueueLock(self->_statesQueueMutex);
                self->_statesQueue.push(*rpcRequest->second);

                // wake-up update own state function
                if (!self->_isOwnStateUpdating)
                {
                    self->_isOwnStateUpdating = true;
                    boost::asio::post(self->_normalPrivateStrand.wrap([self]() { self->AsyncUpdateOwnState(); }));
                }
            }
            
            })); 
        })
    );
}

void Session::EnqueueSendUdpPackets(const std::list<std::shared_ptr<SSendPacket>> sendPackets)
{
    std::lock_guard<std::mutex> lock(_sendUdpQueueMutex);
	for (const auto& packet : sendPackets)
	{
        // rpc packet deep copy
        RpcPacket listupPacket;
        listupPacket.CopyFrom(*packet->packet);

        // copied RpcPacket push
		_sendUdpPacketQueue.push(listupPacket);
	}

    if (!_isSerializingUdp)
    {
        _isSerializingUdp = true;
        boost::asio::post(_rpcPrivateStrand, [this]() { SerializeRpcPacketAndEnqueueData(); });
    }
}

RpcPacket Session::DequeueSendUdpPackets()
{
	std::lock_guard<std::mutex> lock(_sendUdpQueueMutex);
	auto nextPacket = _sendUdpPacketQueue.front();
	_sendUdpPacketQueue.pop();
	return nextPacket;
}

void Session::SendPingPacket(CompletionHandler onComplete)
{
    RpcPacket packet;
    packet.set_method(PING);
    const auto serializePingPacket = std::make_shared<std::string>(packet.SerializeAsString());
    _pingTime = Util::StartStopwatch();

    EnqueueTcpSendData(serializePingPacket);
    onComplete();
}

void Session::ProcessTcpRequest(const std::shared_ptr<RpcPacket> packet)
{
    switch (packet->method())
    {
    case PONG:
    {
        const auto rtt = Util::StopStopwatch(_pingTime);
        _lastRtt = rtt;

        ConsoleMonitor::Get().UpdateLatency(rtt); // latency average calculate

        RpcPacket rttPacket;
        rttPacket.set_method(LAST_RTT);

        std::string rttData = std::to_string(rtt);
        rttPacket.set_data(rttData);
        const auto serializedRttPacket = std::make_shared<std::string>(rttPacket.SerializeAsString());

        EnqueueTcpSendData(std::move(serializedRttPacket));
        break;
    }
    case MoveStart:
    case Move:
    case MoveStop:
        break;

    default:
        spdlog::error("{} : invalid packet method ({})", _sessionInfo.uid(), Util::MethodToString(packet->method()));
        break;
    }
}

void Session::EnqueueTcpSendData(std::shared_ptr<std::string> data)
{
    std::lock_guard<std::mutex> lock(_sendTcpQueueMutex);
    _sendTcpQueue.push(std::move(data));

    if (!_isTcpSending)
    {
        _isTcpSending = true;
        boost::asio::post(_normalPrivateStrand.wrap([this]() { TcpAsyncWrite(); }));
    }
}

void Session::TcpAsyncWrite()
{
    auto self(shared_from_this());
    std::shared_ptr<std::string> nextData;

    {
        std::lock_guard<std::mutex> lock(_sendTcpQueueMutex);
        if (_sendTcpQueue.empty())
        {
            _isTcpSending = false;
            return;
        }

        nextData = std::move(_sendTcpQueue.front());
        _sendTcpQueue.pop();
    }

    const std::int32_t dataSize = static_cast<std::int32_t>(nextData->size()); // 4 byte
    const std::int32_t netSize = htonl(dataSize); // 4 byte size Big-Endian

    boost::asio::async_write(*_tcpSocketPtr, boost::asio::buffer(&netSize, sizeof(netSize)),
        _normalPrivateStrand.wrap([self, nextData] (const boost::system::error_code& sizeEc, std::size_t) {
            if (sizeEc)
            {
                spdlog::error("{} : TCP error sending size ({})", self->_sessionInfo.uid(), sizeEc.message());
                // If an error occurs, we should probably stop the send loop, but for now, we just log and continue the loop to try the next message.
                boost::asio::post(self->_normalPrivateStrand.wrap([self]() { self->TcpAsyncWrite(); }));
                return;
            }

            boost::asio::async_write(*self->_tcpSocketPtr, boost::asio::buffer(*nextData),
                self->_normalPrivateStrand.wrap([self, nextData](const boost::system::error_code& dataEc, std::size_t) {
                    if (dataEc)
                    {
                        spdlog::error("TCP error sending data to {} : {}", self->_sessionInfo.uid(), dataEc.message());
                    }

                    // Continue the loop for the next item.
                    boost::asio::post(self->_normalPrivateStrand.wrap([self]() { self->TcpAsyncWrite(); }));
                    }
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
                    self->Stop(false);
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
                if (dataEc == boost::asio::error::eof 
                    || dataEc == boost::asio::error::connection_reset
                    || dataEc == boost::asio::error::connection_aborted
                    || dataEc == boost::asio::error::operation_aborted)
                {
                    self->Stop(false);
                    return;
                }

                spdlog::error("{} : TCP error receiving data ({})", self->_sessionInfo.uid(), dataEc.message());
                self->TcpAsyncReadSize();
                return;
            }

            ConsoleMonitor::Get().IncrementTcpPacket();

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

void Session::AsyncUpdateOwnState()
{
    auto self(shared_from_this());
    boost::asio::post(_normalPrivateStrand.wrap([self]() {
        std::queue<RpcPacket> localQueue;
        {
            std::lock_guard<std::mutex> lock(self->_statesQueueMutex);
            if (self->_statesQueue.empty())
            {
                self->_isOwnStateUpdating = false;
                return;
            }

            // all update packets moved local queue
            self->_statesQueue.swap(localQueue);
        }

        // dequeue and process moved local queue
        while (!localQueue.empty())
        {
            const auto nextPacket = localQueue.front();
            localQueue.pop();

            switch (nextPacket.method())
            {
            case RpcMethod::MoveStart:
            case RpcMethod::Move:
            case RpcMethod::MoveStop:
            {
                MoveData newMoveData;
                // parsing move data and apply
                if (!newMoveData.ParseFromString(nextPacket.data()))
                {
                    spdlog::error("{} error parsing move data for update own state", self->_sessionInfo.uid());
                    return;
                }

                std::lock_guard<std::mutex> stateLock(self->_stateMutex);
                self->_userState.position.SetPosition(newMoveData.x(), newMoveData.y(), newMoveData.z());
                break;
            }
            case RpcMethod::Hit:
            {
                // 값 만큼 hp 감소
                std::int32_t value = stoi(nextPacket.data());

                if (value < 0)
                {
                    spdlog::error("[internal] invalid value from hit damage: {}, owner: {}", value, nextPacket.uid());
                    break;
                }

                std::lock_guard<std::mutex> stateLock(self->_stateMutex);
                self->_userState.hp -= value;
                break;
            }
            default:
                spdlog::error("{} invalid method in update state: {}", self->_sessionInfo.uid(), Util::MethodToString(nextPacket.method()));
                break;
            }
        }

        std::lock_guard<std::mutex> lock(self->_statesQueueMutex);
        if (self->_statesQueue.empty())
        {
            self->_isOwnStateUpdating = false;
            return;
        }

        // if queue is not empty -> re process
        boost::asio::post([self]() { self->AsyncUpdateOwnState(); });
        })
    );
}

void Session::SendGameStatePacket(CompletionHandler onComplete)
{
    RpcPacket packet;
    packet.set_method(CLIENT_GAME_INFO);

    Util::SUserState curGameState = GetGameState();

    GameData gameData;
    MoveData* moveData = gameData.mutable_position();
    moveData->set_x(curGameState.position.x);
    moveData->set_y(curGameState.position.y);
    moveData->set_z(curGameState.position.z);
    gameData.set_hp(curGameState.hp);

    auto serializedGameData = gameData.SerializeAsString();
    packet.set_data(serializedGameData);

    const auto serializedPacket = std::make_shared<std::string>(packet.SerializeAsString());
    EnqueueTcpSendData(std::move(serializedPacket));

    onComplete(); // call again this function by scheduler
}
