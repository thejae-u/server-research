#include "Server.h"

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
	_dbSessionPtr = std::make_shared<DBSession>(_io, "localhost", _dbUser, _dbPassword);
	if (!_dbSessionPtr->IsConnected()) // DBMS Connection Failed
	{
		std::cerr << "DB Connection Failed\n";
		return;
	}

	_isRunning = true;
	AcceptClient();

	boost::asio::post(_io, [this]() { ProcessReq();	});
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

	_dbSessionPtr->Stop();

	for (auto& session : _sessions)
	{
		session->Stop();
	}

	_sessions.clear();
}

void Server::AddReq(SNetworkData req) // Add Request to Server
{
	std::lock_guard<std::mutex> lock(_reqMutex);
	_reqQueue.push(req);
}

void Server::ProcessReq() 
{
	std::lock_guard<std::mutex> lock(_reqMutex);

	if (_reqQueue.empty())
	{
		boost::asio::post(_io, [this]() { ProcessReq(); });

		return;
	}
	else
	{
		SNetworkData req = _reqQueue.front();
		_reqQueue.pop();

		_dbSessionPtr->AddReq(req);

		boost::asio::post(_io, [this]() { ProcessReq(); });
	}
}