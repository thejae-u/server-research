#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <memory>

#include "NetworkData.pb.h"

class Server;
class DBSession;

// for Login, Logic Server Connection

using io_context = boost::asio::io_context;
using boost_socket = boost::asio::ip::tcp::socket;
using boost_ec = boost::system::error_code;
using n_data = NetworkData::NetworkData;
using n_type = NetworkData::ENetworkType;

enum class ESessionReq
{
	LOGIN,
	REGISTER,
	ADMIN_SERVER_OFF,
};

enum class ESessionType
{
	LOGIN,
	LOGIC,
	ADMIN,
};

class Session : public std::enable_shared_from_this<Session>
{
public:
	Session(io_context& io, const std::shared_ptr<Server>& server, std::size_t sessionId);
	~Session();

	boost_socket& GetSocket() const { return *_socket; }
	void Start();

	void Stop();
	void AsyncReceiveSize();
	void ReplyLoginReq(const n_data& req);

	// Test Code

private:
	io_context& _io;
	std::shared_ptr<boost_socket> _socket;
	std::shared_ptr<Server> _serverPtr;
	std::size_t _sessionId;
	std::shared_ptr<DBSession> _dbSessionPtr;

	std::uint32_t _netSize;
	std::uint32_t _dataSize;
	std::vector<char> _buffer;
	
	void AsyncReceiveData();
};