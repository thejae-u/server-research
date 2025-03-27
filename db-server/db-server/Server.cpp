#include "Server.h"

Server::Server(io_context& io) : _io(io)
{
    auto session = std::make_shared<mysqlx::Session>(mysqlx::getSession("localhost", 33060, "root", "thejaeu"));
    auto db = std::make_shared<mysqlx::Schema>(session->getSchema("mmo_server_data"));

    _dbSessionPtr = session;
    _dbSchemaPtr = db;
}

Server::~Server()
{
}

void Server::Start()
{
}

void Server::Stop()
{
}

void Server::ProcessReq(SNetworkData data)
{
	if (data.bufSize == 0)
	{
		return;
	}

	std::string query(data.data, data.bufSize);
	// and more Code...
}
