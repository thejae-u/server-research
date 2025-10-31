#include "GroupManager.h"

#include "LockstepGroup.h"
#include "Session.h"
#include "ContextManager.h"
#include "Utility.h"

GroupManager::GroupManager(const std::shared_ptr<ContextManager>& ctxManager) : _ctxManager(ctxManager)
{
}

void GroupManager::AddSession(const std::shared_ptr<GroupDto> groupDto, const std::shared_ptr<Session>& newSession)
{
    uuid joinGroupId = _toUuid(groupDto->groupid());

    {
        std::lock_guard<std::mutex> lock(_groupMutex);
        for (const auto& [groupId, group] : _groups)
        {
            if (groupId != joinGroupId)
            {
                continue;
            }

            if (group->IsFull())
            {
                spdlog::error("fatal error: {} is full (invalid situation)", to_string(groupId));
                return;
            }

            group->AddMember(newSession);
            spdlog::info("session {} is allocated to group {}", to_string(newSession->GetSessionUuid()), to_string(groupId));
            newSession->Start();
            return;
        }
    }

    const auto newGroup = CreateNewGroup(groupDto);
    newGroup->AddMember(newSession);
    newGroup->Start();
    newSession->Start();

    {
        std::lock_guard<std::mutex> lock(_groupMutex);
        _groups[newGroup->GetGroupId()] = newGroup;
    }
}

std::shared_ptr<LockstepGroup> GroupManager::CreateNewGroup(const std::shared_ptr<GroupDto> groupDto)
{
    const auto newGroup = std::make_shared<LockstepGroup>(_ctxManager, groupDto);
    newGroup->SetNotifyEmptyCallback(_ctxManager->GetStrand().wrap([this](const std::shared_ptr<LockstepGroup> emptyGroup) {
        RemoveEmptyGroup(emptyGroup);
        })
    );

    newGroup->Start();

    spdlog::info("created new group {}", to_string(newGroup->GetGroupId()));

    return newGroup;
}

void GroupManager::RemoveEmptyGroup(const std::shared_ptr<LockstepGroup> emptyGroup)
{
    {
        const auto groupKey = emptyGroup->GetGroupId();
        const auto it = _groups.find(groupKey);

        if (it == _groups.end())
        {
            spdlog::error("group {} not found in group manager", to_string(groupKey));
            return;
        }

        std::lock_guard<std::mutex> lock(_groupMutex);
        _groups.erase(it);
    }

    spdlog::info("removed empty group {}", to_string(emptyGroup->GetGroupId()));
}