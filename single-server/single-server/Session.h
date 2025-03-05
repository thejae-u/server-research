#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>

#include "PacketHeader.h"

using namespace boost::asio;

class Session
{
public:
	Session(io_context& io);
	~Session();

	void StartSession();
	void SendData(PacketHeader& packet);
	void RecvData(PacketHeader& packet);

private:
	ip::tcp::socket socket_;
};

