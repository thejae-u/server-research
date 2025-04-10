#pragma once

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

class ClientSession;

class Server : std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io, boost_acceptor& acceptor, std::string dbIp, int dbPort);
	~Server();

	void Start();
	void Stop();

	void AsyncAccept();

private:
	io_context& _io;
	boost_acceptor& _acceptor;

	std::string _dbServerIp;
	int _dbServerPort;
	
	std::shared_ptr<std::set<std::shared_ptr<ClientSession>>> _sessionsPtr;
	std::mutex _sessionSetMutex;
	std::condition_variable _sessionSetCondVar;
	std::size_t _sessionIdCounter;

	std::shared_ptr<std::queue<n_data>> _reqQueue;
	std::mutex _reqQueueMutex;
	std::condition_variable _reqQueueCondVar;
	
	bool _isRunning;
};