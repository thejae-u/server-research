#include "Server.h"
#include "Session.h"
#include "LockstepGroup.h"

#include <iostream>

Server::Server(const IoContext::strand& strand, const IoContext::strand& rpcStrand, tcp::acceptor& acceptor) : _strand(strand), _rpcStrand(rpcStrand), _acceptor(acceptor)
{
	_uuidGenerator = std::make_shared<random_generator>();
	_groupManager = std::make_unique<GroupManager>(strand, _uuidGenerator);
}

void Server::AcceptClientAsync()
{
	auto self(shared_from_this());
	auto newSession = std::make_shared<Session>(_strand, _rpcStrand, (*_uuidGenerator)());

	_acceptor.async_accept(newSession->GetSocket(), _strand.wrap([this, newSession](const boost::system::error_code& ec)
	{
		if (ec)
		{
			SPDLOG_ERROR("Accept failed: {}", ec.message());
			AcceptClientAsync();
			return;
		}

		boost::asio::post(_strand.wrap([this, newSession]() { InitSessionNetwork(newSession); }));
		AcceptClientAsync();
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
