#pragma once
#include <mysqlx/xdevapi.h>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <memory>

#include "NetworkData.h"

class RequestProcess;
class Session;

using io_context = boost::asio::io_context;

class DBSession : public std::enable_shared_from_this<DBSession>
{
public:
	DBSession(io_context& io, std::string ip, std::string id, std::string password);
	~DBSession();

	void Stop();

	void AddReq(SNetworkData req);
	void ProcessReq();
	
	bool IsConnected() const; 

private:
	io_context& _io; // io context owner by Server main thread

	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::shared_ptr<mysqlx::Schema> _dbSchemaPtr;
	std::shared_ptr<RequestProcess> _reqProcessPtr;

	std::queue<SNetworkData> _reqQueue;
	std::mutex _reqMutex;

	std::mutex _processMutex;

	bool _isRunning;
};

