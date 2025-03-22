#ifndef SESSION_H
#define SESSION_H

#include <memory>
#include <boost/asio.hpp>

using io_context = boost::asio::io_context;
using tcp = boost::asio::ip::tcp;

class Session : std::enable_shared_from_this<Session>
{
public:
	Session(io_context& io, std::shared_ptr<tcp::socket> socket);
	~Session();
	void Start();

private:
	io_context& io;
	std::shared_ptr<tcp::socket> socket_ptr;
};

#endif // SESSION_H

