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
#include "../logic-server/NetworkData.pb.h"

using namespace NetworkData;

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

enum class ClientState
{
    Disconnected,
    Connecting,
    Handshake_Udp,
    Handshake_UserInfo,
    Handshake_GroupInfo,
    Connected,
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
    default: return "Error";
    }
}

struct ClientStats
{
    int rttMs = 0;
    int tickGap = 0;
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t droppedPackets = 0;
};

class VirtualClient : public std::enable_shared_from_this<VirtualClient>
{
public:
    VirtualClient(boost::asio::io_context& io_context, int id, std::string serverIp, int serverPort, std::string groupId);
    ~VirtualClient();

    void Start();
    void Stop();

    // Called from UI thread to get data for visualization
    ClientState GetState() const { return _state; }
    const char* GetStateString()  const { return StateToString(_state); }
    ClientStats GetStats() const;
    int GetId() const { return _id; }
    std::string GetUuid() const { return _uuid; }
    std::string GetGroupId() const { return _groupId; }
    std::pair<float, float> GetSimPosition() const { return { _simX.load(), _simZ.load() }; }
    std::pair<float, float> GetServerPosition() const { return { _serverX.load(), _serverZ.load() }; }

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

public:
    void StartRandomUdpTraffic(int intervalMs);
    void StopRandomUdpTraffic();

    void StartRandomAtkTraffic(int intervalMs, std::string targetUuid);
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

    // Buffers
    uint32_t _tcpHeader; // 4 bytes
    std::vector<char> _tcpBodyBuffer;
    
    char _udpBuffer[4096];
    
    // Ping measurement
    std::chrono::steady_clock::time_point _lastPingTime;
    boost::asio::steady_timer _pingTimer;

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

    // Random UDP Traffic (Attack Simulation)
    bool _isRandomAtkActive = false;
    int _atkIntervalMs = 1000;
    std::string _targetUuid;
    boost::asio::steady_timer _randomAtkTimer;
};