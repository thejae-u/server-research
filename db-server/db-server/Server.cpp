#include "Server.h"
#include "db-server-class-utility.h"

Server::Server(io_context& io, std::string id, std::string password) : _io(io), _isRunning(false)
{
	_dbUser = id;
	_dbPassword = password;
}

Server::~Server()
{
}

void Server::Start()
{
	try
	{
		auto session = std::make_shared<mysqlx::Session>(mysqlx::getSession("localhost", 33060, _dbUser, _dbPassword));
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

	_reqProcessPtr = std::make_shared<RequestProcess>(_dbSchemaPtr);

	_isRunning = true;

	_io.post([this]() { ProcessReq(); });
}

void Server::Stop()
{
	_isRunning = false;

	_io.post([=]() { _reqProcessPtr->SaveServerLog("Server Off"); });
}

void Server::AddReq(SNetworkData req)
{
	_reqMutex.lock(); // lock mutex
	_reqQueue.push(req);
	_reqMutex.unlock(); // return mutex
}

void Server::ProcessReq()
{
	if (!_isRunning)
	{
		// server is down
		return;
	}

	// if request queue is empty, return and call ProcessReq() again
	_reqMutex.lock(); // lock mutex

	if (_reqQueue.empty())
	{
		_reqMutex.unlock();
	}
	else
	{
		SNetworkData req = _reqQueue.front();
		_reqQueue.pop();
		_reqMutex.unlock(); // return mutex

		std::vector<std::string> splitedData = Server_Util::SplitString(req.data); // split data by ','

		switch (req.type)
		{
		case ENetworkType::LOGIN:
			if(_reqProcessPtr->RetreiveUserID(splitedData) == ELastErrorCode::USER_NOT_FOUND) // Not found user in db
			{
				_io.post([=]() { _reqProcessPtr->SaveServerLog("User not found"); });
				break;
			}

			if (_reqProcessPtr->Login(splitedData) == ELastErrorCode::SUCCESS)
				_io.post([=]() { _reqProcessPtr->SaveServerLog("Login Success user_name " + splitedData[0]); });
			else
				_io.post([=]() { _reqProcessPtr->SaveServerLog("Login Failed user_name " + splitedData[0]); });

			break;

		case ENetworkType::REGISTER:
			if (_reqProcessPtr->RetreiveUserID(splitedData) == ELastErrorCode::USER_ALREADY_EXIST)
			{
				_io.post([=]() { _reqProcessPtr->SaveServerLog("User " + splitedData[0] + " already exist"); });
				break;
			}

			if (_reqProcessPtr->Register(splitedData) == ELastErrorCode::SUCCESS)
				_io.post([=]() { _reqProcessPtr->SaveServerLog("Register Success user_name " + splitedData[0]); });
			else
				_io.post([=]() { _reqProcessPtr->SaveServerLog("Register Failed user_name " + splitedData[0]); });
			break;

		case ENetworkType::ADMIN_SERVER_OFF:
			Stop();

			break;

		case ENetworkType::ACCESS: // Not implemented
		case ENetworkType::LOGOUT:

		default:
			std::cerr << "Invalid request\n";
			// Save Log to db (server_log Table)

			std::string log = "Invalid request: " + req.uuid + " " + req.data;

			_io.post([=]() { _reqProcessPtr->SaveServerLog(log); });

			break;
		}
	}

	_io.post([=]() { ProcessReq(); });
}