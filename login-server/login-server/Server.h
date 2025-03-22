#ifndef SERVER_H
#define SERVER_H

#define PORT 55000

#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>

#include "Session.h"

using io_context = boost::asio::io_context;
using tcp = boost::asio::ip::tcp;

class Server : std::enable_shared_from_this<Server>
{
public:
	Server(std::shared_ptr<io_context> io_ptr);
	~Server();

	void AsyncAccept();

private:
	std::shared_ptr<io_context> io_ptr;
	std::shared_ptr<tcp::acceptor> acceptor_ptr;
	std::shared_ptr<tcp::socket> socket_ptr;
};

#endif // SERVER_H