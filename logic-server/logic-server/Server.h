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
#include <functional>
#include <queue>
#include <unordered_map>

#include "Base.h"
#include "GroupManager.h"
#include "Utility.h"
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
    using UdpSocket = udp::socket;

	std::shared_ptr<ContextManager> _normalCtxManager;
	std::shared_ptr<ContextManager> _rpcCtxManager;
	tcp::acceptor& _acceptor;
    boost::uuids::string_generator _toUuid;

	boost::asio::io_context::strand _normalPrivateStrand;
	boost::asio::io_context::strand _rpcPrivateStrand;

    std::shared_ptr<UdpSocket> _udpSocket;
    std::uint16_t _allocatedUdpPort;

    std::mutex _sendDataQueueMutex;
    std::queue<std::shared_ptr<std::pair<udp::endpoint, std::string>>> _sendDataQueue;

	std::shared_ptr<GroupManager> _groupManager;
	std::atomic<bool> _isRunning;
    std::atomic<std::size_t> _udpReceiveCount;

    std::unordered_map<uuid, std::shared_ptr<Session>> _sessions;
    std::mutex _sessionsMutex;

    const std::size_t _maxPacketSize = 65535;

	void AcceptClientAsync();
	void InitSessionNetwork(const std::shared_ptr<Session>& newSession);

	// Udp Socket Functions
    void AsyncReceiveUdpData();
    void EnqueueSendData(std::shared_ptr<std::pair<udp::endpoint, std::string>> sendData);
    void AsyncSendUdpData();

    void AddSession(std::shared_ptr<Session> newSession);
    void RemoveSession(uuid sessionId);
};
