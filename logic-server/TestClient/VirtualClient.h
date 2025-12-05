/*

Made with GEMINI CLI

*/

#pragma once

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>          // Boost.UUID library
#include <boost/uuid/uuid_generators.hpp> // For random_generator
#include <boost/uuid/uuid_io.hpp>       // For to_string
#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <format>
#include "../logic-server/NetworkData.pb.h"

using namespace NetworkData;

using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using tp = std::chrono::system_clock::time_point;

enum class ClientState
{
    Disconnected,
    Connecting,
    Handshake_Udp,
    Handshake_UserInfo,
    Handshake_GroupInfo,
    Connected,
    Connection_Failed,
    Error
};

static const char* StateToString(ClientState state)
{
    switch (state)
    {
    case ClientState::Disconnected: return "Disconnected";
    case ClientState::Connecting: return "Connecting";
    case ClientState::Handshake_Udp: return "Handshake_Udp";
    case ClientState::Handshake_UserInfo: return "Handshake_UserInfo";
    case ClientState::Handshake_GroupInfo: return "Handshake_GroupInfo";
    case ClientState::Connected: return "Connected";
    case ClientState::Connection_Failed: return "Connection_Failed";
    default: return "Error";
    }
}

struct ClientStats
{
    int rttMs = 0;
    int minRtt = INT_MAX; // INT_MAX
    int maxRtt = 0;
    long long totalRtt = 0;
    long long rttCount = 0;

    int tickGap = 0;
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t droppedPackets = 0;
    
    uint64_t txBytes = 0;
    uint64_t rxBytes = 0;
    double txBps = 0.0;
    double rxBps = 0.0;

    double GetAvgRtt() const 
    {
        return rttCount > 0 ? (double)totalRtt / rttCount : 0.0;
    }
};

// Packet history
struct SHistory
{
    std::string groupId;
    std::string userId;
    std::string method;
    std::string data;
    tp time;
};

struct RemoteUser
{
    std::string uuid;
    float x = 0, y = 0, z = 0;
    float hp = 100.0f;
    long long lastTimestamp = 0; // For packet ordering check
    // 추가 상태 정보가 필요하면 여기에 확장
};

class VirtualClient : public std::enable_shared_from_this<VirtualClient>
{
public:
    VirtualClient(boost::asio::io_context& io_context, int id, std::string serverIp, int serverPort, std::string groupId, int groupIndex, int indexInGroup);
    ~VirtualClient();

    void Start(std::function<void(SHistory history)> enqueueHistory);
    void Stop();

    // Called from UI thread to get data for visualization
    ClientState GetState() const { return _state; }
    const char* GetStateString()  const { return StateToString(_state); }
    ClientStats GetStats() const;
    int GetId() const { return _id; }
    std::string GetUuid() const { return _uuid; }
    std::string GetGroupId() const { return _groupId; }
    
    // Display Names
    std::string GetDisplayGroupId() const { return _displayGroupId; }
    std::string GetDisplayUserId() const { return _displayUserId; }
    int GetGroupIndex() const { return _groupIndex; }

    std::pair<float, float> GetSimPosition() const { return { _simX.load(), _simZ.load() }; }
    std::pair<float, float> GetServerPosition() const { return { _serverX.load(), _serverZ.load() }; }
    
    std::unordered_map<std::string, RemoteUser> GetRemoteUsers() const 
    {
        std::lock_guard<std::mutex> lock(_remoteUsersMutex);
        return _remoteUsers;
    }

private:
    void DoConnect();
    
    // TCP Handlers
    void DoReadHeader();
    void DoReadBody(uint32_t size);
    void HandleTcpPacket(const RpcPacket& packet);
    void HandleGameInfoPacket(const RpcPacket& packet);

    // Handshake Handlers
    void HandleUdpPortExchange(const RpcPacket& packet);
    void HandleUserInfoExchange(const RpcPacket& packet);
    void HandleGroupInfoExchange(const RpcPacket& packet);

    // UDP Handlers
    void SetupUdp();
    void DoUdpReceive();
    void HandleUdpPacket(const RpcPacket& packet);

    // Helpers
    void SendTcpPacket(const RpcPacket& packet);
    void SendUdpPacket(const RpcPacket& packet);

    // Random UDP Traffic (Move Simulation)
    void DoSimulationLoop();
    // Random UDP Traffic (Attack Simulation)
    void DoAtkSimulationLoop();
    
    // Throughput Measurement
    void DoThroughputTick();

public:
    void StartRandomUdpTraffic(int intervalMs);
    void StopRandomUdpTraffic();

    void StartRandomAtkTraffic(int intervalMs, std::vector<std::string> targetList);
    void StopRandomAtkTraffic();

private:
    boost::asio::io_context& _io_context;
    int _id;
    std::string _serverIp;
    int _serverPort;
    
    tcp::socket _tcpSocket;
    udp::socket _udpSocket;
    udp::endpoint _serverUdpEndpoint;

    std::atomic<ClientState> _state{ ClientState::Disconnected };
    mutable std::mutex _statsMutex;
    ClientStats _stats;
    
    std::string _uuid;
    std::string _groupId = "11111111-1111-1111-1111-111111111111"; // Default test group UUID

    // Display Info
    int _groupIndex = 0;
    int _indexInGroup = 0;
    std::string _displayGroupId;
    std::string _displayUserId;

    // Buffers
    uint32_t _tcpHeader; // 4 bytes
    std::vector<char> _tcpBodyBuffer;
    
    char _udpBuffer[4096];
    
    // Ping measurement
    std::chrono::steady_clock::time_point _lastPingTime;
    boost::asio::steady_timer _pingTimer;
    
    // Throughput Measurement
    std::chrono::steady_clock::time_point _lastThroughputTime;
    uint64_t _bytesSentSinceLastTick = 0;
    uint64_t _bytesReceivedSinceLastTick = 0;
    boost::asio::steady_timer _throughputTimer;

    // Random UDP Traffic (Move Simulation)
    bool _isRandomUdpActive = false;
    int _intervalMs = 100;
    boost::asio::steady_timer _randomUdpTimer;
    
    enum class SimulationState { Idle, Moving };
    SimulationState _simState = SimulationState::Idle;
    std::atomic<float> _simX{ 0.0f };
    std::atomic<float> _simY{ 0.0f };
    std::atomic<float> _simZ{ 0.0f };
    float _simVertical = 0.0f;
    float _simHorizontal = 0.0f;
    std::chrono::steady_clock::time_point _simStateStartTime;
    int _simDurationMs = 0; // Duration for current state

    // Server State (Replicated)
    std::atomic<float> _serverX{ 0.0f };
    std::atomic<float> _serverZ{ 0.0f };
    std::atomic<long long> _lastServerTimestamp{ 0 }; // To prevent jitter from out-of-order packets

    mutable std::mutex _remoteUsersMutex;
    std::unordered_map<std::string, RemoteUser> _remoteUsers;

    // Random UDP Traffic (Attack Simulation)
    bool _isRandomAtkActive = false;
    int _atkIntervalMs = 1000;
    std::vector<std::string> _targetList;
    int _ownIndex = -1;
    boost::asio::steady_timer _randomAtkTimer;

    // history enqueue function
    std::function<void(SHistory)> _enqueueHistory;
};