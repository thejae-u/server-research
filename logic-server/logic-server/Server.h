#ifndef SERVER_H
#define SERVER_H

#include "Session.h"
#include <set>

using io_context = boost::asio::io_context;

class Server : std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io);
	~Server();

	void StartServer();
	void FlushSessions();

private:
	io_context& _io;
	std::set<std::shared_ptr<Session>> _sessions;
};

#endif // SERVER_H

