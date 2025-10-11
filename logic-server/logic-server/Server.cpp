#include "Server.h"

#include "Session.h"
#include "LockstepGroup.h"

Server::Server(const std::shared_ptr<ContextManager>& mainCtxManager, const std::shared_ptr<ContextManager>& rpcCtxManager, tcp::acceptor& acceptor)
	: _mainCtxManager(mainCtxManager), _rpcCtxManager(rpcCtxManager), _acceptor(acceptor)
{
	_uuidGenerator = std::make_shared<random_generator>();
	_groupManager = std::make_unique<GroupManager>(_mainCtxManager, _uuidGenerator);
	_isRunning = false;
}

void Server::Start()
{
	_isRunning = true;
	AcceptClientAsync();
}

void Server::Stop()
{
	spdlog::info("server stopping...");

	_isRunning = false;
	_acceptor.close();
	_groupManager.reset();
	_uuidGenerator.reset();

	spdlog::info("server stopped.");
}

void Server::AcceptClientAsync()
{
	if (!_isRunning)
		return;

	auto self(shared_from_this());
	auto newSession = std::make_shared<Session>(_mainCtxManager, _rpcCtxManager, (*_uuidGenerator)());

	_acceptor.async_accept(newSession->GetSocket(), _mainCtxManager->GetStrand().wrap([this, newSession](const boost::system::error_code& ec) {
		if (ec)
		{
			if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::connection_aborted)
			{
				spdlog::info("accept operation aborted");
				return;
			}

			spdlog::error("accept failed : {}", ec.message());
			AcceptClientAsync();
			return;
		}

		AcceptClientAsync();

		boost::asio::post(_mainCtxManager->GetStrand().wrap([this, newSession]() { InitSessionNetwork(newSession); }));
		})
	);
}

void Server::InitSessionNetwork(const std::shared_ptr<Session>& newSession) const
{
	auto self(shared_from_this());
	newSession->AsyncExchangeUdpPortWork([self, newSession](bool success) {
		if (!success)
		{
			spdlog::error("new session failed to excahnge UDP port");
			newSession->Stop();
			return;
		}

		newSession->AsyncSendUuidToClientWork([self, newSession](bool uuidSuccess) {
			if (!uuidSuccess)
			{
				spdlog::error("new session failed to send uuid to client");
				newSession->Stop();
				return;
			}

			self->_groupManager->AddSession(newSession);
			});
		}
	);
}