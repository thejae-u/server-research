#include "Server.h"
#include "Session.h"
#include "LockstepGroup.h"

#include <iostream>

Server::Server(const IoContext::strand& workStrand, const IoContext::strand& rpcStrand, tcp::acceptor& acceptor) : _strand(workStrand), _rpcStrand(rpcStrand), _acceptor(acceptor)
{
	_uuidGenerator = std::make_shared<random_generator>();
	_groupManager = std::make_unique<GroupManager>(workStrand, _uuidGenerator);
	_isRunning = false;
}

void Server::Start()
{
	_isRunning = true;
	AcceptClientAsync();
}

void Server::Stop()
{
	SPDLOG_INFO("Server stopping...");

	_isRunning = false;
	_acceptor.close();
	_groupManager.reset();
	_uuidGenerator.reset();
	
	SPDLOG_INFO("Server stopped.");
}

void Server::AcceptClientAsync()
{
	if (!_isRunning)
		return;
	
	auto self(shared_from_this());
	auto newSession = std::make_shared<Session>(_strand, _rpcStrand, (*_uuidGenerator)());

	_acceptor.async_accept(newSession->GetSocket(), _strand.wrap([this, newSession](const boost::system::error_code& ec)
	{
		if (ec)
		{
			if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::connection_aborted)
			{
				SPDLOG_INFO("Accept operation aborted");
				return;
			}
			
			SPDLOG_ERROR("Accept failed: {}", ec.message());
			AcceptClientAsync();
			return;
		}

		AcceptClientAsync();
		
		boost::asio::post(_strand.wrap([this, newSession]() { InitSessionNetwork(newSession); }));
	}));
}

void Server::InitSessionNetwork(const std::shared_ptr<Session>& newSession) const
{
	if (!newSession->ExchangeUdpPort())
	{
		SPDLOG_ERROR("New Session failed to exchange UDP port");
		return;
	}

	if (!newSession->SendUuidToClient())
	{
		SPDLOG_ERROR("New Session failed to send UUID to client");
		return;
	}

	_groupManager->AddSession(newSession);
}
