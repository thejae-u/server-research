#pragma once

#include <vector>
#include <memory>

#include <spdlog/spdlog.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "NetworkData.pb.h"

using namespace boost::uuids;
using namespace NetworkData;

static void MakeHitPacket(uuid from, uuid to, std::shared_ptr<RpcPacket>& out)
{
    RpcPacket hitPacket;
    hitPacket.set_uid(to_string(to));
    hitPacket.set_method(RpcMethod::Hit);
    hitPacket.set_data(to_string(from));

    out = std::make_shared<RpcPacket>(hitPacket);
}
