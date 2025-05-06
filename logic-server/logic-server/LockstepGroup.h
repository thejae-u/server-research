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
#include <boost/uuid/uuid_hash.hpp>

#include "NetworkData.pb.h"

using io_context = boost::asio::io_context;
using namespace boost::uuids;
using namespace NetworkData;
class Session;

struct SSessionKey
{
    // total 24byte
    std::size_t frame; // 8byte
    uuid guid; // 16byte
    
    bool operator==(const SSessionKey& other) const
    {
        return guid == other.guid && frame == other.frame;
    }
};

// SSessionKey에 대한 std::hash 특수화
namespace std {
    template <>
    struct hash<SSessionKey> {
        std::size_t operator()(const SSessionKey& key) const noexcept
        {
            const std::size_t h1 = std::hash<std::size_t>()(key.frame);
            const std::size_t h2 = std::hash<uuid>()(key.guid);
            return h1 ^ (h2 << 1); // 해시 결합
        }
    };
}

class LockstepGroup : public std::enable_shared_from_this<LockstepGroup>
{
public:
    LockstepGroup(const io_context::strand& strand, uuid groupId);
    ~LockstepGroup() = default;

    void Start();
    void AddMember(const std::shared_ptr<Session>& newSession);
    void RemoveMember(const std::shared_ptr<Session>& session);
    void CollectInput(std::unordered_map<uuid, std::shared_ptr<RpcRequest>> rpcRequest);
    void ProcessStep();
    void Tick();

    bool IsFull() const { return _members.size() == _maxSessionCount; }

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
    const std::size_t _maxSessionCount = 10;
    std::mutex _memberMutex;

    short _groupMaxDelayMs;
    
    const std::size_t _fixedDeltaMs = 33;
    uint64_t _lastTickTime = 0;
    
    std::size_t _currentBucket = 0;

    // input buffer by bucket frame
    std::unordered_map<std::size_t/*bucket frame*/, std::unordered_map<SSessionKey, std::shared_ptr<RpcPacket>>> _inputBuffer;
    
    std::mutex _bufferMutex;

    boost::asio::steady_timer _timer;
    bool _isRunning = false;

    void ScheduleNextTick();
};