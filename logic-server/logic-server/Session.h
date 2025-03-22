#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>
#include <memory>

using io_context = boost::asio::io_context;
using boost::asio::ip::tcp;

class Session : std::enable_shared_from_this<Session>
{
public:
	Session(boost::asio::io_context& io);
	~Session();
	void StartSession();
	void AsyncRecv();
	void AsyncSend();
	bool IsConnected() const { return _socket.is_open(); }

private:
	io_context& _io;
	tcp::socket _socket;
};

#endif // SESSION_H

