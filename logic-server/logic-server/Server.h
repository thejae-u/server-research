#pragma once
#include <vector>
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
	Server(const io_context::strand& strand, tcp::acceptor& acceptor);
	~Server();
	
	void AcceptClientAsync();
	void DisconnectSession(const std::shared_ptr<Session>& caller);
	void BroadcastAll(std::shared_ptr<Session> caller, RpcPacket packet);
	std::size_t GetSessionCount() const { return _sessions.size(); }

private:
	io_context::strand _strand;
	tcp::acceptor& _acceptor;
	std::vector<std::shared_ptr<Session>> _sessions;
	std::size_t _sessionsCount = 0;
};
