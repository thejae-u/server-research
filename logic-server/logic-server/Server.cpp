#include "Server.h"
#include "Session.h"

#include <iostream>

Server::Server(const io_context::strand& strand, tcp::acceptor& acceptor) : _strand(strand), _acceptor(acceptor), _sessionsCount(0)
{
	_sessions.resize(_sessionsCount);
}

Server::~Server()
{
}

void Server::AcceptClientAsync()
{
	auto self(shared_from_this());
	auto newSession = std::make_shared<Session>(_strand, self);

	_acceptor.async_accept(newSession->GetSocket(), _strand.wrap([this, newSession](const boost::system::error_code& ec)
	{
		if (ec)
		{
			std::cerr << "accept failed: " << ec.message() << "\n";
			boost::asio::post(_strand.wrap([this]() { AcceptClientAsync(); }));
			return;
		}
		
		std::cout << "Client Connected: " << newSession->GetSocket().remote_endpoint().address() << "\n";
		newSession->Start();
            
		// Add session to the set
		_sessions.push_back(newSession);
		++_sessionsCount;
		std::cout << "Sessions.size(): " << _sessions.size() << "\n";
		
		boost::asio::post(_strand.wrap([this]() { AcceptClientAsync(); })); // Accept next client
	}));
}

void Server::DisconnectSession(const std::shared_ptr<Session>& caller)
{
	std::cout << "Client Disconnected: " << caller->GetSocket().remote_endpoint().address() << "\n";
	_sessions.erase(std::find(_sessions.begin(), _sessions.end(), caller));
	--_sessionsCount;
}

void Server::BroadcastAll(std::shared_ptr<Session> caller, RpcPacket packet) 
{
	for (auto& session : _sessions)
	{
		if (session == caller)
			continue;

		boost::asio::post(_strand.wrap([this, session, packet]() { session->RpcProcess(packet); }));
	}
}
