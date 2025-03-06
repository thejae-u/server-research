#ifndef PACKET_HEADER_H
#define PACKET_HEADER_H

enum EPacketType
{
	None = 0,
	Request,
	Response,
	Notify,
};

enum ERequestType
{
	Login = 0,
	Logout,
	// Additional request types
};

struct PACKET_HEADER
{
	std::size_t size;
	std::string data;
}typedef PacketHeader;

#endif // PACKET_HEADER_H
