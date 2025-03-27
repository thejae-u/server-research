#include "Server.h"
#include <iostream>

Server::Server(io_context& io) : _io(io)
{
}

Server::~Server()
{
}

void Server::StartServer()
{
	auto self(shared_from_this());
}

void Server::FlushSessions()
{
	int count = 0;
	for (auto& session : _sessions)
	{
		if (!session->IsConnected())
		{
			count++;
			_sessions.erase(session);
		}
	}
	std::cout << "Flushed " << count << " sessions.\n";
}
