#include "Session.h"
#include "Server.h"
#include <iostream>

#include "LockstepGroup.h"
#include "Utility.h"

Session::Session(const IoContext::strand& strand, const uuid guid)
	: _strand(strand),
	  _tcpSocketPtr(std::make_shared<TcpSocket>(strand.context())),
	  _udpSocketPtr(std::make_shared<UdpSocket>(strand.context(), udp::endpoint(udp::v4(), 0))),
	  _sessionUuid(guid),
	  _lastRtt(0),
	  _pingTimer(strand.context())
{
}

void Session::Start()
{
	std::cout << "session " << _sessionUuid << " started\n";
	
	// Start reading data
	boost::asio::post(_strand.wrap([this]() { UdpAsyncReadBufferHeader(); }));
	boost::asio::post(_strand.wrap([this]() { TcpAsyncReadSize(); }));
}

bool Session::SendUdpPort() const
{
	const std::uint16_t udpPort = _udpSocketPtr->local_endpoint().port();
	std::string udpPortString = std::to_string(udpPort);
	
	RpcPacket packet;
	packet.set_method(UDP_PORT);
	packet.set_data(udpPortString);
	std::cout << "serialized udp port: " << packet.data() << "\n";

	std::string udpPortPacket;
	if (!packet.SerializeToString(&udpPortPacket))
	{
		std::cerr << "error serializing data\n";
		return false;
	}

	const auto sendDataSize = static_cast<uint32_t>(udpPortPacket.size());
	auto sendNetSize = htonl(sendDataSize);

	try
	{
		boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)));
		boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(udpPortPacket));
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << "\n";
		return false;
	}

	return true;
}


std::int64_t Session::CheckAndGetRtt() const
{
	RpcPacket packet;
	packet.set_method(PING);

	auto serializeRttPacket = packet.SerializeAsString();
	const uint32_t sendDataSize =  static_cast<uint32_t>(serializeRttPacket.size());
	const uint32_t sendNetSize = htonl(sendDataSize);
	uint32_t receiveNetSize = 0;
	std::int64_t rtt = INVALID_RTT;

	try
	{
		const auto stopwatch = Utility::StartStopwatch();
		boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)));
		boost::asio::write(*_tcpSocketPtr, boost::asio::buffer(serializeRttPacket));
		
		boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(&receiveNetSize, sizeof(receiveNetSize)));
		const auto receiveDataSize = ntohl(receiveNetSize);
		std::vector<char> receiveBuffer(receiveDataSize, 0);
		boost::asio::read(*_tcpSocketPtr, boost::asio::buffer(receiveBuffer));
		rtt = Utility::StopStopwatch(stopwatch);
		
		RpcPacket deserializeRpcPacket;
		if (!deserializeRpcPacket.ParseFromArray(receiveBuffer.data(), static_cast<int>(receiveDataSize)))
		{
			std::cerr << "failed to parse rtt packet\n";
			rtt = INVALID_RTT;
		}

		if (deserializeRpcPacket.method() != PONG)
		{
			std::cerr << "invalid packet type: " << Utility::MethodToString(deserializeRpcPacket.method()) << "\n";
			rtt = INVALID_RTT;
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "rtt get error: " << e.what() << "\n";
	}

	return rtt;
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
		std::cerr << e.what() << "\n";
		return false;
	}

	return true;
}

void Session::Stop()
{
	std::cout << to_string(_sessionUuid) << " session stopped\n";
	_onStopCallback(shared_from_this());
}

void Session::SetStopCallback(StopCallback stopCallback)
{
	_onStopCallback = std::move(stopCallback);
}

void Session::RpcProcess(RpcPacket packet)
{
	auto serializedData = std::make_shared<std::string>(packet.SerializeAsString());
	if (!serializedData)
	{
		std::cerr << "RpcProcess error: serialize failed\n";
		return;
	}

	// Send the size of the serialized data
	const uint32_t sendNetSize =  static_cast<uint32_t>(serializedData->size());
	const uint32_t sendDataSize = htonl(sendNetSize);

	std::cout << "send Packet " << Utility::MethodToString(packet.method()) << "\n";
	
	boost::asio::async_write(*_tcpSocketPtr, boost::asio::buffer(&sendDataSize, sizeof(sendDataSize)),
		_strand.wrap([this, serializedData] (const boost::system::error_code& sizeEc, std::size_t)
			{
				if (sizeEc)
				{
					std::cerr << "error Sending Size: " << sizeEc.message() << "\n";
					return;
				}

				// Send the serialized data
				boost::asio::async_write(*_tcpSocketPtr, boost::asio::buffer(*serializedData),
				_strand.wrap([this, serializedData](const boost::system::error_code& dataEc, std::size_t)
					{
						if (dataEc)
						{
							std::cerr << "error Sending Data: " << dataEc.message() << "\n";
						}
					}));
			}));
}

void Session::SchedulePingTimer()
{
	_pingTimer.expires_after(std::chrono::milliseconds(1000));
	_pingTimer.async_wait(
		_strand.wrap([this](const boost::system::error_code& ec)
			{
				if (ec)
				{
					std::cerr << "Ping Timer error: " << ec.message() << "\n";
					return;
				}

				AsyncSendPingPacket();
			}));
}

void Session::AsyncSendPingPacket()
{
	{
		std::lock_guard<std::mutex> lock(_stopMutex);
		if (_onStopCallback == nullptr)
			return;
	}

	RpcPacket packet;
	packet.set_method(PING);
	auto serializePingPacket = std::make_shared<std::string>(packet.SerializeAsString());
	const uint32_t sendDataSize =  static_cast<uint32_t>(serializePingPacket->size());
	const uint32_t sendNetSize = htonl(sendDataSize);
	
	_lastSendTime = Utility::StartStopwatch();
	boost::asio::async_write(*_tcpSocketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)),
		_strand.wrap([this, serializePingPacket](const boost::system::error_code& sizeEc, std::size_t)
			{
				if (sizeEc)
				{
					std::cerr << "error Sending Size: " << sizeEc.message() << "\n";
					return;
				}

				boost::asio::async_write(*_tcpSocketPtr, boost::asio::buffer(*serializePingPacket),
					_strand.wrap([this](const boost::system::error_code& dataEc, std::size_t)
						{
							if (dataEc)
							{
								std::cerr << "error Sending Data: " << dataEc.message() << "\n";
							}

						SchedulePingTimer();
						}));
			}));
}

void Session::ProcessTcpRequest(const std::shared_ptr<RpcPacket>& packet)
{
	if (packet->method() == PONG)
	{
		const auto rtt = Utility::StopStopwatch(_lastSendTime);
		_lastRtt = rtt;
		std::cout << to_string(_sessionUuid) << " rtt: " << rtt << "\n";
	}
	else
	{
		std::cerr << "not tcp packet type: " << Utility::MethodToString(packet->method()) << "\n";
	}
}

void Session::TcpAsyncReadSize()
{
	std::uint32_t netSize = 0;
	boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(&netSize, sizeof(netSize)),
		_strand.wrap([this, netSize](const boost::system::error_code& sizeEc, std::size_t)
		{
			if (sizeEc)
			{
				if (sizeEc != boost::asio::error::eof && sizeEc != boost::asio::error::connection_reset)
				{
					std::cerr << "TCP error Receiving Size: " << sizeEc.message() << "\n";
				}
				
				Stop();
				return;
			}

			// Convert network byte order to host byte order
			std::uint32_t dataSize = ntohl(netSize);
			const auto dataBuffer = std::make_shared<std::vector<char>>(dataSize, 0);

			TcpAsyncReadData(dataBuffer);
		}));
}

void Session::TcpAsyncReadData(const std::shared_ptr<std::vector<char>>& dataBuffer)
{
	boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(*dataBuffer),
		_strand.wrap([this, dataBuffer](const boost::system::error_code& dataEc, std::size_t)
		{
			if (dataEc)
			{
				if (dataEc == boost::asio::error::eof || dataEc == boost::asio::error::connection_reset)
				{
					Stop();
					return;
				}

				std::cerr << "TCP error Receiving Data: " << dataEc.message() << "\n";
				TcpAsyncReadSize();
				return;
			}

			RpcPacket deserializeRpcPacket;
			if (!deserializeRpcPacket.ParseFromArray(dataBuffer->data(), static_cast<int>(dataBuffer->size())))
			{
				std::cerr << "in ReadData Error Parsing Data: " << dataBuffer->data() << "\n";
				TcpAsyncReadSize();
				return;
			}

			const auto packetPtr = std::make_shared<RpcPacket>(deserializeRpcPacket);
			boost::asio::post([this, packetPtr]() { ProcessTcpRequest(packetPtr); });
			
			TcpAsyncReadSize();
		}));
}

void Session::UdpAsyncReadBufferHeader()
{
    auto headerBuffer = std::make_shared<std::array<char, sizeof(std::uint32_t)>>();
    _udpSocketPtr->async_receive(boost::asio::buffer(*headerBuffer),
        _strand.wrap([this, headerBuffer](const boost::system::error_code& ec, const std::size_t bytesTransferred)
        {
            if (ec)
            {
                return;
            }

            if (bytesTransferred != sizeof(std::uint32_t))
            {
                std::cerr << "Invalid size header received\n";
                UdpAsyncReadBufferHeader();
                return;
            }

            std::uint32_t dataSize = ntohl(*reinterpret_cast<std::uint32_t*>(headerBuffer->data()));
            if (dataSize >= MAX_PACKET_SIZE)
            {
                std::cerr << "Too large packet size: " << dataSize << "\n";
                UdpAsyncReadBufferHeader();
                return;
            }

            const auto dataBuffer = std::make_shared<std::vector<char>>(dataSize);
            UdpAsyncReadData(dataBuffer);
        }));
}

void Session::UdpAsyncReadData(const std::shared_ptr<std::vector<char>>& dataBuffer)
{
    _udpSocketPtr->async_receive(boost::asio::buffer(*dataBuffer),
        _strand.wrap([this, dataBuffer](const boost::system::error_code& ec, const std::size_t bytesTransferred)
        {
            if (ec)
            {
                if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset)
                {
                    return;
                }

                UdpAsyncReadBufferHeader();
                return;
            }

            if (bytesTransferred != dataBuffer->size())
            {
                std::cerr << "Incomplete data received\n";
                UdpAsyncReadBufferHeader();
                return;
            }

            // Process the received data
            RpcPacket packet;
            if (!packet.ParseFromArray(dataBuffer->data(), static_cast<int>(dataBuffer->size())))
            {
                std::cerr << "Error parsing data\n";
                UdpAsyncReadBufferHeader();
                return;
            }

            if (_lockstepGroupPtr)
            {
                _lockstepGroupPtr->CollectInput({{_sessionUuid, std::make_shared<RpcPacket>(packet)}});
            }

            UdpAsyncReadBufferHeader();
        }));
}