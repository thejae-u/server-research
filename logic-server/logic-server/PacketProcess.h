#pragma once

#include <vector>
#include <memory>

#include <spdlog/spdlog.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "NetworkData.pb.h"

using namespace boost::uuids;
using namespace NetworkData;

static void MakeHitPacket(uuid attacker, uuid victim, std::shared_ptr<RpcPacket>& out, std::int32_t dmg)
{
    RpcPacket hitPacket;
    hitPacket.set_uid(to_string(victim));
    hitPacket.set_method(RpcMethod::Hit);

    HitData hitData;
    hitData.set_attacker(to_string(attacker));
    hitData.set_dmg(dmg);

    std::string serializedHitData;
    hitData.SerializeToString(&serializedHitData);
    hitPacket.set_data(serializedHitData);

    out = std::make_shared<RpcPacket>(hitPacket);
}
