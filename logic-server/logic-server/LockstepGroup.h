#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <set>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>

#include "NetworkData.pb.h"
using namespace NetworkData;
class Session;

class LockstepGroup : public std::enable_shared_from_this<LockstepGroup>
{
public:
    void CollectInput(std::unordered_map<boost::uuids::uuid, std::shared_ptr<RpcRequest>> rpcRequest);
    void ProcessStep();
    void Tick();

private:
    std::set<std::shared_ptr<Session>> _members;
    const std::uint32_t _fixedDeltaMs = 33;
    uint64_t _lastTickTime = 0;
    
    std::uint32_t _currentFrame = 0;
    std::unordered_map<std::uint32_t, std::unordered_map<boost::uuids::uuid, std::shared_ptr<RpcPacket>>> _inputBuffer;
    std::mutex _bufferMutex;

    boost::asio::steady_timer _timer;
    bool _isRunning = false;

    void ScheduleNextTick();
};