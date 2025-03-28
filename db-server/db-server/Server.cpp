#include "Server.h"

Server::Server(io_context& io) : _io(io)
{
	try
	{
		auto session = std::make_shared<mysqlx::Session>(mysqlx::getSession("localhost", 33060, "root", "thejaeu"));
		auto db = std::make_shared<mysqlx::Schema>(session->getSchema("mmo_server_data"));

		_dbSessionPtr = session;
		_dbSchemaPtr = db;
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
		_dbSessionPtr = nullptr;
		_dbSchemaPtr = nullptr;
	}

	_reqProcessPtr = std::make_shared<RequestProcess>();
}

Server::~Server()
{
}

void Server::Start()
{
	try
	{
		auto tableData = _dbSchemaPtr->getTable("users").select("uuid", "user_name").execute();
		for (auto row : tableData)
		{
			std::cout << "uuid: " << row[0] << ", user_name: " << row[1] << "\n";
		}
	}
	catch (const mysqlx::Error& err)
	{
		std::cerr << "Error: " << err.what() << "\n";
	}
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

	switch (data.type)
	{
	case ENetworkType::OPTION_ONE:
		_reqProcessPtr->Work1();
		break;
	case ENetworkType::OPTION_TWO:
		_reqProcessPtr->Work2();
		break;
	case ENetworkType::OPTION_THREE:
		_reqProcessPtr->Work3();
		break;

	default:
		std::cerr << "Invalid Network Type\n";
		break;
	}
}
