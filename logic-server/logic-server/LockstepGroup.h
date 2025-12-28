#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <set>
#include <list>
#include <functional>
#include <atomic>

#include <asio.hpp>
#include <stduuid/uuid.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "Base.h"
#include "PacketProcess.h"
#include "NetworkData.pb.h"

using IoContext = asio::io_context;
using uuids::uuid;
using namespace NetworkData;

using CompletionHandler = std::function<void()>;

class Session;
class Scheduler;
class ContextManager;

constexpr int TICK_TIME = 33;

struct SSendPacket
{
    std::size_t packetNumber;
    uuid guid;
    std::shared_ptr<RpcPacket> packet; // uid inclusive
};

class LockstepGroup final : public Base<LockstepGroup>
{
public:
	LockstepGroup(const std::shared_ptr<ContextManager>& ctxManager, const std::shared_ptr<GroupDto> newGroupDtoPtr);
	~LockstepGroup() override
	{
		spdlog::info("{} : lockstep group destroyed", _groupInfo->groupid());
	}

	using NotifyEmptyCallback = std::function<void(const std::shared_ptr<LockstepGroup>&)>;
	void SetNotifyEmptyCallback(NotifyEmptyCallback notifyEmptyCallback);

	void Start() override;
	void Stop(bool forceStop) override;
	void AddMember(const std::shared_ptr<Session>& newSession);
	void RemoveMember(const std::shared_ptr<Session>& session);
	void CollectInput(std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>> rpcRequest);
	void Tick(CompletionHandler onComplete);

	uuid GetGroupId() const { return *uuid::from_string(_groupInfo->groupid()); }

	bool IsFull()
	{
		std::lock_guard<std::mutex> lock(_memberMutex);
		return _members.size() == _maxSessionCount;
	}

    void AsyncMakeHitPacket(std::shared_ptr<RpcPacket> atkPacket);

private:
	std::shared_ptr<ContextManager> _ctxManager;
    asio::io_context::strand _privateStrand;

	std::shared_ptr<GroupDto> _groupInfo;

	std::mutex _memberMutex;
	std::unordered_map<uuid, std::shared_ptr<Session>> _members;
	const std::size_t _maxSessionCount = 500;

	std::size_t _fixedDeltaMs;
    std::atomic<std::size_t> _currentBucket = 0;

	std::mutex _bufferMutex;
    std::unordered_map<std::size_t, std::list<std::shared_ptr<SSendPacket>>> _inputBuffer;
	std::atomic<std::size_t> _inputCounter = 0;

	std::shared_ptr<Scheduler> _tickTimer;
	std::atomic<bool> _isRunning = false;

	NotifyEmptyCallback _notifyEmptyCallback;
};