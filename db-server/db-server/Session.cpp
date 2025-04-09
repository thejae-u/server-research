#include "Session.h"

#include "db-server-class-utility.h"
#include "Server.h"
#include "DBSession.h"
#include "db-system-utility.h"
#include <boost/endian.hpp>

Session::Session(io_context& io, const std::shared_ptr<Server>& server, std::size_t sessionId)
	: _io(io), _serverPtr(server), _sessionId(sessionId)
{
	_socket = std::make_shared<boost_socket>(io); // empty socket

	std::size_t dbThreadCount = 9; // can be changed

	_dbSessionPtr =
		std::make_shared<DBSession>(io, dbThreadCount, _serverPtr->GetDbIp(), _serverPtr->GetDbUser(), _serverPtr->GetDbPassword());

	if (!_dbSessionPtr->IsConnected())
	{
		std::cerr << "DB Connection Failed : " << std::this_thread::get_id() << " Session\n";
		return;
	}

	std::cout << "DB Connection Success : " << std::this_thread::get_id() << " Session id - " << _sessionId << "\n";

	_buffer.resize(0);
}

Session::~Session()
{
	if (_socket->is_open())
		Stop();

	std::cout << "Session Closed : " << std::this_thread::get_id() << " Session id - " << _sessionId << "\n";
}

void Session::Start()
{
	// start session
	AsyncReceiveSize();
	std::cout << "Session Start : " << _socket->remote_endpoint().address() << "\n";
}

void Session::Stop()
{
	// Remove from server
	const auto self(shared_from_this());
	_serverPtr->RemoveSession(self);
	
	// stop session
	_socket->close();
}

void Session::AsyncReceiveSize()
{
	// Receive from client(Login, Logic Server Connection)
	// Add Request to Server
	auto self(shared_from_this());

	_socket->async_read_some(boost::asio::buffer(&_netSize, sizeof(_netSize)),
		[this](const boost::system::error_code& ec, std::size_t)
		{
			if (!ec)
			{
				_dataSize = ntohl(_netSize);
				if (_buffer.capacity() < _dataSize)
                    _buffer.resize(_dataSize);
				
				AsyncReceiveData();
			}
			else
			{
				if (ec == boost::asio::error::eof)
                {
                    std::cout << "Session Closed : " << _socket->remote_endpoint().address() << "\n";
					Stop();
					return;
                }
				
				std::cerr << "Error Receive Size: " << ec.message() << "\n";
			}
		});
}

void Session::AsyncReceiveData()
{
	auto self(shared_from_this());

	boost::asio::async_read(*_socket, boost::asio::buffer(_buffer),
		[this](const boost::system::error_code& ec, std::size_t)
		{
			if (!ec)
			{
				std::string receivedData(_buffer.begin(), _buffer.end());

				n_data deserializedData;
				
				try
				{
					deserializedData.ParseFromString(receivedData);
				}
				catch (const std::exception& e)
                {
                    std::cerr << "Error Parsing Data: " << e.what() << "\n";
					std::cout << "Session " << _sessionId << " Close : " << _socket->remote_endpoint().address() << "\n";
					Stop();
                    return;
                }

				switch (deserializedData.type())
				{
				case n_type::LOGIN:
					break;
					
				default:
					break;
				}

				_netSize = 0;
				_dataSize = 0;
				_buffer.clear();

				AsyncReceiveSize();
			}
			else
			{
				std::cerr << "Error Receive Data: " << ec.message() << "\n";
			}
		});
}

void Session::ReplyLoginReq(const n_data& req)
{
	switch (req.type())
	{
	case n_type::ACCESS:
		// Send Logic Server Connection
		break;

	case n_type::REJECT:
		break;
		
	default:
		break;
	}
}

// Test Code Area