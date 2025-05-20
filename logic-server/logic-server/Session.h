#pragma once

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
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
	
	Session(const IoContext::strand& strand, uuid guid);
	~Session() = default;

	void Start();
	void Stop();

	using StopCallback = std::function<void(const std::shared_ptr<Session>&)>;
	void SetStopCallback(StopCallback stopCallback);

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

	std::mutex _stopMutex;
	
	IoContext::strand _strand;
	std::shared_ptr<TcpSocket> _tcpSocketPtr;
	std::shared_ptr<UdpSocket> _udpSocketPtr;

	uuid _sessionUuid;
	std::shared_ptr<LockstepGroup> _lockstepGroupPtr;

	std::chrono::high_resolution_clock::time_point _lastSendTime;
	std::uint64_t _lastRtt;
	boost::asio::steady_timer _pingTimer;

	StopCallback _onStopCallback;

	void SchedulePingTimer();
	void AsyncSendPingPacket();
	
	void ProcessTcpRequest(const std::shared_ptr<RpcPacket>& packet);
	
	void TcpAsyncReadSize();
	void TcpAsyncReadData(const std::shared_ptr<std::vector<char>>& dataBuffer);

	void UdpAsyncReadBufferHeader();
	void UdpAsyncReadData(const std::shared_ptr<std::vector<char>>& dataBuffer);
};