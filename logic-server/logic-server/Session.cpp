#include "Session.h"

Session::Session(boost::asio::io_context& io) : _io(io), _socket(io)
{
}

Session::~Session()
{
}

void Session::StartSession()
{
	auto self(shared_from_this());
	_socket.async_connect(tcp::endpoint(tcp::v4(), 1234), [this](const boost::system::error_code& ec)
		{
			if (!ec)
			{
				AsyncRecv();
			}
		});
}

void Session::AsyncRecv()
{
	auto self(shared_from_this());
}

void Session::AsyncSend()
{
	auto self(shared_from_this());
}
