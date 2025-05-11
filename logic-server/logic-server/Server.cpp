#include "Server.h"
#include "Session.h"
#include "LockstepGroup.h"

#include <iostream>

Server::Server(const IoContext::strand& strand, tcp::acceptor& acceptor) : _strand(strand), _acceptor(acceptor)
{
	_groupManager = std::make_unique<GroupManager>(strand);
}

void Server::AcceptClientAsync()
{
	auto self(shared_from_this());
	auto newSession = std::make_shared<Session>(_strand, self, _sessionUuidGenerator());

	_acceptor.async_accept(newSession->GetSocket(), _strand.wrap([this, newSession](const boost::system::error_code& ec)
	{
		if (ec)
		{
			std::cerr << "accept failed: " << ec.message() << "\n";
			boost::asio::post(_strand.wrap([this]() { AcceptClientAsync(); }));
			return;
		}
		
		//std::cout << "Client Connected: " << newSession->GetSocket().remote_endpoint().address() << "\n";
		boost::asio::post(_strand.wrap([this, newSession]() { _groupManager->AddSession(newSession); }));
		boost::asio::post(_strand.wrap([this]() { AcceptClientAsync(); })); // Accept next client
	}));
}