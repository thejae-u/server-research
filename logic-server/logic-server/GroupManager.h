#pragma once
#include <memory>
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <mutex>

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

class Session;
class LockstepGroup;

using io_context = boost::asio::io_context;
using namespace boost::uuids;

class GroupManager
{
public:
    explicit GroupManager(const io_context::strand& strand);
    void AddSession(const std::shared_ptr<Session>& newSessionPtr);

private:
    io_context::strand _strand;
    random_generator _uuidGenerator;

    std::map<std::uint64_t, std::map<std::size_t, std::shared_ptr<LockstepGroup>>> _groups;
    std::mutex _groupMutex;
    
    std::uint64_t CheckRtt(const std::shared_ptr<Session>& newSessionPtr);
    bool InsertSessionToGroup(const std::shared_ptr<Session>& sessionPtr, const uint64_t& rtt);
    std::shared_ptr<LockstepGroup> FindGroupByGroupKey(const std::uint64_t& groupKey);
    std::shared_ptr<LockstepGroup> CreateNewKeyGroup(const std::uint64_t& groupKey);
};
