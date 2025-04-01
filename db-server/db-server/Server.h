#pragma once
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>
#include <vector>
#include <mysqlx/xdevapi.h>

#include "RequestProcess.h"
#include "Session.h"

using io_context = boost::asio::io_context;
using boost_socket = boost::asio::ip::tcp::socket;
using boost_acceptor = boost::asio::ip::tcp::acceptor;
using boost_ep = boost::asio::ip::tcp::endpoint;
using boost_ec = boost::system::error_code;

class Session;

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io, boost_acceptor& acceptor, std::string id, std::string password);
	~Server();

	void Start();
	void Stop();

	void AddReq(SNetworkData req);
	void ProcessReq();

	void AcceptClient();

private:
	std::string _dbUser;
	std::string _dbPassword;

	io_context& _io;
	boost_acceptor& _acceptor;

	std::set<std::shared_ptr<Session>> _sessions;

	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::shared_ptr<mysqlx::Schema> _dbSchemaPtr;
	std::shared_ptr<RequestProcess> _reqProcessPtr;

	std::shared_ptr<std::vector<std::thread>> _processThreads;

	std::queue<SNetworkData> _reqQueue;
	std::mutex _reqMutex;
	std::condition_variable _reqCV;

	bool _isRunning;

	mysqlx::Table GetTable(std::string tableName)
	{
		return _dbSchemaPtr->getTable(tableName, true);
	}
};

