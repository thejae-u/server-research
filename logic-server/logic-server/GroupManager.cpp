#include "GroupManager.h"

#include "LockstepGroup.h"
#include "Session.h"
#include "ContextManager.h"
#include "Utility.h"

GroupManager::GroupManager(const std::shared_ptr<ContextManager>& ctxManager) : _ctxManager(ctxManager), _privateStrand(_ctxManager->GetContext())
{
}

void GroupManager::AddSession(const std::shared_ptr<GroupDto> groupDto, const std::shared_ptr<Session>& newSession)
{
    uuid joinGroupId = _toUuid(groupDto->groupid());
    {
        std::lock_guard<std::mutex> groupLock(_groupMutex);
        auto groupIt = _groups.find(joinGroupId);
        if (groupIt != _groups.end())
        {
            const auto& [groupId, group] = *groupIt;

            if (group->IsFull())
            {
                spdlog::error("fatal error: {} is full (invalid situation)", to_string(groupId));
                return;
            }

            std::lock_guard<std::mutex> groupBySessionLock(_groupBySessionMutex);
            group->AddMember(newSession);
            _groupsBySession[newSession->GetSessionUuid()] = group;
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
        std::lock_guard<std::mutex> groupLock(_groupMutex);
        _groups[newGroup->GetGroupId()] = newGroup;

        //std::lock_guard<std::mutex> groupBySessionLock(_groupBySessionMutex);
        //_groupsBySession[newSession->GetSessionUuid()] = newGroup;
    }
}

std::shared_ptr<LockstepGroup> GroupManager::CreateNewGroup(const std::shared_ptr<GroupDto> groupDto)
{
    auto self(shared_from_this());
    const auto newGroup = std::make_shared<LockstepGroup>(_ctxManager, groupDto);
    newGroup->SetNotifyEmptyCallback(_privateStrand.wrap([self](const std::shared_ptr<LockstepGroup> emptyGroup) {
        self->RemoveEmptyGroup(emptyGroup);
        })
    );

    newGroup->Start();
    spdlog::info("created new group {}", to_string(newGroup->GetGroupId()));

    return newGroup;
}

void GroupManager::RemoveEmptyGroup(const std::shared_ptr<LockstepGroup> emptyGroup)
{
    std::lock_guard<std::mutex> groupsLock(_groupMutex);
    const auto groupKey = emptyGroup->GetGroupId();
    const auto it = _groups.find(groupKey);

    if (it == _groups.end())
    {
        spdlog::error("group {} not found in group manager", to_string(groupKey));
        return;
    }

    _groups.erase(it);

    spdlog::info("removed empty group {}", to_string(emptyGroup->GetGroupId()));
}

// 호출 안됨
void GroupManager::CollectInput(std::shared_ptr<RpcPacket> input)
{
    auto uid = _toUuid(input->uid());

    std::lock_guard<std::mutex> lock(_groupBySessionMutex);
    if (_groupsBySession.find(uid) == _groupsBySession.end())
    {
        spdlog::error("no group by {}", input->uid());
        return;
    }

    auto collectInput = std::make_shared<std::pair<uuid, std::shared_ptr<RpcPacket>>>(std::make_pair(uid, std::move(input)));
    _groupsBySession[uid]->CollectInput(collectInput);
}