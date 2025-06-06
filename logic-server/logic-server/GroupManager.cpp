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
    newGroup->SetNotifyEmptyCallback(_strand.wrap([this](const std::shared_ptr<LockstepGroup>& emptyGroup)
    {
        RemoveEmptyGroup(emptyGroup);
    }));
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