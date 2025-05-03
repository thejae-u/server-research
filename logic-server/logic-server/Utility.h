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
}