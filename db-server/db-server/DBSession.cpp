#include "DBSession.h"
#include "RequestProcess.h"
#include "db-server-class-utility.h"
#include "db-system-utility.h"
#include "Session.h"

DBSession::DBSession(io_context& io, const std::size_t threadCount, const std::string& ip, const std::string& id, const std::string& password)
	: _io(io), _isRunning(false)
{
	try
	{
		const auto session = std::make_shared<mysqlx::Session>(mysqlx::getSession(ip, 33060, id, password)); // DBMS connection
		const auto db = std::make_shared<mysqlx::Schema>(session->getSchema("mmo_server_data"));

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

	_reqProcessPtr = std::make_shared<RequestProcess>(_dbSessionPtr, _dbSchemaPtr);

	_isRunning = true;

	// Start DB Session
	for (std::size_t i = 0; i < threadCount; ++i) // make process threads by thread count
		boost::asio::post(_io, [this]() { ProcessReq(); });
}

DBSession::~DBSession()
{
	std::cout << "DBSession Safe Exit Check\n";
}

void DBSession::Stop()
{
	_isRunning = false;
	_dbSessionPtr->close();
}

void DBSession::AddReq(const std::pair<std::shared_ptr<Session>, std::shared_ptr<n_data>>& req)
{
	{
		std::unique_lock<std::mutex> lock(_reqMutex);
		_reqQueue.push(req);
	}

	_reqCondVar.notify_all();
}

void DBSession::ProcessReq()
{
	std::shared_ptr<n_data> req;
	std::shared_ptr<Session> reqPtr = nullptr;
	
	{
		std::unique_lock<std::mutex> lock(_reqMutex);
		_reqCondVar.wait(lock, [this]
		{
			return !_isRunning || !_reqQueue.empty();
		}); // Wait for Request

		if (!_isRunning && !_reqQueue.empty())
			return;

		req = _reqQueue.front().second;
	 	reqPtr = _reqQueue.front().first;
		
		_reqQueue.pop();
	}

	const auto splitData =
		std::make_shared<std::vector<std::string>>(Server_Util::SplitString(req->data())); // split data by ','

	auto lastErrorCode = ELastErrorCode::UNKNOWN_ERROR;

	switch (req->type())
	{
	case n_type::LOGIN:
		{
			auto userName = std::make_shared<std::string>((*splitData)[0]);
			
			{
				std::string userPassword = (*splitData)[1];
				std::unique_lock<std::mutex> lock(_processMutex);
				lastErrorCode = _reqProcessPtr->Login({ *userName, userPassword }); // Login transaction
			}

			auto serverLog = std::make_shared<std::string>();
			auto userLog = std::make_shared<std::string>();

			n_data replyData;

			if (lastErrorCode == ELastErrorCode::SUCCESS)
			{
				// Send Logic Server Connection
				*serverLog = "User " + *userName + " Login Success";
				*userLog = "Login Success";

				replyData.set_type(n_type::ACCESS);
			}
			else
			{
				// Send Error Message
				*serverLog = "User " + *userName + " Login Failed";
				*userLog = "Login Failed";

				replyData.set_type(n_type::REJECT);
			}

			reqPtr->ReplyLoginReq(replyData);
			
			std::thread serverLogThread([this, serverLog]()
			{
				std::unique_lock<std::mutex> lock(_processMutex);
				_reqProcessPtr->SaveServerLog(serverLog);
			});

			serverLogThread.detach();

			break;
		}
		
	// Other cases are not implemented
	case n_type::REGISTER:
	default:
		break;
	}

	boost::asio::post(_io, [this]() { ProcessReq(); }); // Restart ProcessReq Async
}

void DBSession::ReplyReq(const std::shared_ptr<std::pair<Session, std::shared_ptr<n_data>>>& replyData)
{
	// Reply to Session
}

bool DBSession::IsConnected() const
{
	return _dbSessionPtr != nullptr && _dbSchemaPtr != nullptr;
}
