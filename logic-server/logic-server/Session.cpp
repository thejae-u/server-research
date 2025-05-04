#include "Session.h"
#include "Server.h"
#include <iostream>

#include "LockstepGroup.h"
#include "Utility.h"

Session::Session(io_context::strand& strand, std::shared_ptr<Server> serverPtr, boost::uuids::uuid guid)
	: _serverPtr(std::move(serverPtr)), _strand(strand),
		_socketPtr(std::make_shared<tcp::socket>(strand.context())), _receiveNetSize(0), _receiveDataSize(0),
		_sessionGuid(guid)
{
	std::cout << "Session: " << _serverPtr->GetSessionCount() << "\n";
}

void Session::Start()
{
	std::cout << "Session Started: " << _socketPtr->remote_endpoint().address() << "\n";
	boost::asio::post(_strand.wrap([this]() { AsyncReadSize(); }));
}

void Session::Stop()
{
	boost::asio::post(_strand.wrap([this]() { _serverPtr->DisconnectSession(shared_from_this()); }));
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
				std::cerr << "Error Receiving Size: " << sizeEc.message() << "\n";
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

			std::cout << "Received Data: " << deserializeRpcPacket.guid() << " " << deserializeRpcPacket.method() << "\n";

			RpcRequest rpcRequest;
			rpcRequest.set_guid(deserializeRpcPacket.guid());
			rpcRequest.set_method(deserializeRpcPacket.method());
			rpcRequest.set_data(deserializeRpcPacket.data());
			// Process the RPC request
			if (_lockstepGroupPtr)
			{
				_lockstepGroupPtr->CollectInput({{_sessionGuid, std::make_shared<RpcRequest>(rpcRequest)}});
			}

			_receiveBuffer.clear();
			_receiveNetSize = 0;
			_receiveDataSize = 0;
			boost::asio::post(_strand.wrap([this]() { AsyncReadSize(); }));
		}));
}