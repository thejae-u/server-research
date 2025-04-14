#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <set>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using boost_acceptor = boost::asio::ip::tcp::acceptor;
using boost_ec = boost::system::error_code;
using boost_ep = boost::asio::ip::tcp::endpoint;

using n_data = NetworkData::NetworkData;
using n_data_type = NetworkData::ENetworkType;
using n_login_err = NetworkData::ELoginError;

class ClientSession;

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io, boost_acceptor& acceptor, const std::shared_ptr<boost_ep>& endPoint);
	~Server();

	void Start();
	void Stop();

	void AsyncAccept();

	std::shared_ptr<boost_ep> GetEndPoint() const { return _dbEndPointPtr; }

private:
	io_context& _io;
	boost_acceptor& _acceptor;

	std::shared_ptr<boost_ep> _dbEndPointPtr;
	
	std::shared_ptr<std::set<std::shared_ptr<ClientSession>>> _sessionsPtr;
	std::mutex _sessionSetMutex;
	std::condition_variable _sessionSetCondVar;
	std::size_t _sessionIdCounter;

	std::shared_ptr<std::queue<n_data>> _reqQueue;
	std::mutex _reqQueueMutex;
	std::condition_variable _reqQueueCondVar;
	
	bool _isRunning;
};