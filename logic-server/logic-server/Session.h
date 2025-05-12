#pragma once

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <memory>
#include "NetworkData.pb.h"

using namespace NetworkData;
using IoContext = boost::asio::io_context;
using boost::asio::ip::tcp;

class Server;
class LockstepGroup;

class Session : public std::enable_shared_from_this<Session>
{
public:
	Session(IoContext::strand& strand, std::shared_ptr<Server> serverPtr, boost::uuids::uuid guid);
	~Session() = default;

	void Start();
	void Stop();

	void RpcProcess(RpcPacket packet);
	tcp::socket& GetSocket() const { return *_socketPtr; }
	void SetGroup(const std::shared_ptr<LockstepGroup>& groupPtr) { _lockstepGroupPtr = groupPtr; }
	const boost::uuids::uuid& GetSessionUuid() const { return _sessionUuid; }
	std::int64_t CheckAndGetRtt() const;
	bool SendUuidToClient() const;
	
private:
	std::shared_ptr<Server> _serverPtr;
	IoContext::strand _strand;
	std::shared_ptr<tcp::socket> _socketPtr;

	std::vector<char> _receiveBuffer;
	uint32_t _receiveNetSize;
	uint32_t _receiveDataSize;

	boost::uuids::uuid _sessionUuid;
	std::shared_ptr<LockstepGroup> _lockstepGroupPtr;
	
	void AsyncReadSize();
	void AsyncReadData();
};