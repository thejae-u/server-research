#include "DBSession.h"

DBSession::DBSession(io_context& io, std::string ip, std::string id, std::string password)
	: _io(io)
{
	try
	{
		auto session = std::make_shared<mysqlx::Session>(mysqlx::getSession(ip, 33060, id, password)); // DBMS connection
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

	if (_dbSessionPtr == nullptr || _dbSchemaPtr == nullptr)
	{
		std::cerr << "DB Connection Failed\n";
		return;
	}

	std::cout << "DBMS Connected\n";

	// Start DB Session
	boost::asio::post(_io, [this]() { ProcessReq(); })
}

DBSession::~DBSession()
{
	_dbSessionPtr->close();
}

void DBSession::AddReq(SNetworkData req)
{
	{
		std::lock_guard<std::mutex> lock(_reqMutex);
		_reqQueue.push(req);
	}

	std::cout << "Hello\n";
}

void DBSession::ProcessReq()
{
	_reqMutex.lock();

	if (_reqQueue.empty())
	{
		_reqMutex.unlock();
		boost::asio::post(_io, [this]() { ProcessReq(); }); // Restart ProcessReq Async
		return;
	}
	else
	{
		SNetworkData req = _reqQueue.front();
		_reqQueue.pop(); // Remove from Queue
		_reqMutex.unlock();

		// Mutex can't be locked under this line

		std::shared_ptr<std::vector<std::string>> splitedData =
			std::make_shared<std::vector<std::string>>(Server_Util::SplitString(req.data)); // split data by ','

		switch (req.type)
		{
		case ENetworkType::LOGIN:
			break;

		case ENetworkType::REGISTER:
			break;

		// Other cases are not implemented
		default:
			break;
		}
	}
}
