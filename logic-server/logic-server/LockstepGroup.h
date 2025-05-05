#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <set>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using namespace boost::uuids;
using namespace NetworkData;
class Session;

class LockstepGroup : public std::enable_shared_from_this<LockstepGroup>
{
public:
    LockstepGroup(const io_context::strand& strand, uuid groupId);
    ~LockstepGroup() = default;

    void Start();
    void AddMember(std::shared_ptr<Session> newSession);
    void RemoveMember(std::shared_ptr<Session> session);
    void CollectInput(std::unordered_map<uuid, std::shared_ptr<RpcRequest>> rpcRequest);
    void ProcessStep();
    void Tick();

    // Operator will be used for sorting
    /*bool operator<(const LockstepGroup& rhs) const
    {
        if (rhs._groupMaxDelayMs == -1 && this->_groupMaxDelayMs == -1)
            return false;
        
        return this->_groupMaxDelayMs < rhs._groupMaxDelayMs;
    }*/

private:
    uuid _groupId;
    std::set<std::shared_ptr<Session>> _members;
    std::mutex _memberMutex;

    short _groupMaxDelayMs;
    
    const std::uint32_t _fixedDeltaMs = 33;
    uint64_t _lastTickTime = 0;
    
    std::uint32_t _currentFrame = 0;
    std::unordered_map<std::uint32_t, std::unordered_map<uuid, std::shared_ptr<RpcPacket>>> _inputBuffer;
    std::mutex _bufferMutex;

    boost::asio::steady_timer _timer;
    bool _isRunning = false;

    void ScheduleNextTick();
};