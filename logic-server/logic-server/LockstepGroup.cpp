#include "LockstepGroup.h"

#include <utility>

#include "Session.h"
#include "Utility.h"
#include "Scheduler.h"

LockstepGroup::LockstepGroup(const IoContext::strand& strand, const uuid groupId)
    : _groupId(groupId), _strand(strand)
{
    _fixedDeltaMs = 33;
    _tickTimer = std::make_unique<Scheduler>(_strand.context(), std::chrono::milliseconds(_fixedDeltaMs), [this]() { Tick(); });
}

void LockstepGroup::SetNotifyEmptyCallback(NotifyEmptyCallback notifyEmptyCallback)
{
    _notifyEmptyCallback = std::move(notifyEmptyCallback);
}

void LockstepGroup::Start()
{
    _isRunning = true;
    _tickTimer->Start();
}

void LockstepGroup::Stop()
{
    _tickTimer->Stop();
    _notifyEmptyCallback(shared_from_this());
}

void LockstepGroup::AddMember(const std::shared_ptr<Session>& newSession)
{
    {
        std::lock_guard<std::mutex> lock(_memberMutex);
        if (const auto [it, success] = _members.insert(newSession); !success)
        {
            SPDLOG_ERROR("{} : Failed to add member to group {}", to_string(newSession->GetSessionUuid()), to_string(_groupId));
        }
    }

    newSession->SetGroup(shared_from_this());
    newSession->SetStopCallback([this](const std::shared_ptr<Session>& session)
    {
       RemoveMember(session); 
    });

    newSession->SetCollectInputAction([this](const std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>>& rpcRequest)
    {
        CollectInput(rpcRequest);
    });
    
    SPDLOG_INFO("{} : Added member {}", to_string(_groupId), to_string(newSession->GetSessionUuid()));
}

void LockstepGroup::RemoveMember(const std::shared_ptr<Session>& session)
{
    SPDLOG_INFO("{} : Removed from {}", to_string(session->GetSessionUuid()), to_string(_groupId));

    {
        std::lock_guard<std::mutex> lock(_memberMutex);
        _members.erase(session);
        
        if (!_members.empty())
        {
            return;
        }

        Stop();
    }
}

void LockstepGroup::CollectInput(const std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>>& rpcRequest)
{
    auto [guid, request] = *rpcRequest;

    SSessionKey key;
    {
        std::lock_guard<std::mutex> inputCounterLock(_inputIdCounterMutex);
        key = SSessionKey{_inputIdCounter++, guid};
    }

    {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        _inputBuffer[_currentBucket][key] = request;
    }
        
    // SPDLOG_INFO("{} CollectInput: Session {} - {}", to_string(_groupId), to_string(guid), Utility::MethodToString(request->method()));
}

void LockstepGroup::ProcessStep()
{
    // _inputBuffer is must locked by Tick()
    // Process the input buffer for the current frame
    std::unordered_map<SSessionKey, std::shared_ptr<RpcPacket>> input;
    
    {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        input = _inputBuffer[_currentBucket];
    }

    {
        std::lock_guard<std::mutex> memberLock(_memberMutex);
        for (auto& [key, packet] : input)
        {
            // Process each packet
            for (auto& member : _members)
            {
                // Lockstep is all members must process the same input
                if (member->GetSocket().is_open())
                {
                    member->ProcessRpc(packet);
                }
            }
        }    
    }
    
}

void LockstepGroup::Tick()
{
    if (!_isRunning)
        return;

    {
        std::lock_guard<std::mutex> memberLock(_memberMutex);
        if (_members.empty())
        {
            return;
        }
    }
    
    ProcessStep();

    {
        std::lock_guard<std::mutex> lock(_bucketMutex);
        _currentBucket++;
    }

    {
        std::lock_guard<std::mutex> lock(_inputIdCounterMutex);
        _inputIdCounter = 0;
    }

    {
        std::lock_guard<std::mutex> lock(_bufferMutex);

        // remain the 5 buckets of input buffer
        if (const std::size_t clearBucket = _currentBucket - 6; _inputBuffer.find(clearBucket) != _inputBuffer.end())
            _inputBuffer.erase(clearBucket);
    }
}