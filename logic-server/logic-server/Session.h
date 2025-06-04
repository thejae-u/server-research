#pragma once

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <memory>
#include <boost/uuid/uuid_io.hpp>

#include "Scheduler.h"
#include "NetworkData.pb.h"

using namespace NetworkData;
using IoContext = boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using boost::uuids::uuid;

class Server;
class LockstepGroup;
class Scheduler;
struct SSessionKey;

constexpr std::int64_t INVALID_RTT = -1;
constexpr std::size_t MAX_PACKET_SIZE = 128;

class Session : public std::enable_shared_from_this<Session>
{
public:
	
	Session(const IoContext::strand& strand, const IoContext::strand& rpcStrand,  uuid guid);
	~Session()
	{
		SPDLOG_INFO("{} : Session destroyed", to_string(_sessionUuid));
	}

	void Start();
	void Stop();

	using StopCallback = std::function<void(const std::shared_ptr<Session>&)>;
	void SetStopCallback(StopCallback stopCallback);

	using SessionInput = std::function<void(std::shared_ptr<std::pair<uuid, std::shared_ptr<RpcPacket>>>)>;
	void SetCollectInputAction(SessionInput inputAction);

	void ProcessRpc(std::unordered_map<SSessionKey, std::shared_ptr<RpcPacket>> allInputs);
	tcp::socket& GetSocket() const { return *_tcpSocketPtr; }
	void SetGroup(const std::shared_ptr<LockstepGroup>& groupPtr) { _lockstepGroupPtr = groupPtr; }
	const uuid& GetSessionUuid() const { return _sessionUuid; }
	bool ExchangeUdpPort();
	std::int64_t CheckAndGetRtt() const;
	bool SendUuidToClient() const;

	bool IsValid() const { return _isConnected; }
	
private:
	using TcpSocket = tcp::socket;
	using UdpSocket = udp::socket;

	IoContext::strand _strand;
	IoContext::strand _rpcStrand;
	std::shared_ptr<TcpSocket> _tcpSocketPtr;
	std::shared_ptr<UdpSocket> _udpSocketPtr;
	udp::endpoint _udpSendEp;

	bool _isConnected = false;

	uuid _sessionUuid;
	std::shared_ptr<LockstepGroup> _lockstepGroupPtr;

	std::shared_ptr<Scheduler> _pingTimer;
	std::uint32_t _checkRttDelay = 500;
	std::chrono::high_resolution_clock::time_point _pingTime;
	std::uint64_t _lastRtt;

	StopCallback _onStopCallback;
	SessionInput _inputAction;

	std::uint32_t _tcpNetSize = 0;
	std::uint32_t _tcpDataSize = 0;

	void SendPingPacket();
	void ProcessTcpRequest(const std::shared_ptr<RpcPacket>& packet);
	
	void TcpAsyncReadSize();
	void TcpAsyncReadData(const std::shared_ptr<std::vector<char>>& dataBuffer);

	void UdpAsyncRead();
	void UdpAsyncWrite(const std::shared_ptr<std::string>& data);
};