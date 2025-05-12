#pragma once
#include <string>
#include <chrono>
#include "NetworkData.pb.h"
using namespace NetworkData;

namespace Utility
{
    static std::string PositionDataToString(const PositionData& positionData)
    {
        return std::to_string(positionData.x1()) + " "
            + std::to_string(positionData.y1()) + " "
            + std::to_string(positionData.z1()) + " "
            + std::to_string(positionData.x2()) + " "
            + std::to_string(positionData.y2()) + " "
            + std::to_string(positionData.z2()) + " ";
    }

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
        if (bytes.size() != 16) throw std::invalid_argument("Invalid UUID size");
        boost::uuids::uuid uuid;
        std::copy(bytes.begin(), bytes.end(), uuid.begin());

        // Reverse the byte order for the first 8 bytes (to big-endian)
        std::reverse(uuid.begin(), uuid.begin() + 4);
        std::reverse(uuid.begin() + 4, uuid.begin() + 6);
        std::reverse(uuid.begin() + 6, uuid.begin() + 8);
        return uuid;
    }

    static PositionData ParseToPositionData(const RpcPacket& packet)
    {
        PositionData result;
        result.ParseFromString(packet.data());
        return result;
    }

    static std::chrono::high_resolution_clock::time_point StartStopwatch()
    {
        return std::chrono::high_resolution_clock::now();
    }

    static std::int64_t StopStopwatch(const std::chrono::high_resolution_clock::time_point startTime)
    {
        std::cout << "RTT Check end\n";
        auto endTime = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    }

    static std::string MethodToString(const RpcMethod& method)
    {
        std::string result;
        switch (method)
        {
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
        
        case LOGIN:
        case REGISTER:
        case RETRIEVE:
        case ACCESS:
        case REJECT:
        case LOGOUT:
        case ATTACK:
        case DROP_ITEM:
        case USE_ITEM:
        case USE_SKILL:
        case STATE_NONE:
        case STATE_MOVE_START:
        case STATE_MOVE_END:
        case STATE_ATTACK_START:
        case STATE_ATTACK_END:
        case REMOTE_MOVE_CALL:
        case REMOTE_ATTACK_CALL:
        case NONE:
        case IN_GAME_NONE:
        default:
            result = "INVALID";
            break;
        }

        return result;
    }
}