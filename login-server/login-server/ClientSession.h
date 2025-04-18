#pragma once
#include <memory>
#include <boost/asio.hpp>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using boost_acceptor = boost::asio::ip::tcp::acceptor;
using boost_socket = boost::asio::ip::tcp::socket;

using n_data = NetworkData::NetworkData;

class Server;
class DBConnectSession;

class ClientSession : public std::enable_shared_from_this<ClientSession>
{
public:
	ClientSession(io_context& io, const std::shared_ptr<Server>& serverPtr, std::size_t sessionId);
	~ClientSession();
	void Start();
	void Stop();

	void ReceiveSize();
	void ReceiveData();
	void ReplyReq(const n_data& reply) const;

	boost_socket& GetSocket() const { return *_socketPtr; }

private:
	io_context& _io;
	std::shared_ptr<boost_socket> _socketPtr;
	std::shared_ptr<Server> _serverPtr;
	std::shared_ptr<DBConnectSession> _dbConnectSessionPtr;
	std::size_t _sessionId;

	std::uint32_t _netSize;
	std::uint32_t _dataSize;
	std::vector<char> _buffer;
};