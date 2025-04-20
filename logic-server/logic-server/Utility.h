#pragma once
#include <string>
#include "NetworkData.pb.h"
using namespace NetworkData;

namespace Utility
{
    static std::string PositionDataToString(const PositionData& positionData)
    {
        return std::to_string(positionData.x()) + " "
            + std::to_string(positionData.y()) + " "
            + std::to_string(positionData.z());
    }
}