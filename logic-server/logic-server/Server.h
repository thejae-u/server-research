#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <mutex>
#include <memory>
#include <vector>

#include "Base.h"
#include "GroupManager.h"
#include "NetworkData.pb.h"
#include "ContextManager.h"

using IoContext = boost::asio::io_context;
using namespace boost::asio::ip;
using namespace NetworkData;

class Session;
class LockstepGroup;

class Server final : public Base<Server>
{
public:
	Server(const std::shared_ptr<ContextManager>& mainCtxManager, const std::shared_ptr<ContextManager>& rpcCtxManager, tcp::acceptor& acceptor);

	void Start() override;
	void Stop() override;

private:
	std::shared_ptr<ContextManager> _mainCtxManager;
	std::shared_ptr<ContextManager> _rpcCtxManager;
	tcp::acceptor& _acceptor;

	std::unique_ptr<GroupManager> _groupManager;
	std::shared_ptr<random_generator> _uuidGenerator;

	bool _isRunning;

	void AcceptClientAsync();
	void InitSessionNetwork(const std::shared_ptr<Session>& newSession) const;
};
