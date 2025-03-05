#include "Session.h"

Session::Session(io_context& io)
	: socket_(io)
{
}

Session::~Session()
{
}

void Session::StartSession()
{
}

void Session::SendData(PacketHeader& packet)
{
	if (packet.size == 0 || packet.data.empty())
	{
		std::cerr << "Invalid Packet to Send\n";
		return;
	}
}

void Session::RecvData(PacketHeader& packet)
{
	if (packet.size == 0 || packet.data.empty())
	{
		std::cerr << "Invalid Packet to Receive\n";
		return;
	}
}
