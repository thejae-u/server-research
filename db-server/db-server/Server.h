#pragma once
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>
#include <mysqlx/xdevapi.h>

struct SNetworkData;

class RequestProcess;
class DBSession;
class Session;

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
	void Stop(); // Stop All Sessions

	void AddReq(SNetworkData req);
	void ProcessReq();

	void AcceptClient();

private:
	std::string _dbUser;
	std::string _dbPassword;
	std::shared_ptr<DBSession> _dbSessionPtr;

	io_context& _io;
	boost_acceptor& _acceptor;

	std::set<std::shared_ptr<Session>> _sessions;
	std::shared_ptr<std::vector<std::thread>> _processThreads;

	std::queue<SNetworkData*> _reqQueue;
	std::mutex _reqMutex;

	bool _isRunning;
};

