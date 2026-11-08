#pragma once

#include "../net/SendContract.h"

#include <array>

namespace game_flow_test
{
    using World = std::array<protocol::ObjectStateRecord, 2>;

    World MakeInitialWorld();
    void ApplyInput(protocol::ObjectStateRecord& player, const protocol::PlayerInputPayload& input);

    protocol::Message MakeClientInput(std::uint32_t sessionId);
    net::SnapshotBatch MakeSnapshot(std::uint32_t sessionId, const World& world);
    net::SnapshotBatch MakeExpectedSnapshot(std::uint32_t sessionId);
}
