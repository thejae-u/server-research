#ifndef SERVER_H
#define	SERVER_H

#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>
#include <vector>

#include <mysqlx/xdevapi.h>
#include "NetworkData.h"
#include "RequestProcess.h"

using io_context = boost::asio::io_context;

class Server
{
public:
	Server(io_context& io, std::string id, std::string password);
	~Server();

	bool IsInitValid() { return _dbSessionPtr != nullptr && _dbSchemaPtr != nullptr; }

	void Start();
	void Stop();

	void AddReq(SNetworkData req);
	void ProcessReq();

private:
	std::string _dbUser;
	std::string _dbPassword;

	io_context& _io;
	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::shared_ptr<mysqlx::Schema> _dbSchemaPtr;
	std::shared_ptr<RequestProcess> _reqProcessPtr;

	std::queue<SNetworkData> _reqQueue;
	std::mutex _reqMutex;
	std::condition_variable _reqCV;

	bool _isRunning;

	mysqlx::Table GetTable(std::string tableName)
	{
		return _dbSchemaPtr->getTable(tableName, true);
	}
};

#endif // !SERVER_H

