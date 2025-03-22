#include "Server.h"

Server::Server(std::shared_ptr<io_context> io_ptr) : io_ptr(std::move(io_ptr))
{
	acceptor_ptr = std::make_shared<tcp::acceptor>(*io_ptr, tcp::endpoint(tcp::v4(), PORT));
	socket_ptr = std::make_shared<tcp::socket>(*io_ptr);
}

Server::~Server()
{

}

void Server::AsyncAccept()
{
	acceptor_ptr->async_accept(*socket_ptr, [this](const boost::system::error_code& ec)
		{
			if (!ec)
			{
				std::make_shared<Session>(*io_ptr, socket_ptr)->Start();
			}
			AsyncAccept();
		});
}
