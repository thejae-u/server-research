#pragma once
#include <mysqlx/xdevapi.h>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>

#include "NetworkData.pb.h"

class RequestProcess;
class Session;

using io_context = boost::asio::io_context;
using n_data = NetworkData::NetworkData;
using n_type = NetworkData::ENetworkType;
using n_login_err = NetworkData::ELoginError;

class DBSession : public std::enable_shared_from_this<DBSession>
{
public:
	DBSession(io_context& io, const std::size_t threadCount, const std::string& ip, const std::string& id, const std::string& password);
	~DBSession();

	void Stop();

	void AddReq(const std::pair<std::shared_ptr<Session>, std::shared_ptr<n_data>>& req);
	void ProcessReq();
	
	bool IsConnected() const; 

private:
	io_context& _io; // io context owner by Server main thread

	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::shared_ptr<mysqlx::Schema> _dbSchemaPtr;
	std::shared_ptr<RequestProcess> _reqProcessPtr;

	std::queue<std::pair<std::shared_ptr<Session>, std::shared_ptr<n_data>>> _reqQueue;
	std::mutex _reqMutex;

	std::mutex _processMutex;
	std::condition_variable _reqCondVar;
	std::condition_variable _reqProcessCondVar;

	std::mutex _processedMutex;
	std::condition_variable _processedCondVar;

	bool _isRunning;
};

