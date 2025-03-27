#ifndef SERVER_H
#define	SERVER_H

#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>
#include <mysqlx/xdevapi.h>
#include "NetworkData.h"

using io_context = boost::asio::io_context;

class Server
{
public:
	Server(io_context& io);
	~Server();

	void Start();
	void Stop();

	void ProcessReq(SNetworkData data);

private:
	io_context& _io;
	std::shared_ptr<mysqlx::Session> _dbSessionPtr;
	std::shared_ptr<mysqlx::Schema> _dbSchemaPtr;
};

#endif // !SERVER_H

