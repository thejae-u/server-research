#pragma once

#include <vector>
#include <memory>

#include <spdlog/spdlog.h>

#include <stduuid/uuid.h>

#include "NetworkData.pb.h"

using uuids::uuid;
using namespace NetworkData;

static void MakeHitPacket(uuid attacker, uuid victim, std::shared_ptr<RpcPacket>& out, std::int32_t dmg)
{
    RpcPacket hitPacket;
    hitPacket.set_uid(uuids::to_string(victim));
    hitPacket.set_method(RpcMethod::Hit);

    HitData hitData;
    hitData.set_attacker(uuids::to_string(attacker));
    hitData.set_dmg(dmg);

    std::string serializedHitData;
    hitData.SerializeToString(&serializedHitData);
    hitPacket.set_data(serializedHitData);

    out = std::make_shared<RpcPacket>(hitPacket);
}