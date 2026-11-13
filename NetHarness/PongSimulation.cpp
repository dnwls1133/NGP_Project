#include "PongSimulation.h"

#include <algorithm>

namespace pong_test
{
    void UpdatePaddle(Paddle& paddle, const Input& input)
    {
        // 초당 화면 6칸 이동. 위치 단위는 1칸 = 100.
        constexpr std::int32_t SPEED_PER_SECOND = 6 * POSITION_SCALE;
        constexpr std::int32_t MOVE_PER_TICK = SPEED_PER_SECOND / SIMULATION_RATE_HZ;

        constexpr std::int32_t MIN_Y = 0;
        constexpr std::int32_t MAX_Y = (FIELD_HEIGHT - PADDLE_HEIGHT) * POSITION_SCALE;

        // 아래만 누르면 +1, 위만 누르면 -1.
        // 둘 다 누르거나 아무것도 누르지 않으면 0.
        const int direction = static_cast<int>(input.down) - static_cast<int>(input.up);

        const std::int32_t previousY = paddle.position.y;
        const std::int32_t nextY = previousY + direction * MOVE_PER_TICK;

        paddle.position.y = std::clamp(nextY, MIN_Y, MAX_Y);

        // 경계에 막혔을 때도 실제 이동량에 맞는 속도를 기록한다.
        paddle.velocity.x = 0;
        paddle.velocity.y = (paddle.position.y - previousY) * SIMULATION_RATE_HZ;
    }

    void SimulateTick(World& world, const std::array<Input, 2>& inputs)
    {
        for (std::size_t i = 0; i < world.paddles.size(); ++i)
        {
            UpdatePaddle(world.paddles[i], inputs[i]);
        }

        ++world.hostTick;
    }
}
