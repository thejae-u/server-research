#include "GroupManager.h"

#include "LockstepGroup.h"
#include "Session.h"
#include "Utility.h"

GroupManager::GroupManager(const IoContext::strand& strand, const std::shared_ptr<random_generator>& uuidGenerator): _strand(strand), _uuidGenerator(uuidGenerator)
{
}

void GroupManager::AddSession(const std::shared_ptr<Session>& newSession)
{
    {
        std::lock_guard<std::mutex> lock(_groupMutex);
        for (const auto& [groupId, group] : _groups)
        {
            if (!group->IsFull())
            {
                group->AddMember(newSession);
                SPDLOG_INFO("{} : Session {} is allocated to Group {}", __func__, to_string(newSession->GetSessionUuid()), to_string(groupId));
                newSession->Start();
                return;
            }
        }
    }

    // If no group is found, create a new group
    const auto newGroup = CreateNewGroup();
    newGroup->AddMember(newSession);
    newGroup->Start();
    newSession->Start();

    {
        std::lock_guard<std::mutex> lock(_groupMutex);
        _groups[newGroup->GetGroupId()] = newGroup;
    }
}

std::shared_ptr<LockstepGroup> GroupManager::CreateNewGroup()
{
    const auto newGroup = std::make_shared<LockstepGroup>(_strand, (*_uuidGenerator)());
    newGroup->SetNotifyEmptyCallback([this](const std::shared_ptr<LockstepGroup>& emptyGroup)
    {
        RemoveEmptyGroup(emptyGroup);
    });
    newGroup->Start();
    
    SPDLOG_INFO("{} : Created new group {}", __func__, to_string(newGroup->GetGroupId()));
    
    return newGroup;
}

void GroupManager::RemoveEmptyGroup(const std::shared_ptr<LockstepGroup>& emptyGroup)
{
    {
        const auto groupKey = emptyGroup->GetGroupId();
        const auto it = _groups.find(groupKey);

        if (it == _groups.end())
        {
            SPDLOG_ERROR("{} : Group {} not found in GroupManager", __func__, to_string(groupKey));
            return;
        }
        
        std::lock_guard<std::mutex> lock(_groupMutex);
        _groups.erase(it);
    }

    SPDLOG_INFO("{} : Removed empty group {}", __func__, to_string(emptyGroup->GetGroupId()));
}



/*
bool GroupManager::InsertSessionToGroup(const std::shared_ptr<Session>& session, const int64_t& rtt)
{
    // RTT Group Key
    const auto groupKey = static_cast<std::uint64_t>(rtt / 33);
    
    const auto enterGroup = FindGroupByGroupKey(groupKey);

    if (enterGroup == nullptr)
    {
        SPDLOG_ERROR("{} : enterGroup is nullptr (rtt {})", __func__, rtt);
    }

    enterGroup->AddMember(session);
    return true;
}

std::shared_ptr<LockstepGroup> GroupManager::FindGroupByGroupKey(const std::uint64_t& groupKey)
{
    // if group key is not found, create a new group
    std::lock_guard<std::mutex> lock(_groupMutex);
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
    auto newGroup = std::make_shared<LockstepGroup>(_strand, (*_uuidGenerator)(), groupKey, newGroupNumber);
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
    
    const auto newGroup = std::make_shared<LockstepGroup>(_strand, (*_uuidGenerator)(), groupKey, 0);
    newGroup->SetNotifyEmptyCallback([this](const std::shared_ptr<LockstepGroup>& emptyGroup)
    {
        RemoveEmptyGroup(emptyGroup);
    });
    newGroup->Start();

    _groups[groupKey][0] = newGroup;
    return newGroup;
}*/
