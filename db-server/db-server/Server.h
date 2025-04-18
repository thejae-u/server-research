#pragma once
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>

#include "NetworkData.pb.h"

class RequestProcess;
class DBSession;
class Session;

using io_context = boost::asio::io_context;
using boost_socket = boost::asio::ip::tcp::socket;
using boost_acceptor = boost::asio::ip::tcp::acceptor;
using boost_ep = boost::asio::ip::tcp::endpoint;
using boost_ec = boost::system::error_code;

using n_data = NetworkData::NetworkData;
using n_type = NetworkData::ENetworkType;
using n_login_err = NetworkData::ELoginError;

class Session;

class Server : public std::enable_shared_from_this<Server>
{
public:
	Server(io_context& io, boost_acceptor& acceptor,const std::size_t& threadCount,
		const std::string& dbIp, const std::string& id, const std::string& password);
	~Server();

	bool IsRunning() const { return _isRunning; }

	void Start();
	void Stop(); // Stop All Sessions

	void AcceptClient();

	std::string GetDbIp() const { return _dbIp; } // Need to be changed
	std::string GetDbUser() const {return _dbUser;}
	std::string GetDbPassword() const { return _dbPassword; }

	void RemoveSession(const std::shared_ptr<Session>& session);

private:
	std::string _dbIp;
	std::string _dbUser;
	std::string _dbPassword;

	io_context& _io;
	boost_acceptor& _acceptor;

	std::set<std::shared_ptr<Session>> _sessions;
	std::mutex _sessionsMutex;
	std::condition_variable _sessionsCondition;

	std::size_t _sessionAvailableThreadCount;
	std::size_t _networkAvailableThreadCount;

	bool _isRunning;

	std::size_t _sessionIdCounter;

	void FlushSessions();
};

