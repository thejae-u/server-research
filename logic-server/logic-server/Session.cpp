#include "Session.h"
#include "Server.h"
#include <iostream>

#include "LockstepGroup.h"
#include "Utility.h"

#define INVALID_RTT -1

Session::Session(IoContext::strand& strand, std::shared_ptr<Server> serverPtr, boost::uuids::uuid guid)
	: _serverPtr(std::move(serverPtr)), _strand(strand),
		_socketPtr(std::make_shared<tcp::socket>(strand.context())), _receiveNetSize(0), _receiveDataSize(0),
		_sessionUuid(guid)
{
}

void Session::Start()
{
	std::cout << "Session " << _sessionUuid << " started\n";
	// Start reading data
	boost::asio::post(_strand.wrap([this]() { AsyncReadSize(); }));
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
		auto stopwatch = Utility::StartStopwatch();
		boost::asio::write(*_socketPtr, boost::asio::buffer(&sendNetSize, sizeof(sendNetSize)));
		boost::asio::write(*_socketPtr, boost::asio::buffer(serializeRttPacket));
		
		boost::asio::read(*_socketPtr, boost::asio::buffer(&receiveNetSize, sizeof(receiveNetSize)));
		const auto receiveDataSize = ntohl(receiveNetSize);
		std::vector<char> receiveBuffer(receiveDataSize, 0);
		boost::asio::read(*_socketPtr, boost::asio::buffer(receiveBuffer));
		rtt = Utility::StopStopwatch(stopwatch);
		
		RpcPacket deserializeRpcPacket;
		if (!deserializeRpcPacket.ParseFromArray(receiveBuffer.data(), static_cast<int>(receiveDataSize)))
		{
			std::cerr << "failed to parse rtt packet\n";
			rtt = INVALID_RTT;
		}

		if (deserializeRpcPacket.method() != PONG)
		{
			std::cerr << "Invalid packet type: " << Utility::MethodToString(deserializeRpcPacket.method()) << "\n";
			rtt = INVALID_RTT;
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "RTT get error: " << e.what() << "\n";
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
		boost::asio::write(*_socketPtr, boost::asio::buffer(&sendDataSize, sizeof(sendDataSize)));
		boost::asio::write(*_socketPtr, boost::asio::buffer(serializedGuid));
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
	}));
}

void Session::RpcProcess(RpcPacket packet)
{
	auto serializedData = std::make_shared<std::string>();
	if (!packet.SerializeToString(serializedData.get()))
	{
		std::cerr << "Error Serializing Data\n";
		return;
	}

	// Send the size of the serialized data
	const uint32_t sendNetSize =  static_cast<uint32_t>(serializedData->size());
	const uint32_t sendDataSize = htonl(sendNetSize);

	boost::asio::async_write(*_socketPtr, boost::asio::buffer(&sendDataSize, sizeof(sendDataSize)),
		_strand.wrap([this, serializedData] (const boost::system::error_code& sizeEc, std::size_t)
			{
				if (sizeEc)
				{
					std::cerr << "Error Sending Size: " << sizeEc.message() << "\n";
					return;
				}

				// Send the serialized data
				boost::asio::async_write(*_socketPtr, boost::asio::buffer(*serializedData),
				_strand.wrap([this, serializedData](const boost::system::error_code& dataEc, std::size_t)
					{
						if (dataEc)
						{
							std::cerr << "Error Sending Data: " << dataEc.message() << "\n";
						}
					}));
			}));
}

void Session::AsyncReadSize()
{
	auto self(shared_from_this());
	boost::asio::async_read(*_socketPtr, boost::asio::buffer(&_receiveNetSize, sizeof(_receiveNetSize)),
		_strand.wrap([this, self](const boost::system::error_code& sizeEc, std::size_t)
		{
			if (sizeEc)
			{
				if (sizeEc == boost::asio::error::eof || sizeEc == boost::asio::error::connection_reset)
				{
					Stop();
					return;
				}
				
				std::cerr << "Error Receiving Size: " << sizeEc.message() << "\n";
				AsyncReadSize();
				return;
			}

			// Convert network byte order to host byte order
			_receiveNetSize = ntohl(_receiveNetSize);
			_receiveDataSize = _receiveNetSize;

			if (_receiveBuffer.size() < _receiveNetSize)
				_receiveBuffer.resize(_receiveDataSize, 0);

			boost::asio::post(_strand.wrap([this]() { AsyncReadData(); }));
		}));
}

void Session::AsyncReadData()
{
	auto self(shared_from_this());
	boost::asio::async_read(*_socketPtr, boost::asio::buffer(_receiveBuffer),
		_strand.wrap([this, self](const boost::system::error_code& dataEc, std::size_t)
		{
			if (dataEc)
			{
				if (dataEc == boost::asio::error::eof || dataEc == boost::asio::error::connection_reset)
				{
					Stop();
					return;
				}

				std::cerr << "Error Receiving Data: " << dataEc.message() << "\n";
				AsyncReadSize();

				return;
			}

			RpcPacket deserializeRpcPacket;
			if (!deserializeRpcPacket.ParseFromArray(_receiveBuffer.data(), static_cast<int>(_receiveDataSize)))
			{
				std::cerr << "In ReadData Error Parsing Data: " << _receiveBuffer.data() << "\n";
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
			boost::asio::post(_strand.wrap([this]() { AsyncReadSize(); }));
		}));
}