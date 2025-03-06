#include "Server.h"

Server::Server(io_context& io, short port)
	: port_(port), io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port))
{
	this->AcceptClients();

	io_.post([this] { this->FlushSessions(); });
	sessions_.emplace_back(std::thread([this] { io_.run(); }));
}

Server::~Server()
{
	for (auto& state : this->sessionStates_)
	{
		state.first->close();
		sessionStates_.erase(state.first);
	}
}

void Server::Start()
{
	for (auto& session : sessions_)
	{
		session.join();
	}
}

void Server::Stop()
{
	io_.stop();
}

void Server::FlushSessions()
{
	for (auto& state : this->sessionStates_)
	{
		if (!state.first->is_open())
		{
			this->sessionStates_.erase(state.first);
		}
	}
}
