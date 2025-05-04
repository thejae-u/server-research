#include "LockstepGroup.h"

#include <boost/uuid/uuid_io.hpp>

#include "Utility.h"

void LockstepGroup::CollectInput(std::unordered_map<boost::uuids::uuid, std::shared_ptr<RpcRequest>> rpcRequest)
{
    std::lock_guard<std::mutex> lock(_bufferMutex);
    for (auto& [guid, request] : rpcRequest)
    {
        auto packet = std::make_shared<RpcPacket>();
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
        std::cout << boost::uuids::to_string(guid) << ": "<< packet->method() << "\n";
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