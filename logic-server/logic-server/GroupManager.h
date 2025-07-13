#pragma once
#include <memory>
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

class Session;
class LockstepGroup;

using IoContext = boost::asio::io_context;
using namespace boost::uuids;

class GroupManager
{
public:
    GroupManager(const IoContext::strand& strand, const std::shared_ptr<random_generator>& uuidGenerator);
    void AddSession(const std::shared_ptr<Session>& newSession);
    void RemoveEmptyGroup(const std::shared_ptr<LockstepGroup>& emptyGroup);

private:
    IoContext::strand _strand;
    std::shared_ptr<random_generator> _uuidGenerator;

    const std::int64_t _invalidRtt = -1;

    std::map<uuid, std::shared_ptr<LockstepGroup>> _groups;
    std::mutex _groupMutex;
    
    std::shared_ptr<LockstepGroup> CreateNewGroup();
};
