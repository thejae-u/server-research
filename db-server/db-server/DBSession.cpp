#include "DBSession.h"
#include "NetworkData.h"
#include "RequestProcess.h"
#include "db-server-class-utility.h"

DBSession::DBSession(io_context& io, std::string ip, std::string id, std::string password)
	: _io(io), _isRunning(false)
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

	_reqProcessPtr = std::make_shared<RequestProcess>(_dbSessionPtr, _dbSchemaPtr);

	_isRunning = true;

	// Start DB Session
	boost::asio::post(_io, [this]() { ProcessReq(); });
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

void DBSession::AddReq(SNetworkData* req)
{
	std::lock_guard<std::mutex> lock(_reqMutex);
	_reqQueue.push(req);
}

void DBSession::ProcessReq()
{
	std::lock_guard<std::mutex> lock(_reqMutex);

	if (_reqQueue.empty())
	{
		boost::asio::post(_io, [this]() { ProcessReq(); }); // Restart ProcessReq Async
		return;
	}
	else
	{
		SNetworkData req = *_reqQueue.front();
		_reqQueue.pop(); // Remove from Queue

		/*
			Mutex can't be locked under this line
		*/

		std::shared_ptr<std::vector<std::string>> splitedData =
			std::make_shared<std::vector<std::string>>(Server_Util::SplitString(req.data)); // split data by ','

		ELastErrorCode ec = ELastErrorCode::UNKNOWN_ERROR;

		switch (req.type)
		{
		case ENetworkType::LOGIN:
		{
			std::string userName = (*splitedData)[0];
			std::string userPassword = (*splitedData)[1];

			std::lock_guard<std::mutex> lock(_processMutex);

			ec = _reqProcessPtr->Login({ userName, userPassword });

			std::string serverLog;
			std::string userLog;

			if (ec == ELastErrorCode::SUCCESS)
			{
				// Send Logic Server Connection
				serverLog = "User " + userName + " Login Success";
				userLog = "Login Success";
			}
			else
			{
				// Send Error Message
				serverLog = "User " + userName + " Login Failed";
				userLog = "Login Failed";
			}
		
			boost::asio::post(_io, [this, serverLog]() { _reqProcessPtr->SaveServerLog(serverLog); });
			boost::asio::post(_io, [this, userName, userLog]() { _reqProcessPtr->SaveUserLog(userName, userLog); });
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
}

bool DBSession::IsConnected() const
{
	return _dbSessionPtr != nullptr && _dbSchemaPtr != nullptr;
}
