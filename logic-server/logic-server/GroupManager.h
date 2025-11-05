#pragma once
#include <memory>
#include <chrono>
#include <unordered_map>
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

using namespace boost::uuids;
using namespace NetworkData;

class GroupManager : public std::enable_shared_from_this<GroupManager>
{
public:
    GroupManager(const std::shared_ptr<ContextManager>& ctxManager);
    void AddSession(const std::shared_ptr<GroupDto> groupDto, const std::shared_ptr<Session>& newSession);
    void RemoveEmptyGroup(const std::shared_ptr<LockstepGroup> emptyGroup);
    void CollectInput(std::shared_ptr<RpcPacket> input);

private:
    std::shared_ptr<ContextManager> _ctxManager;
    boost::asio::io_context::strand _privateStrand;

    boost::uuids::string_generator _toUuid; // string to uuid utility
    const std::int64_t _invalidRtt = -1;

    std::mutex _groupMutex;
    std::unordered_map<uuid, std::shared_ptr<LockstepGroup>> _groups;

    std::mutex _groupBySessionMutex;
    std::unordered_map<uuid, std::shared_ptr<LockstepGroup>> _groupsBySession;

    std::shared_ptr<LockstepGroup> CreateNewGroup(const std::shared_ptr<GroupDto> groupDtoPtr);
};
