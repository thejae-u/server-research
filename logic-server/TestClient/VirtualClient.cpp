/*

Made With GEMINI CLI

*/

#include "VirtualClient.h"
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <boost/uuid/uuid_io.hpp> // Required for boost::uuids::to_string

// Boost UUID generator
boost::uuids::random_generator uuid_gen;

VirtualClient::VirtualClient(boost::asio::io_context& io_context, int id, std::string serverIp, int serverPort, std::string groupId, int groupIndex, int indexInGroup)
    : _io_context(io_context), _id(id), _serverIp(serverIp), _serverPort(serverPort), _groupId(std::move(groupId)),
      _groupIndex(groupIndex), _indexInGroup(indexInGroup),
      _tcpSocket(io_context), _udpSocket(io_context), _pingTimer(io_context), _randomUdpTimer(io_context), _randomAtkTimer(io_context), _throughputTimer(io_context)
{
    // Generate UUID using boost::uuid
    _uuid = boost::uuids::to_string(uuid_gen());
    
    // Set Display Names
    _displayGroupId = std::format("group_{}", _groupIndex);
    _displayUserId = std::format("group_{}_{}", _groupIndex, _indexInGroup);
}

VirtualClient::~VirtualClient()
{
    Stop();
}

void VirtualClient::Start(std::function<void(SHistory history)> enqueueHistory)
{
    if (_state != ClientState::Disconnected) return;

    _state = ClientState::Connecting;
    DoConnect();

    _enqueueHistory = std::move(enqueueHistory);
    
    _lastThroughputTime = std::chrono::steady_clock::now();
    DoThroughputTick();
}

void VirtualClient::Stop()
{
    _state = ClientState::Disconnected;
    _isRandomUdpActive = false;
    _isRandomAtkActive = false;
    boost::system::error_code ec;
    _tcpSocket.close(ec);
    _udpSocket.close(ec);
    _pingTimer.cancel();
    _randomUdpTimer.cancel();
    _randomAtkTimer.cancel();
    _throughputTimer.cancel();
}

void VirtualClient::DoThroughputTick()
{
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - _lastThroughputTime;
    if (elapsed.count() >= 1.0)
    {
        std::lock_guard<std::mutex> lock(_statsMutex);
        _stats.txBps = (_bytesSentSinceLastTick * 8.0) / elapsed.count();
        _stats.rxBps = (_bytesReceivedSinceLastTick * 8.0) / elapsed.count();
        
        _bytesSentSinceLastTick = 0;
        _bytesReceivedSinceLastTick = 0;
        _lastThroughputTime = now;
    }

    _throughputTimer.expires_after(std::chrono::milliseconds(1000));
    auto self(shared_from_this());
    _throughputTimer.async_wait([self](const boost::system::error_code& ec)
    {
        if (!ec)
        {
            self->DoThroughputTick();
        }
    });
}

ClientStats VirtualClient::GetStats() const
{
    std::lock_guard<std::mutex> lock(_statsMutex);
    return _stats;
}

void VirtualClient::StartRandomUdpTraffic(int intervalMs)
{
    // Packet size is ignored for Move Simulation, using interval for tick rate
    _intervalMs = intervalMs;
    _isRandomUdpActive = true;
    
    // Initialize Simulation State
    _simState = SimulationState::Idle;
    _simStateStartTime = std::chrono::steady_clock::now();
    _simDurationMs = 0; // Start immediately
    _simX = 0; _simY = 0; _simZ = 0;
    
    DoSimulationLoop();
}

void VirtualClient::StopRandomUdpTraffic()
{
    _isRandomUdpActive = false;
    _randomUdpTimer.cancel();
}

void VirtualClient::StartRandomAtkTraffic(int intervalMs, std::vector<std::string> targetList)
{
    _atkIntervalMs = intervalMs;
    _targetList = targetList;

    for (int i = 0; i < targetList.size(); ++i)
    {
        if (targetList[i] == _uuid)
        {
            _ownIndex = i;
            break;
        }
    }

    _isRandomAtkActive = true;

    DoAtkSimulationLoop();
}

void VirtualClient::StopRandomAtkTraffic()
{
    _isRandomAtkActive = false;
    _randomAtkTimer.cancel();
}

void VirtualClient::DoAtkSimulationLoop()
{
    if (!_isRandomAtkActive || _state != ClientState::Connected) return;

    SHistory history;
    RpcPacket packet;
    packet.set_uid(_uuid);
    packet.set_method(RpcMethod::Atk);

    history.groupId = _displayGroupId;
    history.userId = _displayUserId;
    history.method = "atk";

    // Set Timestamp
    auto sysNow = std::chrono::system_clock::now();
    auto timestamp = packet.mutable_timestamp();
    timestamp->set_seconds(std::chrono::duration_cast<std::chrono::seconds>(sysNow.time_since_epoch()).count());
    timestamp->set_nanos(0);

    history.time = sysNow;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, _targetList.size() - 1);

    int targetIdx = dis(gen);
    while (targetIdx == _ownIndex)
    {
        targetIdx = dis(gen);
    }

    // Create AtkData
    AtkData atkData;
    atkData.set_victim(_targetList[targetIdx]);
    std::cout << _displayUserId << " trying attack " << _targetList[targetIdx] << "\n";
    history.data = std::format("{} atk {}", _displayUserId, _targetList[targetIdx]);
    
    // Random Damage
    std::uniform_int_distribution<> distDmg(1, 10);
    atkData.set_dmg(distDmg(gen));
    packet.set_data(atkData.SerializeAsString());

    SendUdpPacket(packet);

    if (_enqueueHistory)
        _enqueueHistory(history); // enqueue packet history

    _randomAtkTimer.expires_after(std::chrono::milliseconds(_atkIntervalMs));
    auto self(shared_from_this());
    _randomAtkTimer.async_wait([self](const boost::system::error_code& ec)
    {
        if (!ec)
        {
            self->DoAtkSimulationLoop();
        }
    });
}

void VirtualClient::DoSimulationLoop()
{
    if (!_isRandomUdpActive || _state != ClientState::Connected) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - _simStateStartTime).count();

    SHistory history;
    RpcPacket packet;
    packet.set_uid(_uuid);
    history.groupId = _displayGroupId;
    history.userId = _displayUserId;
    
    // Set Timestamp
    auto sysNow = std::chrono::system_clock::now();
    auto timestamp = packet.mutable_timestamp();
    timestamp->set_seconds(std::chrono::duration_cast<std::chrono::seconds>(sysNow.time_since_epoch()).count());
    timestamp->set_nanos(0);
    history.time = sysNow;

    if (_simState == SimulationState::Idle)
    {
        if (elapsedMs >= _simDurationMs)
        {
            // Switch to Moving
            _simState = SimulationState::Moving;
            _simStateStartTime = now;
            
            // Random Duration (1~3 sec)
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> distDur(1000, 3000);
            _simDurationMs = distDur(gen);

            // Random Direction
            std::uniform_real_distribution<> distDir(-1.0, 1.0);
            _simHorizontal = static_cast<float>(distDir(gen));
            _simVertical = static_cast<float>(distDir(gen));
            
            // Normalize direction (simple check)
            float len = std::sqrt(_simHorizontal*_simHorizontal + _simVertical*_simVertical);
            if (len > 0.01f) { _simHorizontal /= len; _simVertical /= len; }

            // Send MoveStart
            packet.set_method(MoveStart);
            history.method = "move start";

            MoveData moveData;
            moveData.set_x(_simX);
            moveData.set_y(_simY);
            moveData.set_z(_simZ);
            moveData.set_horizontal(_simHorizontal);
            moveData.set_vertical(_simVertical);
            moveData.set_speed(5.0f); // Fixed speed
            packet.set_data(moveData.SerializeAsString());
            history.data = std::format("x{} y{} z{}, hor{}, ver{}, spd{}", 
                std::to_string(_simX), std::to_string(_simY), std::to_string(_simZ), 
                std::to_string(_simHorizontal), std::to_string(_simVertical), 5.0f);
            
            SendUdpPacket(packet);

            if (_enqueueHistory)
                _enqueueHistory(history);
        }
    }
    else if (_simState == SimulationState::Moving)
    {
        // Update Position (Dead Reckoning)
        float dt = _intervalMs / 1000.0f;
        float speed = 5.0f;
        _simX += _simHorizontal * speed * dt;
        _simZ += _simVertical * speed * dt; // Assuming Y is up, so Z is forward/backward

        // Check if time to stop
        if (elapsedMs >= _simDurationMs)
        {
            // Switch to Idle
            _simState = SimulationState::Idle;
            _simStateStartTime = now;
            
            // Random Duration (0.5~2 sec)
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> distDur(500, 2000);
            _simDurationMs = distDur(gen);

            // Send MoveStop
            packet.set_method(MoveStop);
            history.method = "move stop";

            MoveData moveData;
            moveData.set_x(_simX);
            moveData.set_y(_simY);
            moveData.set_z(_simZ);
            moveData.set_horizontal(0);
            moveData.set_vertical(0);
            moveData.set_speed(0);
            packet.set_data(moveData.SerializeAsString());
            history.data = std::format("x{} y{} z{}, hor{}, ver{}, spd{}", 
                std::to_string(_simX), std::to_string(_simY), std::to_string(_simZ)
                , std::to_string(_simHorizontal), std::to_string(_simVertical), 0);

            SendUdpPacket(packet);

            if (_enqueueHistory)
                _enqueueHistory(history);
        }
        else
        {
            // Keep Moving
            packet.set_method(Move);
            history.method = "move";

            MoveData moveData;
            moveData.set_x(_simX);
            moveData.set_y(_simY);
            moveData.set_z(_simZ);
            moveData.set_horizontal(_simHorizontal);
            moveData.set_vertical(_simVertical);
            moveData.set_speed(speed);
            packet.set_data(moveData.SerializeAsString());
            history.data = std::format("x{} y{} z{}, hor{}, ver{}, spd{}", 
                std::to_string(_simX), std::to_string(_simY), std::to_string(_simZ)
                , std::to_string(_simHorizontal), std::to_string(_simVertical), std::to_string(speed));

            SendUdpPacket(packet);
            
            if (_enqueueHistory)
                _enqueueHistory(history);
        }
    }

    _randomUdpTimer.expires_after(std::chrono::milliseconds(_intervalMs));
    auto self(shared_from_this());
    _randomUdpTimer.async_wait([self](const boost::system::error_code& ec)
    {
        if (!ec)
        {
            self->DoSimulationLoop();
        }
    });
}

void VirtualClient::DoConnect()
{
    auto self(shared_from_this());
    tcp::resolver resolver(_io_context);
    auto endpoints = resolver.resolve(_serverIp, std::to_string(_serverPort));

    boost::asio::async_connect(_tcpSocket, endpoints, [self](const boost::system::error_code& ec, tcp::endpoint)
    {
        if (!ec)
        {
            self->_state = ClientState::Handshake_Udp;
            self->DoReadHeader();
        }
        else if(ec == boost::asio::error::connection_refused)
        {
            self->_state = ClientState::Connection_Failed;
        }
        else
        {
            self->_state = ClientState::Error;
        }
    });
}

void VirtualClient::DoReadHeader()
{
    auto self(shared_from_this());
    boost::asio::async_read(_tcpSocket, boost::asio::buffer(&_tcpHeader, 4),
        [self](const boost::system::error_code& ec, std::size_t)
    {
        if (!ec)
        {
            uint32_t bodySize = ntohl(self->_tcpHeader);
            self->DoReadBody(bodySize);
        }
        else
        {
            self->Stop();
        }
    });
}

void VirtualClient::DoReadBody(uint32_t size)
{
    auto self(shared_from_this());
    _tcpBodyBuffer.resize(size);

    boost::asio::async_read(_tcpSocket, boost::asio::buffer(_tcpBodyBuffer),
        [self, size](const boost::system::error_code& ec, std::size_t)
    {
        if (!ec)
        {
            self->_bytesReceivedSinceLastTick += (size + 4); // Body + Header
            RpcPacket packet;
            if (packet.ParseFromArray(self->_tcpBodyBuffer.data(), static_cast<int>(self->_tcpBodyBuffer.size())))
            {
                {
                    std::lock_guard<std::mutex> lock(self->_statsMutex);
                    //self->_stats.rxPackets++;
                }
                self->HandleTcpPacket(packet);
            }
            self->DoReadHeader();
        }
        else
        {
            self->Stop();
        }
    });
}

void VirtualClient::HandleTcpPacket(const RpcPacket& packet)
{
    switch (packet.method())
    {
        case UDP_PORT:
            HandleUdpPortExchange(packet);
            break;
        case USER_INFO:
            HandleUserInfoExchange(packet);
            break;
        case GROUP_INFO:
            HandleGroupInfoExchange(packet);
            break;
        case PING:
        {
            // Server sent PING, we reply with PONG
            RpcPacket pongPacket;
            pongPacket.set_method(PONG);
            SendTcpPacket(pongPacket);
            break;
        }
        case LAST_RTT:
        {
            // Server sent calculated RTT
            try
            {
                int rtt = std::stoi(packet.data());
                std::lock_guard<std::mutex> lock(_statsMutex);
                _stats.rttMs = rtt;
                
                // Update Statistics
                if (rtt < _stats.minRtt) _stats.minRtt = rtt;
                if (rtt > _stats.maxRtt) _stats.maxRtt = rtt;
                _stats.totalRtt += rtt;
                _stats.rttCount++;

                SHistory history = { _displayGroupId, _displayUserId, "rtt", std::to_string(rtt), std::chrono::system_clock::now() };

                /*if (_enqueueHistory)
                    _enqueueHistory(history);*/
            }
            catch (...) {}
            break;
        }
        case CLIENT_GAME_INFO:
            HandleGameInfoPacket(packet);
            break;
        default:
            break;
    }
}

void VirtualClient::HandleGameInfoPacket(const RpcPacket& packet)
{
    GameData gameData;
    if (gameData.ParseFromString(packet.data()))
    {
        if (gameData.has_position())
        {
            _serverX = gameData.position().x();
            _serverZ = gameData.position().z();
        }
    }
}

void VirtualClient::HandleUdpPortExchange(const RpcPacket& packet)
{
    // 1. Get Server UDP Port
    std::uint16_t serverUdpPortNet;
    if (packet.data().size() >= sizeof(serverUdpPortNet))
    {
        std::memcpy(&serverUdpPortNet, packet.data().data(), sizeof(serverUdpPortNet));
        std::uint16_t serverUdpPort = ntohs(serverUdpPortNet);

        _serverUdpEndpoint = udp::endpoint(_tcpSocket.remote_endpoint().address(), serverUdpPort);

        // 2. Setup Local UDP
        SetupUdp();

        // 3. Send Local UDP Port
        std::uint16_t localPort = _udpSocket.local_endpoint().port();
        std::uint16_t localPortNet = htons(localPort);

        RpcPacket response;
        response.set_method(UDP_PORT);
        response.set_data(std::string(reinterpret_cast<char*>(&localPortNet), sizeof(localPortNet)));
        SendTcpPacket(response);

        _state = ClientState::Handshake_UserInfo;
    }
}

void VirtualClient::HandleUserInfoExchange(const RpcPacket& packet)
{
    RpcPacket response;
    response.set_method(USER_INFO);
    response.set_uid(_uuid);
    SendTcpPacket(response);

    _state = ClientState::Handshake_GroupInfo;
}

void VirtualClient::HandleGroupInfoExchange(const RpcPacket& packet)
{
    // Send Group Info
    GroupDto groupDto;
    groupDto.set_groupid(_groupId);

    RpcPacket response;
    response.set_method(GROUP_INFO);
    response.set_data(groupDto.SerializeAsString());
    SendTcpPacket(response);

    _state = ClientState::Connected;

    // Start UDP receive loop
    DoUdpReceive();
}

void VirtualClient::SetupUdp()
{
    boost::system::error_code ec;
    _udpSocket.open(udp::v4(), ec);
    _udpSocket.bind(udp::endpoint(udp::v4(), 0), ec);
}

void VirtualClient::DoUdpReceive()
{
    auto self(shared_from_this());
    _udpSocket.async_receive_from(boost::asio::buffer(_udpBuffer), _serverUdpEndpoint,
        [self](const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        if (!ec && bytes_transferred > 2)
        {
            self->_bytesReceivedSinceLastTick += bytes_transferred;
            // Parse UDP Packet (Size 2 bytes + Body)
            uint16_t payloadSize;
            std::memcpy(&payloadSize, self->_udpBuffer, 2);
            payloadSize = ntohs(payloadSize);

            if (payloadSize > 0 && payloadSize + 2 <= bytes_transferred)
            {
                RpcPacket packet;
                if (packet.ParseFromArray(self->_udpBuffer + 2, payloadSize))
                {
                    self->HandleUdpPacket(packet);
                }
            }

            self->DoUdpReceive();
        }
    });
}

void VirtualClient::HandleUdpPacket(const RpcPacket& packet)
{
    {
        std::lock_guard<std::mutex> lock(_statsMutex);
        _stats.rxPackets++;
    }

    switch (packet.method())
    {
    case RpcMethod::MoveStart:
    case RpcMethod::Move:
    case RpcMethod::MoveStop:
    {
        MoveData moveData;
        if (moveData.ParseFromString(packet.data()))
        {
            long long currentTimestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::seconds(packet.timestamp().seconds())
            ).count() + packet.timestamp().nanos();

            if (packet.uid() == _uuid)
            {
                // Only update if packet is newer
                if (currentTimestamp > _lastServerTimestamp)
                {
                    _lastServerTimestamp = currentTimestamp;
                    _serverX = moveData.x();
                    _serverZ = moveData.z();
                }
            }
            else
            {
                std::lock_guard<std::mutex> lock(_remoteUsersMutex);
                auto& user = _remoteUsers[packet.uid()];
                
                // Only update if packet is newer
                if (currentTimestamp > user.lastTimestamp)
                {
                    user.lastTimestamp = currentTimestamp;
                    user.uuid = packet.uid();
                    user.x = moveData.x();
                    user.y = moveData.y();
                    user.z = moveData.z();
                }
            }
        }
        break;
    }
    case RpcMethod::Atk:
    {
        AtkData atkData;
        if (atkData.ParseFromString(packet.data()))
        {
            SHistory history;
            history.groupId = _displayGroupId;
            history.userId = (packet.uid() == _uuid) ? _displayUserId : packet.uid();
            history.method = "atk";
            history.data = std::format("atk -> {} (dmg {})", atkData.victim(), atkData.dmg());
            history.time = std::chrono::system_clock::now();

            if (_enqueueHistory)
                _enqueueHistory(history);
        }
        break;
    }
    case RpcMethod::Hit:
    {
        AtkData atkData;
        if (atkData.ParseFromString(packet.data()))
        {
            std::string victimId = atkData.victim();
            int dmg = atkData.dmg();

            // Update HP
            if (victimId == _uuid)
            {
                // My HP handling (if needed)
            }
            else
            {
                std::lock_guard<std::mutex> lock(_remoteUsersMutex);
                if (_remoteUsers.find(victimId) != _remoteUsers.end())
                {
                    _remoteUsers[victimId].hp -= dmg;
                }
            }

            SHistory history;
            history.groupId = _displayGroupId;
            history.userId = (packet.uid() == _uuid) ? _displayUserId : packet.uid(); // Attacker or Server(if system hit)
            history.method = "hit";
            history.data = std::format("hit by {} (dmg {})", packet.uid(), dmg);
            history.time = std::chrono::system_clock::now();

            if (_enqueueHistory)
                _enqueueHistory(history);
        }
        break;
    }
    }
}

void VirtualClient::SendTcpPacket(const RpcPacket& packet)
{
    std::string data = packet.SerializeAsString();
    uint32_t size = static_cast<uint32_t>(data.size());
    uint32_t netSize = htonl(size);

    auto buffer = std::make_shared<std::vector<char>>(sizeof(netSize) + size);
    std::memcpy(buffer->data(), &netSize, sizeof(netSize));
    std::memcpy(buffer->data() + sizeof(netSize), data.data(), size);

    auto self(shared_from_this());
    boost::asio::async_write(_tcpSocket, boost::asio::buffer(*buffer),
        [self, buffer, netSize, size](const boost::system::error_code& ec, std::size_t)
    {
        if (!ec)
        {
            self->_bytesSentSinceLastTick += (sizeof(netSize) + size);
        }
    });
}

void VirtualClient::SendUdpPacket(const RpcPacket& packet)
{
    if (_state != ClientState::Connected) return;

    std::string data = packet.SerializeAsString();
    if (data.size() > 65000) return; // UDP limit safety

    uint16_t size = static_cast<uint16_t>(data.size());
    uint16_t netSize = htons(size);

    // Buffer: [Length(2)][Protobuf Payload]
    auto buffer = std::make_shared<std::vector<char>>(2 + size);
    std::memcpy(buffer->data(), &netSize, 2);
    std::memcpy(buffer->data() + 2, data.data(), size);

    auto self(shared_from_this());
    _udpSocket.async_send_to(boost::asio::buffer(*buffer), _serverUdpEndpoint,
        [self, buffer, netSize, size](const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        if (!ec)
        {
            self->_bytesSentSinceLastTick += (2 + size);
            std::lock_guard<std::mutex> lock(self->_statsMutex);
            self->_stats.txPackets++;
        }
    });
}