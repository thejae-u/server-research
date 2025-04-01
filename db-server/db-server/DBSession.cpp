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

	_isRunning = true;

	// Start DB Session
	boost::asio::post(_io, [this]() { ProcessReq(); })
}

DBSession::~DBSession()
{
	std::cout << "DBSession Safe Exit Check\n";
}

void DBSession::Stop()
{
	_isRunning = false;

	std::this_thread::sleep_for(std::chrono::seconds(500)); // Wait for ProcessReq to finish

	_dbSessionPtr->close();
}

void DBSession::AddReq(SNetworkData req)
{
	std::lock_guard<std::mutex> lock(_reqMutex);
	_reqQueue.push(req);

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

		/*
			Mutex can't be locked under this line
		*/

		std::shared_ptr<std::vector<std::string>> splitedData =
			std::make_shared<std::vector<std::string>>(Server_Util::SplitString(req.data)); // split data by ','

		switch (req.type)
		{
		case ENetworkType::LOGIN:
		{
			std::string userName = (*splitedData)[0];
			std::string userPassword = (*splitedData)[1];
			
			auto rowResult = GetTable("users")->select("user_name", "user_password")
				.where("user_name = :name AND user_password = :password")
				.bind("name", userName)
				.bind("password", userPassword)
				.execute();

			if (rowResult.count() == 0)
			{
				std::cout << "Login Failed\n";
			}
			else
			{
				std::cout << "Login Success\n";
			}

			break;
		}
		case ENetworkType::REGISTER:
		{
			break;
		}

		// Other cases are not implemented
		default:
			break;
		}

		boost::asio::post(_io, [this]() { ProcessReq(); }); // Restart ProcessReq Async
	}

	assert(false); // This line should not be reached
}

bool DBSession::IsConnected() const
{
	return _dbSessionPtr != nullptr && _dbSchemaPtr != nullptr;
}
