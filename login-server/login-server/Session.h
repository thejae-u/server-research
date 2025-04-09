#pragma once
#include <memory>
#include <boost/asio.hpp>

using io_context = boost::asio::io_context;
using boost_acceptor = boost::asio::ip::tcp::acceptor;
using boost_socket = boost::asio::ip::tcp::socket;

class Server;

class Session : std::enable_shared_from_this<Session>
{
public:
	Session(io_context& io, const std::shared_ptr<Server>& serverPtr, std::size_t sessionId);
	~Session();
	void Start();

	void ProcessReq();
	void ReceiveReq();

	boost_socket& GetSocket() const { return *_socketPtr; }

private:
	io_context& _io;
	std::shared_ptr<boost_socket> _socketPtr;
	std::shared_ptr<Server> _serverPtr;
	std::size_t _sessionId;
};
