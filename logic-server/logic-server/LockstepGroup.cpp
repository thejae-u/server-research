#include "LockstepGroup.h"
#include "Utility.h"

LockstepGroup::LockstepGroup(const io_context::strand& strand, const uuid groupId) : _groupId(groupId), _timer(strand.context()), _groupMaxDelayMs(-1)
{
}

void LockstepGroup::Start()
{
    ScheduleNextTick(); // Start the first tick
}

void LockstepGroup::AddMember(std::shared_ptr<Session> newSession)
{
    {
        std::lock_guard<std::mutex> lock(_memberMutex);
        if (const auto [it, success] = _members.insert(newSession); !success)
        {
            std::cerr << "Failed to add member\n";
        }
    }

    std::cout << _groupId << ": Added Member\n";
}

void LockstepGroup::RemoveMember(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(_memberMutex);
    _members.erase(session);
}

void LockstepGroup::CollectInput(std::unordered_map<uuid, std::shared_ptr<RpcRequest>> rpcRequest)
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    for (auto& [guid, request] : rpcRequest)
    {
        const auto packet = std::make_shared<RpcPacket>();
        packet->set_guid(Utility::GuidToBytes(guid));
        packet->set_method(request->method());
        packet->set_data(request->data());
    
        _inputBuffer[_currentFrame][guid] = packet;
    }
}

void LockstepGroup::ProcessStep()
{
    // _inputBuffer is must locked by Tick()
    // Process the input buffer for the current frame
    auto& input = _inputBuffer[_currentFrame];

    for (auto& [guid, packet] : input)
    {
        // Process each packet
        std::cout << /*uuids*/ to_string(guid) << ": "<< packet->method() << "\n";
    }
}

void LockstepGroup::Tick()
{
    {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        if (!_isRunning)
            return;
    
        // Process current frame
        ProcessStep();
    
        _currentFrame++;
        _inputBuffer.erase(_currentFrame - 1);
    }
    

    // Schedule the next tick
    ScheduleNextTick();
}

void LockstepGroup::ScheduleNextTick()
{
    auto self(shared_from_this());
    _timer.expires_after(std::chrono::milliseconds(_fixedDeltaMs));
    _timer.async_wait([self](const boost::system::error_code& ec)
        {
            if (!ec)
            {
                self->Tick();
            }
        }); 
}