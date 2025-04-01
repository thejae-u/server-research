#pragma once
#include <memory>
#include <mysqlx/xdevapi.h>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <thread>

#include "NetworkData.h"
#include "db-server-class-utility.h"

using io_context = boost::asio::io_context;

class DBSession : public std::enable_shared_from_this<DBSession>
{
public:
	DBSession(io_context& io, std::string ip, std::string id, std::string password);
	~DBSession();

	void AddReq(SNetworkData req);
	void ProcessReq();

private:
	io_context& _io; // io context onwer by Server main thread

	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::shared_ptr<mysqlx::Schema> _dbSchemaPtr;

	std::queue<SNetworkData> _reqQueue;
	std::mutex _reqMutex;
};

