#include "LockstepGroup.h"

#include <utility>

#include "Session.h"
#include "Util.h"
#include "Scheduler.h"
#include "ContextManager.h"

LockstepGroup::LockstepGroup(const std::shared_ptr<ContextManager>& ctxManager, const std::shared_ptr<GroupDto> newGroupDtoPtr)
    : _ctxManager(ctxManager), _privateStrand(_ctxManager->GetContext()), _groupInfo(newGroupDtoPtr)
{
    _fixedDeltaMs = TICK_TIME; // Delay Time
}

void LockstepGroup::SetNotifyEmptyCallback(NotifyEmptyCallback notifyEmptyCallback)
{
    _notifyEmptyCallback = std::move(notifyEmptyCallback);
}

void LockstepGroup::Start()
{
    _isRunning = true;
    auto self(shared_from_this());
    _tickTimer = std::make_shared<Scheduler>(_privateStrand, std::chrono::milliseconds(_fixedDeltaMs), [self](CompletionHandler onComplete) {
        self->Tick(onComplete);
        }
    );

    _tickTimer->Start();
}

void LockstepGroup::Stop()
{
    _isRunning = false;
    _tickTimer->Stop();
    _notifyEmptyCallback(shared_from_this());
}

void LockstepGroup::AddMember(const std::shared_ptr<Session>& newSession)
{
    {
        std::lock_guard<std::mutex> lock(_memberMutex); 
        _members[newSession->GetSessionUuid()] = newSession;
    }

    auto self(shared_from_this());
    newSession->SetGroup(self);
    newSession->SetStopCallbackByGroup([self](const std::shared_ptr<Session>& session) {
        self->RemoveMember(session);
        }
    );

    newSession->SetCollectInputAction([self](std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>> rpcRequest) {
        self->CollectInput(std::move(rpcRequest));
        }
    );

    spdlog::info("{} : added member {}", _groupInfo->groupid(), to_string(newSession->GetSessionUuid()));
}

void LockstepGroup::RemoveMember(const std::shared_ptr<Session>& session)
{
    spdlog::info("{} : removed from {}", to_string(session->GetSessionUuid()), _groupInfo->groupid());
    {
        std::lock_guard<std::mutex> lock(_memberMutex);
        _members.erase(session->GetSessionUuid());

        if (!_members.empty())
        {
            return;
        }

        Stop();
    }
}

void LockstepGroup::CollectInput(std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>> rpcRequest)
{
    const auto [guid, request] = *rpcRequest;
    SSendPacket newInput = { _inputCounter++, guid, request };

    {
        std::lock_guard<std::mutex> bufferLock(_bufferMutex);
        _inputBuffer[_currentBucket].push_back(std::make_shared<SSendPacket>(newInput));
    }

    // spdlog::info("{} collect input: session {} - {}", _groupInfo->groupid(), to_string(guid), Utility::MethodToString(request->method()));

    if (request->method() != RpcMethod::Atk)
    {
        return;
    }

    // make hit packet after check valid attack
    auto self(shared_from_this());
    boost::asio::post(_privateStrand.wrap([self, request]() { self->AsyncMakeHitPacket(request); }));
}

void LockstepGroup::Tick(CompletionHandler onComplete)
{
    // check state first
    if (!_isRunning)
    {
        onComplete();
        return;
    }

    auto self(shared_from_this());
    boost::asio::post(_ctxManager->GetBlockingPool(), [self, onComplete]() {
        std::list<std::shared_ptr<SSendPacket>> currentBucketPackets;
        {
            std::lock_guard<std::mutex> bufferLock(self->_bufferMutex);
            currentBucketPackets = self->_inputBuffer[self->_currentBucket];
        }

        boost::asio::post(self->_privateStrand, [self, onComplete, currentBucketPackets]() {
            std::lock_guard<std::mutex> memberLock(self->_memberMutex);
            for (const auto& [uid, member]  : self->_members)
            {
                if (!member->IsValid())
                    continue;

                member->EnqueueSendUdpPackets(currentBucketPackets);
            }

            ++self->_currentBucket;
            self->_inputCounter = 0;
            onComplete(); 
            }
        );}
    );
}

void LockstepGroup::AsyncMakeHitPacket(std::shared_ptr<RpcPacket> atkPacket)
{
    // AABB check
    auto attacker = _toUuid(atkPacket->uid());
    uuid victim;

    if (atkPacket->data().empty())
        return;

    victim = _toUuid(atkPacket->data());

    Util::SGameState fromState;
    Util::SGameState toState;

    {
        std::lock_guard<std::mutex> memberLock(_memberMutex);
        fromState = _members[attacker]->GetGameState();
        toState = _members[victim]->GetGameState();
    }

    // Find User
    auto fromAABB = Util::SAABB::MakeAABB(fromState.position.x, fromState.position.y, fromState.position.z, 0.5f);
    auto toAABB = Util::SAABB::MakeAABB(toState.position.x, toState.position.y, toState.position.z, 0.5f);

    if (fromAABB == toAABB) // Hit
    {
        auto self(shared_from_this());
        spdlog::info("hit {} from {}", to_string(victim), to_string(attacker));
        boost::asio::post(_privateStrand.wrap([self, attacker, victim]() {
            auto hitPacket = std::make_shared<RpcPacket>();
            MakeHitPacket(attacker, victim, hitPacket);

            // victim hit rpc packet
            auto requestPacket = std::make_shared<std::pair<uuid, std::shared_ptr<RpcPacket>>>();
            requestPacket->first = victim;
            requestPacket->second = hitPacket;

            self->CollectInput(std::move(requestPacket));
            })
        );

        return;
    }

    // No Hit
    spdlog::info("no hit {} from {}", to_string(victim), to_string(attacker));
}
