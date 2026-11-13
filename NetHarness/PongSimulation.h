#pragma once

#include "PongProtocolAdapter.h"

namespace pong_test
{
    inline constexpr std::int32_t SIMULATION_RATE_HZ = 60;

    void UpdatePaddle(Paddle& paddle, const Input& input);
    void SimulateTick(World& world, const std::array<Input, 2>& inputs);
}
