#include "GroupManager.h"

#include "LockstepGroup.h"
#include "Session.h"
#include "Utility.h"

GroupManager::GroupManager(const io_context::strand& strand): _strand(strand)
{
}

void GroupManager::AddSession(const std::shared_ptr<Session>& newSessionPtr)
{
    const auto rtt = CheckRtt(newSessionPtr);
    std::cout << newSessionPtr->GetSessionUuid() << " Session RTT: " << rtt << "ms\n";

    if (!InsertSessionToGroup(newSessionPtr, rtt))
    {
        std::cerr << "failed to insert session to group\n";
        return;
    }

    std::cout << newSessionPtr->GetSessionUuid() << " Session is allocated to Group\n";
    newSessionPtr->Start();
}

std::uint64_t GroupManager::CheckRtt(const std::shared_ptr<Session>& newSessionPtr)
{
    tcp::socket& sessionSocket = newSessionPtr->GetSocket();
    
    // check client RTT and allocate group
    RpcPacket pingPacket;
    pingPacket.set_method(PING);
    pingPacket.set_data("");
    pingPacket.set_uuid(Utility::GuidToBytes(newSessionPtr->GetSessionUuid()));
    
    std::string serializedPacket;
    pingPacket.SerializeToString(&serializedPacket);
    
    const uint32_t sendNetSize =  static_cast<uint32_t>(serializedPacket.size());
    const uint32_t sendDataSize = htonl(sendNetSize);
    uint32_t receiveNetSize = 0;
    
    // RTT check start
    auto startTime = std::chrono::high_resolution_clock::now();
        
    boost::asio::write(sessionSocket, boost::asio::buffer(&sendDataSize, sizeof(sendDataSize)));
    boost::asio::write(sessionSocket, boost::asio::buffer(serializedPacket));
        
    boost::asio::read(sessionSocket, boost::asio::buffer(&receiveNetSize, sizeof(receiveNetSize)));
    const uint32_t receiveDataSize = ntohl(receiveNetSize);
    std::vector<char> receiveBuffer(receiveDataSize, 0);
    boost::asio::read(sessionSocket, boost::asio::buffer(receiveBuffer));
        
    auto endTime = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
}

bool GroupManager::InsertSessionToGroup(const std::shared_ptr<Session>& sessionPtr, const uint64_t& rtt)
{
    // RTT Group Key
    const auto groupKey = static_cast<std::uint64_t>(rtt / 33);
    std::shared_ptr<LockstepGroup> enterGroup;
    
    {
        std::lock_guard<std::mutex> lock(_groupMutex);
        enterGroup = FindGroupByGroupKey(groupKey);
    }

    if (enterGroup == nullptr)
    {
        std::cerr << "fatal error: enterGroup is nullptr\n";
    }

    enterGroup->AddMember(sessionPtr);
    return true;
}

std::shared_ptr<LockstepGroup> GroupManager::FindGroupByGroupKey(const std::uint64_t& groupKey)
{
    // if group key is not found, create a new group
    if (const auto it = _groups.find(groupKey); it == _groups.end())
    {
        return CreateNewKeyGroup(groupKey);
    }
    
    // if exist group key, find empty group
    for (const auto& [groupNumber, group] : _groups[groupKey])
    {
        if (!group->IsFull())
        {
            return group;
        }
    }

    // if all groups are full, create a new group
    std::cout << "-------------------------------------\n";
    std::size_t newGroupNumber = _groups[groupKey].size();
    auto newGroup = std::make_shared<LockstepGroup>(_strand, _uuidGenerator(), newGroupNumber);
    newGroup->Start();
    _groups[groupKey][newGroupNumber] = newGroup;

    return newGroup;
}

std::shared_ptr<LockstepGroup> GroupManager::CreateNewKeyGroup(const std::uint64_t& groupKey)
{
    // Group Key == 0
    _groups[groupKey] = std::map<std::size_t, std::shared_ptr<LockstepGroup>>();
    
    const auto newGroup = std::make_shared<LockstepGroup>(_strand, _uuidGenerator(), 0);
    newGroup->Start();

    _groups[groupKey][0] = newGroup;
    return newGroup;
}