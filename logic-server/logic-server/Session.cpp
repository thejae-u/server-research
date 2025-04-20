#include "Session.h"
#include "Server.h"
#include <iostream>
#include "Utility.h"

Session::Session(boost::asio::io_context& io, std::shared_ptr<Server> serverPtr) : _io(io), _serverPtr(serverPtr)
{
	_socketPtr = std::make_shared<tcp::socket>(io);
}

Session::~Session()
{
	
}

void Session::Start()
{
	std::cout << "Session Started: " << _socketPtr->remote_endpoint().address() << "\n";
	boost::asio::post(_io, [this]() { AsyncReadSize(); });
}

void Session::AsyncReadSize()
{
	auto self(shared_from_this());
	boost::asio::async_read(*_socketPtr, boost::asio::buffer(&_netSize, sizeof(_netSize)),
		[this, self](const boost::system::error_code& sizeEc, std::size_t)
		{
			if (sizeEc)
			{
				std::cerr << "Error Receiving Size: " << sizeEc.message() << "\n";
				if (sizeEc == boost::asio::error::eof || sizeEc == boost::asio::error::connection_reset)
				{
					std::cout << "Client Disconnected: " << _socketPtr->remote_endpoint().address() << "\n";
					return;
				}
				
				std::cerr << "Error Receiving Size: " << sizeEc.message() << "\n";
				AsyncReadSize();
				return;
			}

			// Convert network byte order to host byte order
			_netSize = ntohl(_netSize);
			_dataSize = _netSize;

			if (_buffer.size() < _netSize)
				_buffer.resize(_dataSize);

			AsyncReadData();
		});
}

void Session::AsyncReadData()
{
	auto self(shared_from_this());
	boost::asio::async_read(*_socketPtr, boost::asio::buffer(_buffer),
		[this, self](const boost::system::error_code& dataEc, std::size_t)
		{
			if (dataEc)
			{
				if (dataEc == boost::asio::error::eof || dataEc == boost::asio::error::connection_reset)
				{
					std::cout << "Client Disconnected: " << _socketPtr->remote_endpoint().address() << "\n";
					return;
				}

				std::cerr << "Error Receiving Data: " << dataEc.message() << "\n";
				AsyncReadSize();

				return;
			}

			auto receiveData = std::string(_buffer.begin(), _buffer.end());
			RpcPacket deserializeRpcPacket;
			if (!deserializeRpcPacket.ParseFromString(receiveData))
			{
				std::cerr << "In ReadData Error Parsing Data: " << receiveData << "\n";
				return;
			}

			// Process the request asynchronously
			boost::asio::post(_io, [this, deserializeRpcPacket]() { ProcessRequest(deserializeRpcPacket); });
			
			AsyncReadSize();
		});
}

void Session::ProcessRequest(const RpcPacket& reqPacket)
{
	switch (reqPacket.method())
	{
	case MOVE: // handle move request
		{
			PositionData positionData;
			if (!positionData.ParseFromString(reqPacket.data()))
			{
				std::cerr << "In PR Error Parsing Position Data\n";
				return;
			}
        
			std::cout << "Move Request: " << Utility::PositionDataToString(positionData) << "\n";	
			break;
		}
	// Not Implemented
	case ATTACK:
		break;
	case DROP_ITEM:
		break;
	case USE_ITEM:
		break;
	case USE_SKILL:
		break;

	case LOGOUT:
		// Handle logout request
		break;
		
	case NONE:
	case IN_GAME_NONE:
		// No operation
		break;
	default:
		assert(false, "Unknown method in RpcPacket");
		break;
	}
}
