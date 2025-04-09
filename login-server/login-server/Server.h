#pragma once
#define PORT 55000

#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <set>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using boost_acceptor = boost::asio::ip::tcp::acceptor;
using boost_socket = boost::asio::ip::tcp::socket;
using boost_ec = boost::system::error_code;

using n_data = NetworkData::NetworkData;
using n_data_type = NetworkData::ENetworkType;
using n_login_err = NetworkData::ELoginError;

class Session;

class Server : std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io, boost_acceptor& acceptor, std::string dbIp, short dbPort, const std::size_t controlCount)
		: _io(io), _acceptor(acceptor), _dbServerIp(dbIp), _dbServerPort(dbPort),
		_sessionIdCounter(0), _sessionControlCount(controlCount), _isRunning(false) {}
	~Server() {}

	void Start();
	void Stop();

	void AddReq(const n_data& req);

	void AsyncAccept();

private:
	io_context& _io;
	boost_acceptor& _acceptor;

	std::string _dbServerIp;
	short _dbServerPort;
	
	std::shared_ptr<std::set<std::shared_ptr<Session>>> _sessionsPtr;
	std::mutex _sessionSetMutex;
	std::condition_variable _sessionSetCondVar;
	std::size_t _sessionIdCounter;

	std::shared_ptr<std::queue<n_data>> _reqQueue;
	std::mutex _reqQueueMutex;
	std::condition_variable _reqQueueCondVar;

	std::size_t _sessionControlCount;
	
	bool _isRunning;

	void ProcessReq();
};