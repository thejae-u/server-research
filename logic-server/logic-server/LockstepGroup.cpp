#include "LockstepGroup.h"

#include "Session.h"
#include "Utility.h"

LockstepGroup::LockstepGroup(const IoContext::strand& strand, const uuid groupId, const std::uint64_t groupRttKey, const std::size_t groupNumber)
    : _groupId(groupId), _groupRttKey(groupRttKey), _groupNumber(groupNumber), _strand(strand), _timer(strand.context())
{
}

void LockstepGroup::SetNotifyEmptyCallback(NotifyEmptyCallback notifyEmptyCallback)
{
    _notifyEmptyCallback = std::move(notifyEmptyCallback);
}

void LockstepGroup::Start()
{
    _isRunning = true;
    Tick();
}

void LockstepGroup::AddMember(const std::shared_ptr<Session>& newSession)
{
    {
        std::lock_guard<std::mutex> lock(_memberMutex);
        if (const auto [it, success] = _members.insert(newSession); !success)
        {
            std::cerr << "Failed to add member\n";
        }
    }

    newSession->SetGroup(shared_from_this());
    newSession->SetStopCallback([this](const std::shared_ptr<Session>& session)
    {
       RemoveMember(session); 
    });
    
    std::cout << _groupId << ": Added Member\n";
}

void LockstepGroup::RemoveMember(const std::shared_ptr<Session>& session)
{
    std::cout << session->GetSessionUuid() << ": Removed from "<< _groupId << "\n";
    
    std::lock_guard<std::mutex> lock(_memberMutex);
    _members.erase(session);

    // Notify the group manager if the group is empty
    boost::asio::post(_strand.wrap([this]()
    {
        if (!_members.empty())
        {
            return;
        }
    
        _notifyEmptyCallback(shared_from_this());    
    }));
}

void LockstepGroup::CollectInput(std::unordered_map<uuid, std::shared_ptr<RpcPacket>> rpcRequest)
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    for (auto& [guid, request] : rpcRequest)
    {
        auto key = SSessionKey{_inputIdCounter++, guid};
        _inputBuffer[_currentBucket][key] = request;
        
        std::cout << _groupId << " CollectInput: Session " << to_string(guid) << " - " << Utility::MethodToString(request->method()) << "\n";
    }
}

void LockstepGroup::ProcessStep()
{
    // _inputBuffer is must locked by Tick()
    // Process the input buffer for the current frame
    auto& input = _inputBuffer[_currentBucket];

    for (auto& [key, packet] : input)
    {
        auto [frame, guid] = key;
        
        // Process each packet
        for (auto& member : _members)
        {
            if (member->GetSocket().is_open())
            {
                if (guid == member->GetSessionUuid())
                    continue;
                
                member->RpcProcess(*packet);
            }
        }
    }
}

void LockstepGroup::Tick()
{
    {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        if (!_isRunning)
            return;

        std::lock_guard<std::mutex> memberLock(_memberMutex);
        if (_members.empty())
        {
            ScheduleNextTick();
            return;
        }
    
        // Process current frame
        ProcessStep();

        _currentBucket++;
        _inputIdCounter = 0;
        
        // Calculate the bucket count
        _inputBuffer.erase(_currentBucket - 1);
    }

    // Schedule the next tick
    ScheduleNextTick();
}

void LockstepGroup::ScheduleNextTick()
{
    // Set the timer for the next tick (input delay)
    auto self(shared_from_this());
    _timer.expires_after(std::chrono::milliseconds(_fixedDeltaMs));
    _timer.async_wait([self](const boost::system::error_code& ec)
        {
            if (ec)
            {
                std::cerr << "ScheduleNextTick failed: " << ec.message() << "\n";
                return;
            }
        
            self->Tick();
        }); 
}