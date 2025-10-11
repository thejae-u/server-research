#pragma once
#include <string>
#include <chrono>
#include "NetworkData.pb.h"
using namespace NetworkData;

namespace Utility
{
	static std::string GuidToBytes(boost::uuids::uuid uuid)
	{
		std::string bytes(uuid.begin(), uuid.end());

		// Reverse the byte order for the first 8 bytes (to little-endian)
		std::reverse(bytes.begin(), bytes.begin() + 4);
		std::reverse(bytes.begin() + 4, bytes.begin() + 6);
		std::reverse(bytes.begin() + 6, bytes.begin() + 8);
		return bytes;
	}

	static uuid BytesToUuid(const std::string& bytes)
	{
		if (bytes.size() != 16) throw std::invalid_argument("invalid uuid size");
		boost::uuids::uuid uuid;
		std::copy(bytes.begin(), bytes.end(), uuid.begin());

		// Reverse the byte order for the first 8 bytes (to big-endian)
		std::reverse(uuid.begin(), uuid.begin() + 4);
		std::reverse(uuid.begin() + 4, uuid.begin() + 6);
		std::reverse(uuid.begin() + 6, uuid.begin() + 8);
		return uuid;
	}

	static std::chrono::high_resolution_clock::time_point StartStopwatch()
	{
		return std::chrono::high_resolution_clock::now();
	}

	static std::int64_t StopStopwatch(const std::chrono::high_resolution_clock::time_point startTime)
	{
		const auto endTime = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	}

	static std::string MethodToString(const RpcMethod& method)
	{
		std::string result;
		switch (method)
		{
		case NONE:
			result = "NONE";
			break;

		case UDP_PORT:
			result = "UDP_PORT";
			break;

		case PING:
			result = "PING";
			break;

		case PONG:
			result = "PONG";
			break;

		case UUID:
			result = "UUID";
			break;

		case MOVE:
			result = "MOVE";
			break;

		case MoveStart:
			result = "MoveStart";
			break;

		case MoveStop:
			result = "MoveStop";
			break;

		case PACKET_COUNT:
			result = "PacketCount";
			break;

		default:
			result = "INVALID";
			break;
		}
		return result;
	}
}