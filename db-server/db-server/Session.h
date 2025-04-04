#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <memory>

#include "NetworkData.h"

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
	Session(io_context& io, const std::shared_ptr<Server>& serverPtr, const std::size_t sessionId);
	~Session();

	boost_socket& GetSocket() const { return *_socket; }

	void Start();
	void Stop();
	void RecvReq();

	// Test Code

private:
	io_context& _io;
	std::shared_ptr<Server> _serverPtr;
	const std::size_t _sessionID;
	
	std::shared_ptr<boost_socket> _socket;
};

