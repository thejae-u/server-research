#pragma once
#include <string>
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
}