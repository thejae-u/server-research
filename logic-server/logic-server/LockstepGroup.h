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
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "NetworkData.pb.h"

using IoContext = boost::asio::io_context;
using namespace boost::uuids;
using namespace NetworkData;
class Session;
class Scheduler;

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

namespace std
{
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
    LockstepGroup(const IoContext::strand& strand, uuid groupId); 
    ~LockstepGroup()
    {
        SPDLOG_INFO("{} : LockstepGroup destroyed", to_string(_groupId));
    }

    using NotifyEmptyCallback = std::function<void(const std::shared_ptr<LockstepGroup>&)>;
    void SetNotifyEmptyCallback(NotifyEmptyCallback notifyEmptyCallback);

    void Start();
    void Stop();
    void AddMember(const std::shared_ptr<Session>& newSession);
    void RemoveMember(const std::shared_ptr<Session>& session);
    void CollectInput(const std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>>& rpcRequest);
    void ProcessStep();
    void Tick();

    uuid GetGroupId() const { return _groupId; }
    
    bool IsFull()
    {
        std::lock_guard<std::mutex> lock(_memberMutex);
        return _members.size() == _maxSessionCount;
    }

private:
    uuid _groupId;
    IoContext::strand _strand;
    std::set<std::shared_ptr<Session>> _members;
    const std::size_t _maxSessionCount = 4;
    std::mutex _memberMutex;
    
    std::size_t _fixedDeltaMs;
    std::size_t _currentBucket = 0;
    std::mutex _bucketMutex;

    // input buffer by bucket frame
    std::unordered_map<std::size_t, std::unordered_map<SSessionKey, std::shared_ptr<RpcPacket>>> _inputBuffer;
    std::size_t _inputIdCounter = 0;
    std::mutex _inputIdCounterMutex;
    
    std::mutex _bufferMutex;

    std::unique_ptr<Scheduler> _tickTimer;
    bool _isRunning = false;

    NotifyEmptyCallback _notifyEmptyCallback;
};