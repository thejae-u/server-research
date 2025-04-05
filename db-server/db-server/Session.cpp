#include "Session.h"
#include "Server.h"
#include "db-system-utility.h"
#include "NetworkData.h"

Session::Session(io_context& io, const std::shared_ptr<Server>& serverPtr, const std::size_t sessionId)
	: _io(io), _serverPtr(serverPtr), _sessionID(sessionId)
{
	_socket = std::make_shared<boost_socket>(io); // empty socket
}

Session::~Session()
{
	_socket->close();
}

void Session::Start()
{
	// start session
}

void Session::Stop()
{
	// stop session
}

void Session::RecvReq()
{
	// Receive from client(Login, Logic Server Connection)
	// Add Request to Server
	auto self = shared_from_this();

	std::shared_ptr<std::string> recvBuffer;

	//_socket->async_read_some(boost::asio::buffer(*recvBuffer), [this, self, recvBuffer](const boost_ec& ec)
	//	{
	//		if (!ec)
	//		{
	//			// Add Request to Server
	//		}
	//		else
	//		{
	//			// Error Handling
	//		}
	//	});
}

// Test Code Area
