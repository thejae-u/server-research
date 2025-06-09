#include "Session.h"
#include "Server.h"
#include <iostream>

#include "LockstepGroup.h"
#include "Utility.h"

Session::Session(const IoContext::strand& strand, const IoContext::strand& rpcStrand, const uuid guid)
	: _strand(strand), _rpcStrand(rpcStrand),
	_tcpSocketPtr(std::make_shared<TcpSocket>(strand.context())),
	_udpSocketPtr(std::make_shared<UdpSocket>(rpcStrand.context(), udp::endpoint(udp::v4(), 0))),
	_sessionUuid(guid),
	_lastRtt(0)
{
	_pingTimer = std::make_shared<Scheduler>(_strand, std::chrono::milliseconds(_pingDelay), [this]() {
		SendPingPacket();
	});
}

void Session::Start()
{
	SPDLOG_INFO("Session {} started", to_string(_sessionUuid));

	// Async Functions Start
	TcpAsyncReadSize();
	_pingTimer->Start();
	UdpAsyncRead();

	_isConnected = true;
}

void Session::Stop()
{
	SPDLOG_INFO("{} session stopped", to_string(_sessionUuid));
	_isConnected = false;

	_tcpSocketPtr->close();
	_udpSocketPtr->close();

	_pingTimer->Stop();
	_onStopCallback(shared_from_this());
}

bool Session::ExchangeUdpPort()
{
	const std::uint16_t udpPort = _udpSocketPtr->local_endpoint().port();
	std::string udpPortString = std::to_string(udpPort);

	SPDLOG_INFO("UDP Port {} for Session {}", udpPortString, to_string(_sessionUuid));
	
	RpcPacket packet;
	packet.set_method(UDP_PORT);
	packet.set_data(udpPortString);

	std::string udpPortPacket;
	if (!packet.SerializeToString(&udpPortPacket))
	{
		SPDLOG_ERROR("{} {} : failed to serialize UDP port packet", __func__, to_string(_sessionUuid));
		return false;
	}

	const auto sendDataSize = static_cast<uint32_t>(udpPortPacket.size());
	auto sendNetSize = htonl(sendDataSize);

	std::vector<char> receiveBuffer;
	uint32_t receiveNetSize;

	try
	{
		boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)));
		boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(udpPortPacket));

		boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(&receiveNetSize, sizeof(uint32_t)));
		const auto receiveDataSize = ntohl(receiveNetSize);
		receiveBuffer.resize(receiveDataSize);
		boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(receiveBuffer));

		RpcPacket receivePacket;
		if (!receivePacket.ParseFromArray(receiveBuffer.data(), static_cast<int>(receiveDataSize)))
		{
			SPDLOG_ERROR("{} {} : failed to parse UDP port packet", __func__, to_string(_sessionUuid));
			return false;
		}

		if (receivePacket.method() != UDP_PORT)
			return false;

		// Big endian to little endian
		std::uint16_t clientPort;
		std::memcpy(&clientPort, receivePacket.data().data(), sizeof(clientPort));
		
		// Set the UDP endpoint
		_udpSendEp = udp::endpoint(_tcpSocketPtr->remote_endpoint().address(), clientPort);
	}
	catch (const std::exception& e)
	{
		SPDLOG_ERROR("{} {} : exception ({})", __func__, to_string(_sessionUuid), e.what());
		return false;
	}

	return true;
}

bool Session::SendUuidToClient() const
{
	// Send uuid to client
	RpcPacket packet;
	packet.set_uuid(Utility::GuidToBytes(_sessionUuid));
	packet.set_method(UUID);

	auto serializedGuid = packet.SerializeAsString();
	const uint32_t sendNetSize =  static_cast<uint32_t>(serializedGuid.size());
	const uint32_t sendDataSize = htonl(sendNetSize);

	try
	{
		boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendDataSize, sizeof(sendDataSize)));
		boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(serializedGuid));
	}
	catch (const std::exception& e)
	{
		SPDLOG_ERROR("{} {} : exception ({})", __func__, to_string(_sessionUuid), e.what());
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

void Session::ProcessRpc(std::unordered_map<SSessionKey, std::shared_ptr<RpcPacket>> allInputs)
{
	std::unordered_map<SSessionKey, RpcPacket> copiedInputs;
	for (const auto& [key, packet] : allInputs)
	{
		copiedInputs[key] = *packet;
	}
	
	for (const auto& [key, packet] : copiedInputs)
	{
		const auto serializedData = std::make_shared<std::string>();
		if (!packet.SerializeToString(serializedData.get()))
		{
			SPDLOG_ERROR("{} {} : RpcPacket serialize failed for key {}", __func__, to_string(_sessionUuid), to_string(key.guid));
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
	const uint32_t sendDataSize =  static_cast<uint32_t>(serializePingPacket->size());
	const uint32_t sendNetSize = htonl(sendDataSize);


	const auto dbgName = std::make_shared<std::string>(__func__);
	_pingTime = Utility::StartStopwatch();
	
	boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)));
	boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(*serializePingPacket));
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
		SPDLOG_ERROR("{} : Invalid packet method ({})", to_string(_sessionUuid), Utility::MethodToString(packet->method()));
	}
}

void Session::TcpAsyncWrite(const std::shared_ptr<std::string>& data)
{
	auto self(shared_from_this());

	const std::uint32_t dataSize = static_cast<std::uint32_t>(data->size());
	const std::uint32_t netSize = htonl(dataSize);
	
	boost::asio::async_write(*_tcpSocketPtr, boost::asio::buffer(&netSize, sizeof(netSize)),
		_strand.wrap([self, data](const boost::system::error_code& sizeEc, std::size_t)
		{
			if (sizeEc)
			{
				SPDLOG_ERROR("TCP Error Sending Size to {}: {}", to_string(self->_sessionUuid), sizeEc.message());
				return;
			}

			boost::asio::async_write(*self->_tcpSocketPtr, boost::asio::buffer(*data),
				self->_strand.wrap([self, data](const boost::system::error_code& dataEc, std::size_t)
				{
					if (dataEc)
					{
						SPDLOG_ERROR("TCP Error Sending Data to {}: {}", to_string(self->_sessionUuid), dataEc.message());
						return;
					}
				}));
		}));
}

void Session::TcpAsyncReadSize()
{
	_tcpDataSize = 0;
	_tcpNetSize = 0;

	const auto dbgName = std::make_shared<std::string>(__func__);
	boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(&_tcpNetSize, sizeof(_tcpNetSize)),
		_strand.wrap([this, dbgName](const boost::system::error_code& sizeEc, std::size_t)
		{
			if (sizeEc)
			{
				if (sizeEc == boost::asio::error::eof
					|| sizeEc == boost::asio::error::connection_reset
					|| sizeEc == boost::asio::error::connection_aborted
					|| sizeEc == boost::asio::error::operation_aborted)
				{
					SPDLOG_INFO("Session {} : TcpAsyncRead aborted", to_string(_sessionUuid));
					Stop();
					return;
				}
				
				SPDLOG_ERROR("{} {} : TCP error Receiving Size ({})", *dbgName, to_string(_sessionUuid), sizeEc.message());
				return;
			}

			// Convert network byte order to host byte order
			_tcpDataSize = ntohl(_tcpNetSize);

			if (_tcpDataSize > MAX_PACKET_SIZE)
			{
				SPDLOG_ERROR("{} {} : TCP error Receiving Data Size ({})", *dbgName, to_string(_sessionUuid), _tcpDataSize);
				TcpAsyncReadSize();
				return;
			}
			
			const auto dataBuffer = std::make_shared<std::vector<char>>(_tcpDataSize, 0);

			TcpAsyncReadData(dataBuffer);
		}));
}

void Session::TcpAsyncReadData(const std::shared_ptr<std::vector<char>>& dataBuffer)
{
	const auto dbgName = std::make_shared<std::string>(__func__);
	boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(*dataBuffer),
		_strand.wrap([this, dataBuffer, dbgName](const boost::system::error_code& dataEc, std::size_t)
		{
			if (dataEc)
			{
				if (dataEc == boost::asio::error::eof || dataEc == boost::asio::error::connection_reset)
				{
					Stop();
					return;
				}

				SPDLOG_ERROR("{} {} : TCP error Receiving Data ({})", *dbgName, to_string(_sessionUuid), dataEc.message());
				TcpAsyncReadSize();
				return;
			}

			RpcPacket deserializeRpcPacket;
			if (!deserializeRpcPacket.ParseFromArray(dataBuffer->data(), static_cast<int>(dataBuffer->size())))
			{
				SPDLOG_ERROR("{} {} : Error Parsing RpcPacket", *dbgName, to_string(_sessionUuid));
				TcpAsyncReadSize();
				return;
			}

			const auto packetPtr = std::make_shared<RpcPacket>(deserializeRpcPacket);
			TcpAsyncReadSize();
			ProcessTcpRequest(packetPtr);
		}));
}

void Session::UdpAsyncRead()
{
	if (!_udpSocketPtr || !_udpSocketPtr->is_open())
		return;

	auto self(shared_from_this());
	
	// max size buffer for udp packet
	const auto buf = std::make_shared<std::string>();
	buf->resize(MAX_PACKET_SIZE);
	
    const auto senderEndpoint = std::make_shared<udp::endpoint>();
	const auto dbgName = std::make_shared<std::string>(__func__);

    _udpSocketPtr->async_receive_from(
        boost::asio::buffer(*buf), *senderEndpoint,
        _rpcStrand.wrap([self, buf, dbgName](const boost::system::error_code& ec, const std::size_t bytesRead)
        {
            if (ec)
            {
            	if (ec == boost::asio::error::eof || ec == boost::asio::error::operation_aborted)
            	{
            		SPDLOG_INFO("UDP receive operation aborted or EOF");
            		return;
            	}

            	self->UdpAsyncRead();
                return;
            }

            if (bytesRead < sizeof(std::uint16_t))
            {
            	SPDLOG_ERROR("{} {} : Invalid packet received (bytes read under 2bytes)", *dbgName, to_string(self->_sessionUuid));
            	self->UdpAsyncRead();
                return;
            }

        	if (buf->data() == nullptr)
        	{
        		SPDLOG_ERROR("{} {} : Invalid packet received (buffer->data() is null)", *dbgName, to_string(self->_sessionUuid));
        		self->UdpAsyncRead();
        		return;
        	}

            // payload size
            std::uint16_t payloadSize = 0;
            std::memcpy(&payloadSize, buf->data(), sizeof(std::uint16_t)); // Access Violation reading location exception 
            payloadSize = ntohs(payloadSize);

            if (payloadSize == 0 || payloadSize + sizeof(std::uint16_t) > bytesRead)
            {
            	SPDLOG_ERROR("{} {} : Invalid payload size (payloadSize is 0 or over bytesRead)", *dbgName, to_string(self->_sessionUuid));
                self->UdpAsyncRead();
                return;
            }

            RpcPacket packet;
            if (!packet.ParseFromArray(buf->data() + sizeof(std::uint16_t), payloadSize))
            {
            	SPDLOG_ERROR("{} {} : Error Parsing RpcPacket", *dbgName, to_string(self->_sessionUuid));
                self->UdpAsyncRead();
                return;
            }

        	self->UdpAsyncRead();

        	if (self->_inputAction)
        	{
				const auto rpcRequest =
					std::make_shared<std::pair<uuid, std::shared_ptr<RpcPacket>>>(self->_sessionUuid, std::make_shared<RpcPacket>(packet));
				self->_inputAction(rpcRequest);
			}
        }));
}

void Session::UdpAsyncWrite(const std::shared_ptr<std::string>& data)
{
	// make udp packet
	const std::uint16_t payloadSize = static_cast<std::uint16_t>(data->size());
	const std::uint16_t netPayloadSize = htons(payloadSize);
	const auto payload = std::make_shared<std::string>();
	payload->append(reinterpret_cast<const char*>(&netPayloadSize), sizeof(netPayloadSize));
	payload->append(*data);

	const auto dbgName = std::make_shared<std::string>(__func__);
	
	_udpSocketPtr->async_send_to(boost::asio::buffer(*payload), _udpSendEp,
		_rpcStrand.wrap([this, payload, dbgName](const boost::system::error_code& ec, std::size_t)
		{
			if (ec)
			{
				SPDLOG_ERROR("{} {} : UDP error Sending Data ({})", *dbgName, to_string(_sessionUuid), ec.message());
				return;
			}

			std::string addressStr = _udpSendEp.address().to_string();
			std::string portStr = std::to_string(_udpSendEp.port());
			// SPDLOG_INFO("{} {} : send packet to {} {}", *dbgName, to_string(_sessionUuid), addressStr, portStr);
		}));
}
