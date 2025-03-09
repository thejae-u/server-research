#include "Server.h"

Server::Server(io_context& io, short port)
	: port_(port), io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port))
{
	this->AcceptClients();

	io_.post([this] { this->FlushSessions(); });

	std::size_t numThreads = std::thread::hardware_concurrency();

	for (int i = 0; i < numThreads; i++)
	{
		sessions_.emplace_back(std::thread([this]
			{
				this->io_.run();
			}));
	}

	std::cout << "Server is running on port " << port_ << std::endl;
}

Server::~Server()
{
	this->Stop();
}

void Server::Start()
{
	std::cout << "Start Called\n";
}

void Server::Stop()
{
	for (auto& state : this->sessionStates_)
	{
		state.first->close();
		sessionStates_.erase(state.first);
	}

	io_.stop();
	for (auto& session : sessions_)
	{
		if (session.joinable())
			session.join();
	}
}

void Server::FlushSessions()
{
	for (auto& state : this->sessionStates_)
	{
		if (!state.first->is_open())
		{
			this->sessionStates_.erase(state.first);
		}
	}
}

void Server::AcceptClients()
{
	std::cout << "Accepting Clients\n";
	acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket)
		{
			if (!ec)
			{
				std::cout << "New Client Connected\n";
				auto session = std::make_shared<Session>(std::make_shared<tcp::socket>(std::move(socket)));
				sessionStates_.insert(std::make_pair(session->GetSocket(), session));
			}
			else
			{
				std::cout << "Error: " << ec.message() << std::endl;
			}
			this->AcceptClients();
		});
}
