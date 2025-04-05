#include "Server.h"
#include "Session.h"
#include "DBSession.h"
#include "RequestProcess.h"
#include "db-server-class-utility.h"

Server::Server(io_context& io, boost_acceptor& acceptor, const std::size_t& threadCount,
	const std::string& id, const std::string& password)
: _io(io), _acceptor(acceptor), _isRunning(false)
{
	_dbUser = id;
	_dbPassword = password;
	
	_dbAvailableThreadCount = threadCount / 2; // DB Thread Count
	_networkAvailableThreadCount = (threadCount - _dbAvailableThreadCount) / 2; // Network Thread Count
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
	_dbSessionPtr = std::make_shared<DBSession>(_io, _dbAvailableThreadCount, "localhost", _dbUser, _dbPassword);
	if (!_dbSessionPtr->IsConnected()) // DBMS Connection Failed
	{
		std::cerr << "DB Connection Failed\n";
		return;
	}

	_isRunning = true;
	AcceptClient();

	for (std::size_t i = 0; i < _networkAvailableThreadCount; ++i) // make process threads by thread count
		boost::asio::post(_io, [this]() { ProcessReq();	});
}

void Server::AcceptClient() // Accept Login or Logic Sessions
{
	auto self = shared_from_this();

	auto newSessionPtr = std::make_shared<Session>(_io, self, _sessions.size());

	_acceptor.async_accept(newSessionPtr->GetSocket(), [this, newSessionPtr](const boost_ec& ec)
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

	_dbSessionPtr->Stop();

	for (auto& session : _sessions)
	{
		session->Stop();
	}

	_sessions.clear();
}

void Server::AddReq(const std::shared_ptr<SNetworkData>& req) // Add Request to Serve
{
	{
		std::unique_lock<std::mutex> lock(_reqMutex);
		_reqCondVar.wait(lock, [this]() { return _isRunning; });
		
		_reqQueue.push(req);
	}

	_reqCondVar.notify_one(); // Notify ProcessReq
}

void Server::ProcessReq()
{
	std::shared_ptr<SNetworkData> req;

	{
		std::unique_lock<std::mutex> lock(_reqMutex);
		_reqCondVar.wait(lock, [this]() { return !_reqQueue.empty() || !_isRunning; });

		if (!_isRunning && _reqQueue.empty())
			return;

		req = _reqQueue.front();
		_reqQueue.pop();
	}
	
	_dbSessionPtr->AddReq(req);
	boost::asio::post(_io, [this]() { ProcessReq(); });
}