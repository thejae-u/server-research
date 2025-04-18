#include "Server.h"
#include "Session.h"
#include "DBSession.h"
#include "RequestProcess.h"
#include "db-server-class-utility.h"

Server::Server(io_context& io, boost_acceptor& acceptor, const std::size_t& threadCount,
	const std::string& dbIp, const std::string& id, const std::string& password)
	: _io(io), _acceptor(acceptor), _isRunning(false)
{
	_dbIp = dbIp;
	_dbUser = id;
	_dbPassword = password;
	_networkAvailableThreadCount = threadCount / 10;
	_sessionAvailableThreadCount = threadCount - _networkAvailableThreadCount;
	_sessionIdCounter = 0;
}

Server::~Server()
{
	for (auto& session : _sessions)
	{
		session->Stop();
	}

	_sessions.clear();
}

void Server::Start()
{
	_isRunning = true;
	AcceptClient();
}

void Server::AcceptClient() // Accept Login or Logic Sessions
{
	auto self = shared_from_this();

	{
		std::unique_lock<std::mutex> sessionsLock(_sessionsMutex);
		_sessionsCondition.wait(sessionsLock, [this]() { return _isRunning || _sessions.size() < _sessionAvailableThreadCount / 10; });
	}

	_sessionsCondition.notify_one();
	
	auto newSessionPtr = std::make_shared<Session>(_io, self, ++_sessionIdCounter);

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

void Server::RemoveSession(const std::shared_ptr<Session>& session)
{
	{
		std::unique_lock<std::mutex> sessionsLock(_sessionsMutex);
		_sessionsCondition.wait(sessionsLock, [this]() { return _isRunning; });
		
		_sessions.erase(session);
	}

	_sessionsCondition.notify_one();
}

void Server::FlushSessions()
{
	// Remove socket closed sessions
	{
		std::unique_lock<std::mutex> sessionsLock(_sessionsMutex);
		_sessionsCondition.wait(sessionsLock, [this]() { return _isRunning; });

		for (auto it = _sessions.begin(); it != _sessions.end();)
		{
			if (!(*it)->GetSocket().is_open())
			{
				it = _sessions.erase(it);
				continue;
			}

			++it;
		}
	}

	_sessionsCondition.notify_one();

	FlushSessions();
}


void Server::Stop()
{
	_isRunning = false;

	for (auto& session : _sessions)
	{
		session->Stop();
	}

	_sessions.clear();
}
