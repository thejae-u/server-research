#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

#include <mutex>
#include <memory>
#include <vector>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using namespace boost::asio::ip;
using namespace NetworkData;

class Session;
class LockstepGroup;

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(const io_context::strand& strand, tcp::acceptor& acceptor);
	~Server() = default;
	
	void AcceptClientAsync();
	void DisconnectSession(const std::shared_ptr<Session>& caller);
	std::size_t GetSessionCount() const { return _sessions.size(); }

private:
	io_context::strand _strand;
	tcp::acceptor& _acceptor;
	std::vector<std::shared_ptr<Session>> _sessions;
	std::size_t _sessionsCount = 0;

	std::set<std::shared_ptr<LockstepGroup>> _groups;
	std::size_t _groupsCount = 0;
	std::size_t _maxSessionCountPerGroup = 10;

	boost::uuids::random_generator_mt19937 _guidGenerator;

	void CreateNewGroup();
};
