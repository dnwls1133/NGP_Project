#include "GameFlowTest.h"

#include <utility>

namespace game_flow_test
{
    constexpr std::uint32_t TEST_HOST_TICK = 121;
    constexpr std::int32_t MOVE_PER_TICK = 100;
    constexpr std::int32_t SIM_RATE_HZ = 60;

    World MakeInitialWorld()
    {
        World world{};

        for (std::size_t index = 0; index < world.size(); ++index)
        {
            auto& player = world[index];
            player.objectId = static_cast<protocol::ObjectId>(index + 1);
            player.kind = protocol::NetObjectKind::PLAYER;
            player.subType = 1;
            player.pos.x = index == 0 ? 10000 : 20000;
            player.pos.y = 10000;
            player.hp = 100;
            player.maxHp = 100;
        }

        return world;
    }

    void ApplyInput(protocol::ObjectStateRecord& player, const protocol::PlayerInputPayload& input)
    {
        protocol::FixedVec2 movement{};

        if (input.moveBits & protocol::MoveBit::LEFT)
            movement.x -= MOVE_PER_TICK;

        if (input.moveBits & protocol::MoveBit::RIGHT)
            movement.x += MOVE_PER_TICK;

        if (input.moveBits & protocol::MoveBit::UP)
            movement.y -= MOVE_PER_TICK;

        if (input.moveBits & protocol::MoveBit::DOWN)
            movement.y += MOVE_PER_TICK;

        player.pos.x += movement.x;
        player.pos.y += movement.y;
        player.vel.x = movement.x * SIM_RATE_HZ;
        player.vel.y = movement.y * SIM_RATE_HZ;
    }

    protocol::Message MakeClientInput(std::uint32_t sessionId)
    {
        protocol::PlayerInputPayload payload{};
        payload.playerId = 1;
        payload.moveBits = protocol::MoveBit::LEFT;
        payload.fireHeld = 1;
        payload.inputSequence = 1;

        protocol::Message message{};
        message.header.type = protocol::MessageType::PLAYER_INPUT;
        message.header.totalBytes = 40;
        message.header.sessionId = sessionId;
        message.header.sequence = payload.inputSequence;
        message.payload = payload;

        return message;
    }

    net::SnapshotBatch MakeSnapshot(std::uint32_t sessionId, const World& world)
    {
        protocol::StateSnapshotPayload payload{};
        payload.snapshotTick = TEST_HOST_TICK;
        payload.partIndex = 0;
        payload.partCount = 1;
        payload.objects.assign(world.begin(), world.end());

        protocol::Message message{};
        message.header.type = protocol::MessageType::STATE_SNAPSHOT;
        message.header.totalBytes = static_cast<std::uint32_t>(
            protocol::HEADER_BYTES + protocol::SNAPSHOT_HEADER_BYTES
            + protocol::OBJECT_STATE_RECORD_BYTES * world.size());
        message.header.sessionId = sessionId;
        message.header.sequence = TEST_HOST_TICK;
        message.header.hostTick = TEST_HOST_TICK;
        message.payload = std::move(payload);

        net::SnapshotBatch batch{};
        batch.parts.push_back(std::move(message));
        return batch;
    }

    net::SnapshotBatch MakeExpectedSnapshot(std::uint32_t sessionId)
    {
        auto expected = MakeInitialWorld();

        // 계산 함수를 호출하지 않고 기대 좌표를 직접 지정
        expected[0].pos.x = 10100;
        expected[0].vel.x = 6000;

        expected[1].pos.x = 19900;
        expected[1].vel.x = -6000;

        return MakeSnapshot(sessionId, expected);
    }
}
