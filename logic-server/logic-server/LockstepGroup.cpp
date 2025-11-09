#include "LockstepGroup.h"

#include <utility>

#include "Session.h"
#include "Utility.h"
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
        if (const auto& [it, success] = _members.insert(newSession); !success)
        {
            spdlog::error("{} : failed to add member to group {}", to_string(newSession->GetSessionUuid()), _groupInfo->groupid());
        }
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
        _members.erase(session);

        if (!_members.empty())
        {
            return;
        }

        Stop();
    }
}

void LockstepGroup::CollectInput(std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>> rpcRequest)
{
    // make send packet
    const auto sendPackets = ReprocessInput(rpcRequest);

    {
        std::lock_guard<std::mutex> bufferLock(_bufferMutex);
        // _inputBuffer[_currentBucket].push_back(packet);
        for (const auto inputs : sendPackets)
        {
            _inputBuffer[_currentBucket].push_back(std::make_shared<SSendPacket>(inputs));
	        spdlog::info("{} collect input: session {} - {}", _groupInfo->groupid(), to_string(inputs.guid), Utility::MethodToString(inputs.packet->method()));
        }
    }

    // spdlog::info("{} collect input: session {} - {}", _groupInfo->groupid(), to_string(guid), Utility::MethodToString(request->method()));
}

std::list<SSendPacket> LockstepGroup::ReprocessInput(const std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>> rpcRequest)
{
    const auto [guid, request] = *rpcRequest;
	std::list<SSendPacket> sendPackets;

	if (request->method() != RpcMethod::Atk)
	{
		// pass through
        sendPackets.push_back({ _inputCounter++, guid, std::move(request) });
	}
    else
    {
        sendPackets.push_back({ _inputCounter++, guid, std::move(request) });

        if (request->data().size() == 0)
        {
            return sendPackets;
        }

		auto hitPacket = std::make_shared<RpcPacket>();
		MakeHitPacket(guid, request->data(), hitPacket);

        sendPackets.push_back({ _inputCounter++, guid, std::move(hitPacket) });
	}

    return sendPackets;
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
            for (const auto& member : self->_members)
            {
                if (!member->IsValid())
                    continue;

                member->EnqueueSendPackets(currentBucketPackets);
            }

            self->_currentBucket++;
            self->_inputCounter = 0;
            onComplete(); 
            }
        );}
    );
}