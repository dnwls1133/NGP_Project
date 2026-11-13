#pragma once

#include "../net/SendContract.h"
#include <array>

namespace pong_test
{
    inline constexpr int FIELD_WIDTH = 42;
    inline constexpr int FIELD_HEIGHT = 11;
    inline constexpr int PADDLE_HEIGHT = 5;
    inline constexpr std::int32_t POSITION_SCALE = 100;

    struct Input
    {
        bool up = false;
        bool down = false;
        std::uint32_t sequence = 0;
    };

    struct Paddle
    {
        protocol::FixedVec2 position{};
        protocol::FixedVec2 velocity{};
    };

    struct World
    {
        std::array<Paddle, 2> paddles{};
        std::uint32_t hostTick = 0;
    };

    World MakeInitialWorld();

    bool EncodeInput(const Input& input, std::uint32_t sessionId, protocol::PlayerSlot playerId, protocol::Message& out);

    bool DecodeInput(const protocol::Message& message, std::uint32_t sessionId, protocol::PlayerSlot playerId, Input& out);

    bool EncodeSnapshot(const World& world, std::uint32_t sessionId, net::SnapshotBatch& out);

    bool DecodeSnapshot(const protocol::Message& message, std::uint32_t sessionId, World& out);
}
