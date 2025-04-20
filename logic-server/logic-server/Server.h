#pragma once
#include <set>
#include <boost/asio.hpp>

using io_context = boost::asio::io_context;
using namespace boost::asio::ip;

class Session;

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io, tcp::acceptor& acceptor);
	~Server();

	void StartServer();

private:
	io_context& _io;
	tcp::acceptor& _acceptor;
	std::set<std::shared_ptr<Session>> _sessions;
};
