#pragma once
#include <string>
#include <chrono>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "NetworkData.pb.h"
using namespace NetworkData;

namespace Util
{
    struct SPos
    {
        float x;
        float y;
        float z;

        void SetPosition(float newX, float newY, float newZ)
        {
            x = newX;
            y = newY;
            z = newZ;
        }
    };

    struct SAABB
    {
        float minX, minY, minZ;
        float maxX, maxY, maxZ;

        static SAABB MakeAABB(float x, float y, float z, float halfSize) 
        {
            return { x - halfSize, y - halfSize, z - halfSize, x + halfSize, y + halfSize, z + halfSize };
        }

        bool operator==(const SAABB& rhs) const
        {
            return (
                maxX >= rhs.minX && rhs.maxX >= minX &&
                maxY >= rhs.minY && rhs.maxY >= minY &&
                maxZ >= rhs.minZ && rhs.maxZ >= minZ);
        }
    };

    struct SGameState
    {
        std::int32_t hp;
        SPos position;
    };

	struct UserSimpleDto
	{
		boost::uuids::uuid userId;
		std::string name;
	};

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
		case UDP_PORT:
			result = "UdpPort";
			break;

		case PING:
			result = "Ping";
			break;

		case PONG:
			result = "Pong";
			break;

		case USER_INFO:
			result = "UserInfoGet";
			break;

		case GROUP_INFO:
			result = "GroupInfo";
			break;

		case Move:
			result = "Move";
			break;

		case MoveStart:
			result = "MoveStart";
			break;

		case MoveStop:
			result = "MoveStop";
			break;

        case Atk:
            result = "Atk";
            break;
        case Dead:
            result = "Dead";

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