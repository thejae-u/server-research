#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

#include <mutex>
#include <memory>
#include <vector>

#include "GroupManager.h"
#include "NetworkData.pb.h"

using IoContext = boost::asio::io_context;
using namespace boost::asio::ip;
using namespace NetworkData;

class Session;
class LockstepGroup;

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(const IoContext::strand& strand, tcp::acceptor& acceptor);
	~Server() = default;
	
	void AcceptClientAsync();

private:
	IoContext::strand _strand;
	tcp::acceptor& _acceptor;

	std::unique_ptr<GroupManager> _groupManager;
	boost::uuids::random_generator_mt19937 _sessionUuidGenerator;
};
