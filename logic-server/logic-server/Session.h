#pragma once

#include <boost/asio.hpp>
#include <memory>
#include "NetworkData.pb.h"

using namespace NetworkData;
using io_context = boost::asio::io_context;
using boost::asio::ip::tcp;

class Server;

class Session : public std::enable_shared_from_this<Session>
{
public:
	Session(io_context& io, std::shared_ptr<Server> serverPtr);
	~Session();

	void Start();

	tcp::socket& GetSocket() const { return *_socketPtr; }
	
private:
	io_context& _io;
	std::shared_ptr<tcp::socket> _socketPtr;
	std::shared_ptr<Server>& _serverPtr;

	std::vector<char> _buffer;
	uint32_t _netSize;
	uint32_t _dataSize;

	void AsyncReadSize();
	void AsyncReadData();
	void ProcessRequest(const RpcPacket& reqPacket);
};