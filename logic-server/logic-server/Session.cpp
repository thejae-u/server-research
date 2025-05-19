#include "Session.h"
#include "Server.h"
#include <iostream>

#include "LockstepGroup.h"
#include "Utility.h"

Session::Session(IoContext::strand& strand, std::shared_ptr<Server> serverPtr, const uuid guid)
	: _serverPtr(std::move(serverPtr)),
		_strand(strand),
		_tcpSocketPtr(std::make_shared<TcpSocket>(strand.context())),
		_udpSocketPtr(std::make_shared<UdpSocket>(strand.context(), udp::endpoint(udp::v4(), 0))),
		_receiveNetSize(0),
		_receiveDataSize(0),
		_sessionUuid(guid)
{
}

void Session::Start()
{
	std::cout << "session " << _sessionUuid << " started\n";
	
	// Start reading data
	boost::asio::post(_strand.wrap([this]() { UdpAsyncReadSize(); }));
	boost::asio::post(_strand.wrap([this]() { TcpAsyncReadSize(); }));
}

bool Session::SendUdpPort() const
{
	const unsigned short udpPort = _udpSocketPtr->local_endpoint().port();
	
	RpcPacket packet;
	packet.set_method(UDP_PORT);
	packet.set_data(std::to_string(udpPort));

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
	packet.set_data("");

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
	boost::asio::post(_strand.wrap([this]()
	{
		_lockstepGroupPtr->RemoveMember(shared_from_this());
		
		_tcpSocketPtr->close();
		_udpSocketPtr->close();
		std::cout << _sessionUuid << " session stopped\n";
	}));
}

void Session::RpcProcess(RpcPacket packet)
{
	auto serializedData = std::make_shared<std::string>();
	if (!packet.SerializeToString(serializedData.get()))
	{
		std::cerr << "error serializing data\n";
		return;
	}

	// Send the size of the serialized data
	const uint32_t sendNetSize =  static_cast<uint32_t>(serializedData->size());
	const uint32_t sendDataSize = htonl(sendNetSize);

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

void Session::TcpAsyncReadSize()
{
	auto self(shared_from_this());
	boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(&_receiveNetSize, sizeof(_receiveNetSize)),
		_strand.wrap([this, self](const boost::system::error_code& sizeEc, std::size_t)
		{
			if (sizeEc)
			{
				if (sizeEc == boost::asio::error::eof || sizeEc == boost::asio::error::connection_reset)
				{
					Stop();
					return;
				}
				
				std::cerr << "error Receiving Size: " << sizeEc.message() << "\n";
				TcpAsyncReadSize();
				return;
			}

			// Convert network byte order to host byte order
			_receiveNetSize = ntohl(_receiveNetSize);
			_receiveDataSize = _receiveNetSize;

			if (_receiveBuffer.size() < _receiveNetSize)
				_receiveBuffer.resize(_receiveDataSize, 0);

			boost::asio::post(_strand.wrap([this]() { TcpAsyncReadData(); }));
		}));
}

void Session::TcpAsyncReadData()
{
	auto self(shared_from_this());
	boost::asio::async_read(*_tcpSocketPtr, boost::asio::buffer(_receiveBuffer),
		_strand.wrap([this, self](const boost::system::error_code& dataEc, std::size_t)
		{
			if (dataEc)
			{
				if (dataEc == boost::asio::error::eof || dataEc == boost::asio::error::connection_reset)
				{
					Stop();
					return;
				}

				std::cerr << "error Receiving Data: " << dataEc.message() << "\n";
				TcpAsyncReadSize();

				return;
			}

			RpcPacket deserializeRpcPacket;
			if (!deserializeRpcPacket.ParseFromArray(_receiveBuffer.data(), static_cast<int>(_receiveDataSize)))
			{
				std::cerr << "in ReadData Error Parsing Data: " << _receiveBuffer.data() << "\n";
				Stop();
				return;
			}

			//std::cout << "Received Data: " << to_string(Utility::BytesToUuid(deserializeRpcPacket.uuid())) << " " << deserializeRpcPacket.method() << "\n";

			RpcPacket request;
			request.set_uuid(deserializeRpcPacket.uuid());
			request.set_method(deserializeRpcPacket.method());
			request.set_data(deserializeRpcPacket.data());
			// Process the RPC request
			if (_lockstepGroupPtr)
			{
				_lockstepGroupPtr->CollectInput({{_sessionUuid, std::make_shared<RpcPacket>(request)}});
			}

			_receiveBuffer.clear();
			_receiveNetSize = 0;
			_receiveDataSize = 0;
			boost::asio::post(_strand.wrap([this]() { TcpAsyncReadSize(); }));
		}));
}

void Session::UdpAsyncReadSize()
{
	std::uint32_t netSize = 0;
	_udpSocketPtr->async_receive(boost::asio::buffer(&netSize, sizeof(netSize)), 
		_strand.wrap([this, netSize](const boost::system::error_code& sizeEc, std::size_t)
		{
			if (sizeEc)
			{
				std::cerr << "error Receiving Size: " << sizeEc.message() << "\n";
				return;
			}

			// Convert network byte order to host byte order
			const std::uint32_t dataSize = ntohl(netSize);
			if (dataSize >= MAX_PACKET_SIZE)
			{
				std::cerr << "too large packet size: " << dataSize << "\n";
				UdpAsyncReadSize();
				return;
			}

			const auto dataBuffer = std::make_shared<std::vector<char>>(dataSize, 0);
			UdpAsyncReadData(std::move(dataBuffer));
		}));
}

void Session::UdpAsyncReadData(std::shared_ptr<std::vector<char>> dataBuffer)
{
	std::size_t receiveDataSize = dataBuffer->size();
	
	_udpSocketPtr->async_receive(boost::asio::buffer(*dataBuffer),
		_strand.wrap([this, dataBuffer, receiveDataSize](const boost::system::error_code& dataEc, std::size_t)
		{
			if (dataEc)
			{
				std::cerr << "error Receiving Data: " << dataEc.message() << "\n";
				return;
			}

			RpcPacket deserializeRpcPacket;
			if (!deserializeRpcPacket.ParseFromArray(dataBuffer->data(), static_cast<int>(receiveDataSize)))
			{
				std::cerr << "in ReadData Error Parsing Data: " << _receiveBuffer.data() << "\n";
				return;
			}

			auto packetPtr = std::make_shared<RpcPacket>(deserializeRpcPacket);
			if (_lockstepGroupPtr)
			{
				_lockstepGroupPtr->CollectInput({{_sessionUuid, std::move(packetPtr)}});
			}
			
			UdpAsyncReadSize();
		}));
}
