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
using boost::asio::ip::udp;
using boost::uuids::uuid;

class Server;
class LockstepGroup;

constexpr std::int64_t INVALID_RTT = -1;
constexpr std::size_t MAX_PACKET_SIZE = 128;

class Session : public std::enable_shared_from_this<Session>
{
public:
	Session(IoContext::strand& strand, std::shared_ptr<Server> serverPtr, uuid guid);
	~Session() = default;

	void Start();
	void Stop();

	void RpcProcess(RpcPacket packet);
	tcp::socket& GetSocket() const { return *_tcpSocketPtr; }
	void SetGroup(const std::shared_ptr<LockstepGroup>& groupPtr) { _lockstepGroupPtr = groupPtr; }
	const uuid& GetSessionUuid() const { return _sessionUuid; }
	bool SendUdpPort() const;
	std::int64_t CheckAndGetRtt() const;
	bool SendUuidToClient() const;
	
private:
	using TcpSocket = tcp::socket;
	using UdpSocket = udp::socket;
	
	std::shared_ptr<Server> _serverPtr;
	IoContext::strand _strand;
	std::shared_ptr<TcpSocket> _tcpSocketPtr;
	std::shared_ptr<UdpSocket> _udpSocketPtr;

	std::vector<char> _receiveBuffer;
	uint32_t _receiveNetSize;
	uint32_t _receiveDataSize;

	uuid _sessionUuid;
	std::shared_ptr<LockstepGroup> _lockstepGroupPtr;
	
	void TcpAsyncReadSize();
	void TcpAsyncReadData();

	void UdpAsyncReadSize();
	void UdpAsyncReadData(std::shared_ptr<std::vector<char>> dataBuffer);
};