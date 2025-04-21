#pragma once
#include <set>
#include <boost/asio.hpp>
#include <mutex>
#include <memory>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using namespace boost::asio::ip;
using namespace NetworkData;

class Session;

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io, tcp::acceptor& acceptor);
	~Server();

	void StartServer();
	void BroadcastAll(const std::shared_ptr<Session>& caller, const std::shared_ptr<RpcPacket>& packetPtr);

private:
	io_context& _io;
	tcp::acceptor& _acceptor;
	std::set<std::shared_ptr<Session>> _sessions;

	std::mutex _sessionsMutex;
};
