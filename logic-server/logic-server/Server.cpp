#include "Server.h"
#include "Session.h"

#include <iostream>

Server::Server(io_context& io, tcp::acceptor& acceptor): _io(io), _acceptor(acceptor)
{
}

Server::~Server()
{
}

void Server::StartServer()
{
	auto self(shared_from_this());
	auto newSession = std::make_shared<Session>(_io, self);
	
	_acceptor.async_accept(newSession->GetSocket(), [this, newSession](const boost::system::error_code& ec)
	{
		if (!ec)
		{
			std::cout << "Client Connected\n";
			newSession->Start();
			
			// Add session to the set
			_sessions.insert(newSession);
		}
		else
		{
			std::cerr << "Accept Failed: " << ec.message() << "\n";
		}

		StartServer(); // Accept next client
	});
}