#pragma once

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <memory>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <queue>

#include "Base.h"
#include "Scheduler.h"
#include "NetworkData.pb.h"
#include "Utility.h"

using namespace NetworkData;

using IoContext = boost::asio::io_context;
using error_code = boost::system::error_code;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using boost::uuids::uuid;

class Server;
class LockstepGroup;
class Scheduler;
class ContextManager;
struct SSessionKey;
struct SSendPacket;

constexpr std::int64_t INVALID_RTT = -1;
constexpr std::size_t MAX_PACKET_SIZE = 65535;

class Session final : public Base<Session>
{
public:
    Session(const std::shared_ptr<ContextManager>& contextManager, const std::shared_ptr<ContextManager>& rpcContextManager);
    ~Session() override
    {
        if (_sessionInfo.uid() != "")
            spdlog::info("{} : session destoryed", _sessionInfo.uid());
        else
            spdlog::info("empty session destroyed");
    }

    void Start() override;
    void Stop() override;

    tcp::socket& GetSocket() const { return *_tcpSocketPtr; }
    void SetGroup(const std::shared_ptr<LockstepGroup>& groupPtr) { _lockstepGroupPtr = groupPtr; }

    bool ExchangeUdpPort(std::uint16_t udpPort);
    void AsyncExchangeUdpPortWork(std::uint16_t udpPort, std::function<void(bool success)> onComplete); // separate real logic to avoid long blocking

    bool ReceiveUserInfo();
    void AsyncReceiveUserInfo(std::function<void(bool success)> onComplete);

    bool ReceiveGroupInfo(std::shared_ptr<GroupDto>& groupInfo);
    void AysncReceiveGroupInfo(std::function<void(bool success, std::shared_ptr<GroupDto> groupInfo)> onComlete);

    bool IsValid() const { return _isConnected; }
    uuid GetSessionUuid() { return _toUuid(_sessionInfo.uid()); }

    void CollectInput(std::shared_ptr<RpcPacket> receivePacket);
    void EnqueueSendPackets(const std::list<std::shared_ptr<SSendPacket>> sendPackets);

public: // Callback Functions
    using StopCallback = std::function<void(const std::shared_ptr<Session>&)>;
    void SetStopCallbackByGroup(StopCallback stopCallback);
    void SetStopCallbackByServer(StopCallback stopCallback);

    using SessionInput = std::function<void(std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>>)>;
    void SetCollectInputAction(SessionInput inputAction);

    using SendDataByUdp = std::function<void(std::shared_ptr<std::pair<udp::endpoint, std::string>>)>;
    void SetSendDataByUdpAction(SendDataByUdp enqueueAction);

private: // internal private functions
    RpcPacket DequeueSendPacket();
    void SerializeRpcPacketAndEnqueueData();

    void SendPingPacket(CompletionHandler onComplete);
    void ProcessTcpRequest(const std::shared_ptr<RpcPacket> packet);

    void TcpAsyncWrite(const std::shared_ptr<std::string> data);

	void TcpAsyncReadSize();
    void TcpAsyncReadData(std::shared_ptr<std::vector<char>> dataBuffer);

    void UdpAsyncRead();
    void UdpAsyncWrite(std::shared_ptr<std::string> data);

    void UpdateOwnState(CompletionHandler onComplete);

private:
    using TcpSocket = tcp::socket;
    using UdpSocket = udp::socket;

    // uuid parser
    boost::uuids::string_generator _toUuid;

    // boost asio context
    std::shared_ptr<ContextManager> _normalCtxManager;
    std::shared_ptr<ContextManager> _rpcCtxManager;
    boost::asio::io_context::strand _normalPrivateStrand;
    boost::asio::io_context::strand _rpcPrivateStrand;

    // boost asio sockets
    std::shared_ptr<TcpSocket> _tcpSocketPtr;
    std::shared_ptr<UdpSocket> _udpSocketPtr;
    udp::endpoint _udpSendEp;

    // tcp net size cache
    std::uint32_t _tcpNetSize = 0;
    std::uint32_t _tcpDataSize = 0;

    // client connected state
    std::atomic<bool> _isConnected = false;

    // group
    std::shared_ptr<LockstepGroup> _lockstepGroupPtr;
    UserSimpleDto _sessionInfo;
    GroupDto _groupDto;

    // rtt timer
    std::shared_ptr<Scheduler> _pingTimer;
    const std::uint32_t _pingDelay = 1000;
    std::chrono::high_resolution_clock::time_point _pingTime;
    std::uint64_t _lastRtt;

    // update state timer
    std::shared_ptr<Scheduler> _updateTimer;
    const std::int32_t _updateTick = 1;

    // callback handlers
    StopCallback _onStopCallbackByGroup;
    StopCallback _onStopCallbackByServer;
    SessionInput _inputAction;
    SendDataByUdp _sendDataByUdp;

    // packet queue
    std::mutex _sendQueueMutex;
    std::queue<RpcPacket> _sendPacketQueue;

    // own state
    std::mutex _stateMutex;
    Utility::SGameState _gameState;
    std::mutex _updatePacketMutex;
    std::queue<RpcPacket> _updatePacketQueue;

private:
    std::shared_ptr<Scheduler> _testTimer;
    const std::uint32_t _delay = 500;
    void SendTestPacket(CompletionHandler onComplete);
};