#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <set>
#include <functional>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_hash.hpp>

#include "NetworkData.pb.h"

using IoContext = boost::asio::io_context;
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
    LockstepGroup(const IoContext::strand& strand, uuid groupId, std::uint64_t groupRttKey, std::size_t groupNumber);
    ~LockstepGroup() = default;

    using NotifyEmptyCallback = std::function<void(const std::shared_ptr<LockstepGroup>&)>;
    void SetNotifyEmptyCallback(NotifyEmptyCallback notifyEmptyCallback);

    void Start();
    void AddMember(const std::shared_ptr<Session>& newSession);
    void RemoveMember(const std::shared_ptr<Session>& session);
    void CollectInput(std::unordered_map<uuid, std::shared_ptr<RpcPacket>> rpcRequest);
    void ProcessStep();
    void Tick();

    uuid GetGroupId() const { return _groupId; }
    std::uint64_t GetGroupRttKey() const { return _groupRttKey; }
    std::size_t GetGroupNumber() const { return _groupNumber; }
    
    bool IsFull()
    {
        std::lock_guard<std::mutex> lock(_memberMutex);
        return _members.size() == _maxSessionCount;
    }

    // Operator will be used for sorting
    bool operator<(const LockstepGroup& rhs) const
    {
        return this->_groupNumber < rhs._groupNumber;
    }

private:
    uuid _groupId;
    std::uint64_t _groupRttKey;
    std::size_t _groupNumber;
    IoContext::strand _strand;
    std::set<std::shared_ptr<Session>> _members;
    const std::size_t _maxSessionCount = 3;
    std::mutex _memberMutex;
    
    const std::size_t _fixedDeltaMs = 33;
    std::size_t _currentBucket = 0;

    // input buffer by bucket frame
    std::unordered_map<std::size_t, std::unordered_map<SSessionKey, std::shared_ptr<RpcPacket>>> _inputBuffer;
    std::size_t _inputIdCounter = 0;
    
    std::mutex _bufferMutex;

    boost::asio::steady_timer _timer;
    bool _isRunning = false;

    NotifyEmptyCallback _notifyEmptyCallback;

    void ScheduleNextTick();
};