#include "GroupManager.h"

#include "LockstepGroup.h"
#include "Session.h"
#include "Utility.h"

#define INVALID_RTT -1

GroupManager::GroupManager(const IoContext::strand& strand): _strand(strand)
{
}

void GroupManager::AddSession(const std::shared_ptr<Session>& newSession)
{
    // Send UDP Port
    if (!newSession->SendUdpPort())
    {
        std::cerr << "error: failed to send udp port\n";
        return;
    }
    
    // RTT Check
    const auto rtt = newSession->CheckAndGetRtt();
    if (rtt == INVALID_RTT)
    {
        std::cerr << "RTT check failed\n";
        return;
    }
    
    std::cout << "RTT: " << rtt << "ms\n";

    // Send Session UUID
    if (!newSession->SendUuidToClient())
    {
        std::cerr << "error: failed to send uuid to client\n";
        return;
    }

    if (!InsertSessionToGroup(newSession, rtt))
    {
        std::cerr << "error: failed to allocate session\n";
        return;
    }
    
    std::cout << newSession->GetSessionUuid() << " Session is allocated to Group\n";
    newSession->Start();
}

void GroupManager::RemoveEmptyGroup(const std::shared_ptr<LockstepGroup>& emptyGroup)
{
    const auto groupKey = emptyGroup->GetGroupRttKey();
    const auto groupNumber = emptyGroup->GetGroupNumber();
    
    std::lock_guard<std::mutex> lock(_groupMutex);
    _groups[groupKey].erase(groupNumber);
    std::cout << "RTT " << groupKey << " Group " << groupNumber << " is empty\n";

    if (_groups[groupKey].empty())
    {
        _groups.erase(groupKey);
        std::cout << "RTT " << groupKey << " Group is empty and removed\n";
    }
}

bool GroupManager::InsertSessionToGroup(const std::shared_ptr<Session>& session, const int64_t& rtt)
{
    // RTT Group Key
    const auto groupKey = static_cast<std::uint64_t>(rtt / 33);
    std::shared_ptr<LockstepGroup> enterGroup;
    
    {
        std::lock_guard<std::mutex> lock(_groupMutex);
        enterGroup = FindGroupByGroupKey(groupKey);
    }

    if (enterGroup == nullptr)
    {
        std::cerr << "error: enterGroup is nullptr (rtt " << rtt <<  ")\n";
    }

    enterGroup->AddMember(session);
    return true;
}

std::shared_ptr<LockstepGroup> GroupManager::FindGroupByGroupKey(const std::uint64_t& groupKey)
{
    // if group key is not found, create a new group
    if (const auto it = _groups.find(groupKey); it == _groups.end())
    {
        return CreateNewKeyGroup(groupKey);
    }
    
    // if exist group key, find empty group
    for (const auto& [groupNumber, group] : _groups[groupKey])
    {
        if (!group->IsFull())
        {
            return group;
        }
    }

    // if all groups are full, create a new group
    std::size_t newGroupNumber = _groups[groupKey].size();
    auto newGroup = std::make_shared<LockstepGroup>(_strand, _uuidGenerator(), groupKey, newGroupNumber);
    newGroup->SetNotifyEmptyCallback([this](const std::shared_ptr<LockstepGroup>& emptyGroup)
    {
        RemoveEmptyGroup(emptyGroup);
    });
    newGroup->Start();
    _groups[groupKey][newGroupNumber] = newGroup;

    return newGroup;
}

std::shared_ptr<LockstepGroup> GroupManager::CreateNewKeyGroup(const std::uint64_t& groupKey)
{
    _groups[groupKey] = std::map<std::size_t, std::shared_ptr<LockstepGroup>>();
    
    const auto newGroup = std::make_shared<LockstepGroup>(_strand, _uuidGenerator(), groupKey, 0);
    newGroup->SetNotifyEmptyCallback([this](const std::shared_ptr<LockstepGroup>& emptyGroup)
    {
        RemoveEmptyGroup(emptyGroup);
    });
    newGroup->Start();

    _groups[groupKey][0] = newGroup;
    return newGroup;
}