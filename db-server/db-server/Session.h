#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <memory>

class Server;

// for Login, Logic Server Connection

using io_context = boost::asio::io_context;
using boost_socket = boost::asio::ip::tcp::socket;
using boost_ec = boost::system::error_code;

enum class ESessionReq
{
	LOGIN,
	REGISTER,
	ADMIN_SERVER_OFF,
};

class Session : public std::enable_shared_from_this<Session>
{
public:
	Session(io_context& io, std::shared_ptr<Server> serverPtr, std::size_t sessionID);
	~Session();

	boost_socket& GetSocket() { return *_socket; }

	void Start();
	void Stop();
	void RecvReq();

	// Test Code

private:
	std::size_t _sessionID;

	io_context& _io;

	std::shared_ptr<Server> _serverPtr;
	std::shared_ptr<boost_socket> _socket;
};

