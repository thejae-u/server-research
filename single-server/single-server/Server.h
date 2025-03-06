#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <map>

#include "Session.h"

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io, short port);
	~Server();

	void Start();
	void Stop();

	void FlushSessions();

private:
	short port_;
	io_context& io_;
	tcp::acceptor acceptor_;

	std::vector<std::thread> sessions_;
	std::map<socket_sptr, std::shared_ptr<Session>> sessionStates_;

	void AcceptClients();
};
