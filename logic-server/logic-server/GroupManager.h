#pragma once
#include <memory>
#include <chrono>
#include <map>
#include <set>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

#include <google/protobuf/util/json_util.h>

#include "NetworkData.pb.h"

class Session;
class LockstepGroup;
class ContextManager;

using IoContext = boost::asio::io_context;
using namespace boost::uuids;
using namespace NetworkData;

class GroupManager
{
public:
	GroupManager(const std::shared_ptr<ContextManager>& ctxManager);
	void AddSession(const std::shared_ptr<GroupDto>& groupDto, const std::shared_ptr<Session>& newSession);
	void RemoveEmptyGroup(const std::shared_ptr<LockstepGroup>& emptyGroup);

private:
	std::shared_ptr<ContextManager> _ctxManager;
	boost::uuids::string_generator _toUuid;
	const std::int64_t _invalidRtt = -1;

	std::map<uuid, std::shared_ptr<LockstepGroup>> _groups;
	std::mutex _groupMutex;
	std::shared_ptr<LockstepGroup> CreateNewGroup(const std::shared_ptr<GroupDto>& groupDtoPtr);
};
