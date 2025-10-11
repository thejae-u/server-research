#include "Session.h"
#include "Server.h"

#include "LockstepGroup.h"
#include "Utility.h"

Session::Session(const std::shared_ptr<ContextManager>& contextManager, const std::shared_ptr<ContextManager>& rpcContextManager, const uuid guid)
	: _ctxManager(contextManager), _rpcCtxManager(rpcContextManager),
	_tcpSocketPtr(std::make_shared<TcpSocket>(_ctxManager->GetStrand().context())),
	_udpSocketPtr(std::make_shared<UdpSocket>(_rpcCtxManager->GetStrand().context(), udp::endpoint(udp::v4(), 0))),
	_sessionUuid(guid),
	_lastRtt(0)
{
	_pingTimer = std::make_shared<Scheduler>(_ctxManager->GetStrand(), std::chrono::milliseconds(_pingDelay), [this]() {
		SendPingPacket();
		});
}

void Session::Start()
{
	spdlog::info("session {} started", to_string(_sessionUuid));

	// Async Functions Start
	TcpAsyncReadSize();
	_pingTimer->Start();
	UdpAsyncRead();

	_isConnected = true;
}

void Session::Stop()
{
	spdlog::info("{} session stopped", to_string(_sessionUuid));
	_isConnected = false;

	_tcpSocketPtr->close();
	_udpSocketPtr->close();

	_pingTimer->Stop();
	_onStopCallback(shared_from_this());
}

// wrapping ExchangeUdpPort, Using Blocking Pool
void Session::AsyncExchangeUdpPortWork(std::function<void(bool success)> onComplete)
{
	auto self(shared_from_this());

	boost::asio::post(_ctxManager->GetBlockingPool(), [self, onComplete]() {
		bool success = self->ExchangeUdpPort();
		boost::asio::post(self->_ctxManager->GetStrand(), [self, success, onComplete]() {
			onComplete(success);
			});
		}
	);
}

bool Session::ExchangeUdpPort()
{
	const std::uint16_t udpPort = _udpSocketPtr->local_endpoint().port();
	std::string udpPortString = std::to_string(udpPort);

	spdlog::info("UDP port {} for session {}", udpPortString, to_string(_sessionUuid));

	RpcPacket packet;
	packet.set_method(UDP_PORT);
	packet.set_data(udpPortString);

	std::string udpPortPacket;
	if (!packet.SerializeToString(&udpPortPacket))
	{
		spdlog::error("{} : failed to serialize UDP port packet", to_string(_sessionUuid));
		return false;
	}

	const auto sendDataSize = static_cast<uint32_t>(udpPortPacket.size());
	auto sendNetSize = htonl(sendDataSize);

	std::vector<char> receiveBuffer;
	uint32_t receiveNetSize;

	// 예외 처리를 위한 system::error_code
	error_code ec;
	boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)), ec);
	if (!ec) boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(udpPortPacket), ec);
	if (!ec) boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(&receiveNetSize, sizeof(uint32_t)), ec);

	const auto receiveDataSize = ntohl(receiveNetSize);
	if (!ec && receiveDataSize > 0)
	{
		receiveBuffer.resize(receiveDataSize);
		if (!ec) boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(receiveBuffer), ec);
	}

	// 어느 지점에서든 error_code가 있을 때 -> 비정상 작동 종료
	if (ec)
	{
		spdlog::error("{} : network error occured ({})", to_string(_sessionUuid), ec.message());
		return false;
	}

	RpcPacket receivePacket;
	if (!receivePacket.ParseFromArray(receiveBuffer.data(), static_cast<int>(receiveDataSize)))
	{
		spdlog::error("{} : failed to parse UDP port packet", to_string(_sessionUuid));
		return false;
	}

	if (receivePacket.method() != UDP_PORT)
	{
		// Invalid Method 확인
		const auto* descriptor = RpcMethod_descriptor();
		const auto& methodName = descriptor->FindValueByNumber(receivePacket.method())->name();
		spdlog::error("{} : invalid method ({})", to_string(_sessionUuid), methodName);
		return false;
	}

	// Big endian to little endian
	std::uint16_t clientPort;
	std::memcpy(&clientPort, receivePacket.data().data(), sizeof(clientPort));

	// Set the UDP endpoint
	_udpSendEp = udp::endpoint(_tcpSocketPtr->remote_endpoint().address(), clientPort);

	return true;
}

// warpping SendUUidToClient, Using BlockingPool
void Session::AsyncSendUuidToClientWork(std::function<void(bool success)> onComplete)
{
	auto self(shared_from_this());

	boost::asio::post(_ctxManager->GetBlockingPool(), [self, onComplete]() {
		bool success = self->SendUuidToClient();

		boost::asio::post(self->_ctxManager->GetStrand(), [self, success, onComplete]() {
			onComplete(success);
			});
		}
	);
}

bool Session::SendUuidToClient() const
{
	// Send uuid to client
	RpcPacket packet;
	packet.set_uuid(Utility::GuidToBytes(_sessionUuid));
	packet.set_method(UUID);

	auto serializedGuid = packet.SerializeAsString();
	const uint32_t sendNetSize = static_cast<uint32_t>(serializedGuid.size());
	const uint32_t sendDataSize = htonl(sendNetSize);

	error_code ec;
	boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendDataSize, sizeof(sendDataSize)), ec);
	if (!ec) boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(serializedGuid), ec);

	// error occured
	if (ec)
	{
		spdlog::error("{} : network error occured ({})", to_string(_sessionUuid), ec.message());
		return false;
	}

	return true;
}

void Session::SetStopCallback(StopCallback stopCallback)
{
	_onStopCallback = std::move(stopCallback);
}

void Session::SetCollectInputAction(SessionInput inputAction)
{
	_inputAction = std::move(inputAction);
}

void Session::SendRpcPacketToClient(std::unordered_map<SSessionKey, std::shared_ptr<RpcPacket>> allInputs)
{
	std::unordered_map<SSessionKey, RpcPacket> copiedInputs;
	for (const auto& [key, packet] : allInputs)
	{
		copiedInputs[key] = *packet;
	}

	for (const auto& [key, packet] : copiedInputs)
	{
		const auto serializedData = std::make_shared<std::string>();
		if (packet.SerializeToString(serializedData.get()))
		{
			spdlog::error("{} : rpc packet serialize failed for key {}", to_string(_sessionUuid), to_string(key.guid));
			return;
		}

		UdpAsyncWrite(serializedData);
	}
}

void Session::SendPingPacket()
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

	// error occured
	if (ec)
	{
		spdlog::error("{} : network error occured ({})", to_string(_sessionUuid), ec.message());
	}
}

void Session::ProcessTcpRequest(const std::shared_ptr<RpcPacket>& packet)
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
		spdlog::error("{} : invalid packet method ({})", to_string(_sessionUuid), Utility::MethodToString(packet->method()));
	}
}

void Session::TcpAsyncWrite(const std::shared_ptr<std::string>& data)
{
	auto self(shared_from_this());

	const std::uint32_t dataSize = static_cast<std::uint32_t>(data->size());
	const std::uint32_t netSize = htonl(dataSize);

	boost::asio::async_write(*_tcpSocketPtr, boost::asio::buffer(&netSize, sizeof(netSize)),
		_ctxManager->GetStrand().wrap([self, data](const boost::system::error_code& sizeEc, std::size_t) {
			if (sizeEc)
			{
				spdlog::error("{} : TCP error sending size ({})", to_string(self->_sessionUuid), sizeEc.message());
				return;
			}

			boost::asio::async_write(*self->_tcpSocketPtr, boost::asio::buffer(*data),
				self->_ctxManager->GetStrand().wrap([self, data](const boost::system::error_code& dataEc, std::size_t) {
					if (dataEc)
					{
						spdlog::error("TCP error sending data to {} : {}", to_string(self->_sessionUuid), dataEc.message());
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

	boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(&_tcpNetSize, sizeof(_tcpNetSize)),
		_ctxManager->GetStrand().wrap([self(shared_from_this())](const boost::system::error_code& sizeEc, std::size_t) {
			if (sizeEc)
			{
				if (sizeEc == boost::asio::error::eof
					|| sizeEc == boost::asio::error::connection_reset
					|| sizeEc == boost::asio::error::connection_aborted
					|| sizeEc == boost::asio::error::operation_aborted)
				{
					spdlog::info("session {} : TcpaSyncRead aborted", to_string(self->_sessionUuid));
					self->Stop();
					return;
				}

				spdlog::error("{} : TCP error receiving size ({})", to_string(self->_sessionUuid), sizeEc.message());
				return;
			}

			// Convert network byte order to host byte order
			self->_tcpDataSize = ntohl(self->_tcpNetSize);

			if (self->_tcpDataSize > MAX_PACKET_SIZE)
			{
				spdlog::error("{} : TCP error receiving data size ({})", to_string(self->_sessionUuid), self->_tcpDataSize);
				self->TcpAsyncReadSize();
				return;
			}

			const auto dataBuffer = std::make_shared<std::vector<char>>(self->_tcpDataSize, 0);
			self->TcpAsyncReadData(dataBuffer);
			}
		)
	);
}

void Session::TcpAsyncReadData(const std::shared_ptr<std::vector<char>>& dataBuffer)
{
	boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(*dataBuffer),
		_ctxManager->GetStrand().wrap([self(shared_from_this()), dataBuffer](const boost::system::error_code& dataEc, std::size_t) {
			if (dataEc)
			{
				if (dataEc == boost::asio::error::eof || dataEc == boost::asio::error::connection_reset)
				{
					self->Stop();
					return;
				}

				spdlog::error("{} : TCP error receiving data ({})", to_string(self->_sessionUuid), dataEc.message());
				self->TcpAsyncReadSize();
				return;
			}

			RpcPacket deserializeRpcPacket;
			if (!deserializeRpcPacket.ParseFromArray(dataBuffer->data(), static_cast<int>(dataBuffer->size())))
			{
				spdlog::error("{} : error parsing rpc packet", to_string(self->_sessionUuid));
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
	const auto buf = std::make_shared<std::string>();
	buf->resize(MAX_PACKET_SIZE);

	const auto senderEndpoint = std::make_shared<udp::endpoint>();

	_udpSocketPtr->async_receive_from(
		boost::asio::buffer(*buf), *senderEndpoint,
		_rpcCtxManager->GetStrand().wrap([self(shared_from_this()), buf](const boost::system::error_code& ec, const std::size_t bytesRead) {
			if (ec)
			{
				if (ec == boost::asio::error::eof || ec == boost::asio::error::operation_aborted)
				{
					spdlog::info("UDP receive operation aborted or EOF");
					return;
				}

				spdlog::error("{} : UDP read error ({})", to_string(self->_sessionUuid), ec.message());
				self->UdpAsyncRead();
				return;
			}

			if (bytesRead < sizeof(std::uint16_t))
			{
				spdlog::error("{} : invalid packet received (bytes read under 2bytes)", to_string(self->_sessionUuid));
				self->UdpAsyncRead();
				return;
			}

			if (buf->data() == nullptr)
			{
				spdlog::error("{} : invalid packet received (buffer->data() is null)", to_string(self->_sessionUuid));
				self->UdpAsyncRead();
				return;
			}

			// payload size
			std::uint16_t payloadSize = 0;
			std::memcpy(&payloadSize, buf->data(), sizeof(std::uint16_t)); // Access Violation reading location exception
			payloadSize = ntohs(payloadSize);

			if (payloadSize == 0 || payloadSize + sizeof(std::uint16_t) > bytesRead)
			{
				spdlog::error("{} : invalid payload size (payloadSize is 0 or over bytesRead)", to_string(self->_sessionUuid));
				self->UdpAsyncRead();
				return;
			}

			RpcPacket packet;
			if (!packet.ParseFromArray(buf->data() + sizeof(std::uint16_t), payloadSize))
			{
				spdlog::error("{} : error parsing rpc packet", to_string(self->_sessionUuid));
				self->UdpAsyncRead();
				return;
			}

			self->UdpAsyncRead();

			// Input Send to Lockstep Group
			if (self->_inputAction)
			{
				const auto rpcRequest =
					std::make_shared<std::pair<uuid, std::shared_ptr<RpcPacket>>>(self->_sessionUuid, std::make_shared<RpcPacket>(packet));
				self->_inputAction(rpcRequest);
			}}
		)
	);
}

void Session::UdpAsyncWrite(const std::shared_ptr<std::string>& data)
{
	// make udp packet
	const std::uint16_t payloadSize = static_cast<std::uint16_t>(data->size());
	const std::uint16_t netPayloadSize = htons(payloadSize);

	const auto payload = std::make_shared<std::string>();
	payload->append(reinterpret_cast<const char*>(&netPayloadSize), sizeof(netPayloadSize));
	payload->append(*data);

	// Send packet to client
	_udpSocketPtr->async_send_to(boost::asio::buffer(*payload), _udpSendEp,
		_rpcCtxManager->GetStrand().wrap([self(shared_from_this()), payload](const boost::system::error_code& ec, std::size_t) {
			if (ec)
			{
				spdlog::error("{} : UDP error sending data ({})", to_string(self->_sessionUuid), ec.message());
				return;
			}

			std::string addressStr = self->_udpSendEp.address().to_string();
			std::string portStr = std::to_string(self->_udpSendEp.port());
			// spdlog::info("{} : send packet to {} {}", to_string(_sessionUuid), addressStr, portStr);
			}
		)
	);
}