#include "LockstepGroup.h"

#include <utility>

#include "Session.h"
#include "Utility.h"
#include "Scheduler.h"
#include "ContextManager.h"

LockstepGroup::LockstepGroup(const std::shared_ptr<ContextManager>& ctxManager, const uuid groupId)
	: _groupId(groupId), _ctxManager(ctxManager)
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
	_tickTimer = std::make_shared<Scheduler>(_ctxManager->GetStrand(), std::chrono::milliseconds(_fixedDeltaMs), [this]()
		{
			Tick();
		});

	_tickTimer->Start();
}

void LockstepGroup::Stop()
{
	_tickTimer->Stop();
	_notifyEmptyCallback(shared_from_this());
}

void LockstepGroup::AddMember(const std::shared_ptr<Session>& newSession)
{
	{
		std::lock_guard<std::mutex> lock(_memberMutex);
		if (const auto& [it, success] = _members.insert(newSession); !success)
		{
			SPDLOG_ERROR("{} {} : Failed to add member to group {}", __func__, to_string(newSession->GetSessionUuid()), to_string(_groupId));
		}
	}

	newSession->SetGroup(shared_from_this());
	newSession->SetStopCallback([this](const std::shared_ptr<Session>& session)
		{
			RemoveMember(session);
		});

	newSession->SetCollectInputAction([this](const std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>>& rpcRequest)
		{
			CollectInput(rpcRequest);
		});

	SPDLOG_INFO("{} {} : Added member {}", __func__, to_string(_groupId), to_string(newSession->GetSessionUuid()));
}

void LockstepGroup::RemoveMember(const std::shared_ptr<Session>& session)
{
	SPDLOG_INFO("{} {} : Removed from {}", __func__, to_string(session->GetSessionUuid()), to_string(_groupId));

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

void LockstepGroup::CollectInput(const std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>>& rpcRequest)
{
	auto [guid, request] = *rpcRequest;

	SSessionKey key;
	{
		std::lock_guard<std::mutex> inputCounterLock(_inputIdCounterMutex);
		key = SSessionKey{ _inputIdCounter++, guid };
	}

	{
		std::lock_guard<std::mutex> lock(_bufferMutex);
		std::lock_guard<std::mutex> bucketLock(_bucketMutex);
		_inputBuffer[_currentBucket][key] = request;
	}

	// SPDLOG_INFO("{} CollectInput: Session {} - {}", to_string(_groupId), to_string(guid), Utility::MethodToString(request->method()));
}

void LockstepGroup::ProcessStep()
{
	std::unordered_map<SSessionKey, std::shared_ptr<RpcPacket>> input;

	{
		std::lock_guard<std::mutex> lock(_bufferMutex);
		std::lock_guard<std::mutex> bucketLock(_bucketMutex);
		input = _inputBuffer[_currentBucket];
	}

	// Copy and safe Access to Members
	std::set<std::shared_ptr<Session>> cpMembers;
	{
		std::lock_guard<std::mutex> memberLock(_memberMutex);
		cpMembers = _members;
	}

	for (auto& member : cpMembers)
	{
		if (member == nullptr || !member->IsValid())
			continue;

		// Session Rpc Call
		member->SendRpcPacketToClient(input);
	}
}

void LockstepGroup::Tick()
{
	if (!_isRunning)
		return;

	auto self = shared_from_this();

	boost::asio::post(_ctxManager->GetBlockingPool(), [self]()
		{
			self->ProcessStep();

			boost::asio::post(self->_ctxManager->GetStrand(), [self]()
				{
					self->_currentBucket++;
					self->_inputIdCounter = 0;
				}
			);
		}
	);
}