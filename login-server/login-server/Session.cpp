#include "Session.h"

Session::Session(boost::asio::io_context& io, std::shared_ptr<tcp::socket> socket) : io(io), socket_ptr(std::move(socket))
{
}

Session::~Session()
{
}

void Session::Start() // Connect with client and Recevie data
{

}
