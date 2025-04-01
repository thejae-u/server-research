#include "Server.h"
#include "db-server-class-utility.h"

Server::Server(io_context& io, boost_acceptor& acceptor, std::string id, std::string password) : _io(io), _acceptor(acceptor), _isRunning(false)
{
	_dbUser = id;
	_dbPassword = password;
}

Server::~Server()
{
	for (auto& session : _sessions)
	{
		session->Stop();
	}

	_sessions.clear();

	for (auto& process : *_processThreads)
	{
		if (process.joinable())
			process.join();
	}
}

void Server::Start()
{
	try
	{
		auto session = std::make_shared<mysqlx::Session>(mysqlx::getSession("localhost", 33060, _dbUser, _dbPassword)); // DBMS connection
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
		_isRunning = false;
		return;
	}

	_isRunning = true;

	AcceptClient();

	_reqProcessPtr = std::make_shared<RequestProcess>(_dbSessionPtr, _dbSchemaPtr);

	boost::asio::post(_io, [this]() { ProcessReq(); });
}

void Server::AcceptClient() // Accept Login or Logic Sessions
{
	auto self = shared_from_this();

	std::shared_ptr<Session> newSessionPtr = std::make_shared<Session>(_io, self, _sessions.size());

	_acceptor.async_accept(newSessionPtr->GetSocket(), [this, self, newSessionPtr](const boost_ec& ec)
		{
			if (!ec)
			{
				_sessions.insert(newSessionPtr);
				newSessionPtr->Start();
				std::cout << "New Session Connected : " << newSessionPtr->GetSocket().remote_endpoint().address() << "\n";
			}
			else
			{
				std::cerr << "Error: " << ec.message() << "\n";
			}

			AcceptClient();
		});
}

void Server::Stop()
{
	_isRunning = false;

	boost::asio::post(_io, [this]() {_reqProcessPtr->SaveServerLog("Server Off"); });
}

void Server::AddReq(SNetworkData req)
{
	_reqMutex.lock();
	_reqQueue.push(req);
	_reqMutex.unlock();
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
		// Do nothing
		_reqMutex.unlock(); // return mutex
	}
	else
	{
		SNetworkData req = _reqQueue.front();
		_reqQueue.pop();
		std::cout << "Request Popped\n";

		_reqMutex.unlock(); // return mutex

		std::shared_ptr<std::vector<std::string>> splitedData = std::make_shared<std::vector<std::string>>(Server_Util::SplitString(req.data)); // split data by ','

		switch (req.type)
		{
		case ENetworkType::LOGIN:
			if(_reqProcessPtr->RetreiveUserID(*splitedData) == ELastErrorCode::USER_NOT_FOUND) // Not found user in db
			{
				boost::asio::post(_io, [this, splitedData]() { _reqProcessPtr->SaveServerLog("User not found" + (*splitedData)[0]); });
				break;
			}

			if (_reqProcessPtr->Login(*splitedData) == ELastErrorCode::SUCCESS)
			{
				boost::asio::post(_io,
					[this, splitedData]()
					{
						std::string userName = (*splitedData)[0];
						_reqProcessPtr->SaveServerLog("Login Success user_name" + userName);
						_reqProcessPtr->SaveUserLog(userName, "Login Success");	
					});
			}
			else
			{
				boost::asio::post(_io, [this, splitedData]()
					{
						std::string userName = (*splitedData)[0];
						_reqProcessPtr->SaveServerLog("Login Failed " + userName);
					});
			}
			break;

		case ENetworkType::REGISTER:
			if (_reqProcessPtr->RetreiveUserID(*splitedData) == ELastErrorCode::USER_ALREADY_EXIST)
			{
				std::string userName = (*splitedData)[0];
				boost::asio::post(_io, [this, splitedData]()
					{
						std::string userName = (*splitedData)[0];
						_reqProcessPtr->SaveUserLog(userName, "Already Exist");
					});
				break;
			}

			if (_reqProcessPtr->Register(*splitedData) == ELastErrorCode::SUCCESS)
			{
				std::string userName = (*splitedData)[0];
				boost::asio::post(_io, [this, userName]()
					{
						_reqProcessPtr->SaveServerLog("Register Success " + userName);
						_reqProcessPtr->SaveUserLog(userName, "First Register");
					});
			}
			else // System Error
				boost::asio::post(_io, [this]() { _reqProcessPtr->SaveServerLog("Register Failed by UnknownError"); });
			break;

		case ENetworkType::ADMIN_SERVER_OFF:
			Stop();
			boost::asio::post(_io, [this]() { _reqProcessPtr->SaveServerLog("Server Off"); });
			break;

		case ENetworkType::ACCESS: // Not implemented
		case ENetworkType::LOGOUT:

		default:
			std::cerr << "Invalid request\n";

			std::string log = "Invalid request: " + req.uuid + " " + req.data;
			boost::asio::post(_io, [this, log]() { _reqProcessPtr->SaveServerLog(log); });
			break;
		}
	}

	boost::asio::post(_io, [this]() { ProcessReq(); });
}