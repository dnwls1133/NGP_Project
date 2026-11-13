#include "PongProtocolAdapter.h"

#include <utility>

namespace pong_test
{
    bool IsValidPaddle(const Paddle& paddle)
    {
        constexpr auto MAX_X = (FIELD_WIDTH - 1) * POSITION_SCALE;
        constexpr auto MAX_Y = (FIELD_HEIGHT - PADDLE_HEIGHT) * POSITION_SCALE;

        return paddle.position.x >= 0 && paddle.position.x <= MAX_X
            && paddle.position.y >= 0 && paddle.position.y <= MAX_Y
            && paddle.velocity.x == 0;
    }

    World MakeInitialWorld()
    {
        World world{};
        world.paddles[0].position = protocol::FixedVec2{ 300, 300 };
        world.paddles[1].position = protocol::FixedVec2{ 3800, 300 };
        return world;
    }

    bool EncodeInput(const Input& input, std::uint32_t sessionId, protocol::PlayerSlot playerId, protocol::Message& out)
    {
        if (sessionId == 0 || playerId >= 2)
            return false;

        protocol::PlayerInputPayload payload{};
        payload.playerId = playerId;
        payload.inputSequence = input.sequence;

        if (input.up)
            payload.moveBits |= protocol::MoveBit::UP;

        if (input.down)
            payload.moveBits |= protocol::MoveBit::DOWN;

        protocol::Message message{};
        message.header.type = protocol::MessageType::PLAYER_INPUT;
        message.header.totalBytes = 40;
        message.header.sessionId = sessionId;
        message.header.sequence = input.sequence;
        message.payload = payload;

        out = std::move(message);
        return true;
    }

    bool DecodeInput(const protocol::Message& message, std::uint32_t sessionId, protocol::PlayerSlot playerId, Input& out)
    {
        if (sessionId == 0 || playerId >= 2 ||
            message.header.type != protocol::MessageType::PLAYER_INPUT ||
            message.header.sessionId != sessionId ||
            message.header.roundId != 1 ||
            message.header.hostTick != 0)
        {
            return false;
        }

        const auto* payload = std::get_if<protocol::PlayerInputPayload>(&message.payload);

        constexpr std::uint8_t VERTICAL_BITS = protocol::MoveBit::UP | protocol::MoveBit::DOWN;

        if (!payload || payload->playerId != playerId ||
            (payload->moveBits & ~VERTICAL_BITS) != 0 ||
            payload->fireHeld != 0 || payload->reserved != 0 ||
            message.header.sequence != payload->inputSequence)
        {
            return false;
        }

        Input input{};
        input.up = (payload->moveBits & protocol::MoveBit::UP) != 0;
        input.down = (payload->moveBits & protocol::MoveBit::DOWN) != 0;
        input.sequence = payload->inputSequence;

        out = input;
        return true;
    }

    bool EncodeSnapshot(const World& world, std::uint32_t sessionId, net::SnapshotBatch& out)
    {
        if (sessionId == 0)
            return false;

        protocol::StateSnapshotPayload payload{};
        payload.snapshotTick = world.hostTick;
        payload.partCount = 1;

        for (std::size_t index = 0; index < world.paddles.size(); ++index)
        {
            const auto& paddle = world.paddles[index];
            if (!IsValidPaddle(paddle))
                return false;

            protocol::ObjectStateRecord record{};
            record.objectId = static_cast<protocol::ObjectId>(index + 1);
            record.kind = protocol::NetObjectKind::PLAYER;
            record.subType = 1;
            record.pos = paddle.position;
            record.vel = paddle.velocity;
            record.hp = 1;
            record.maxHp = 1;

            payload.objects.push_back(record);
        }

        protocol::Message message{};
        message.header.type = protocol::MessageType::STATE_SNAPSHOT;
        message.header.totalBytes = static_cast<std::uint32_t>(protocol::HEADER_BYTES + protocol::SNAPSHOT_HEADER_BYTES + protocol::OBJECT_STATE_RECORD_BYTES * payload.objects.size());
        message.header.sessionId = sessionId;
        message.header.sequence = world.hostTick;
        message.header.hostTick = world.hostTick;
        message.payload = std::move(payload);

        net::SnapshotBatch batch{};
        batch.parts.push_back(std::move(message));

        out = std::move(batch);
        return true;
    }

    bool DecodeSnapshot(const protocol::Message& message, std::uint32_t sessionId, World& out)
    {
        if (sessionId == 0 ||
            message.header.type != protocol::MessageType::STATE_SNAPSHOT ||
            message.header.sessionId != sessionId ||
            message.header.roundId != 1)
        {
            return false;
        }

        const auto* payload = std::get_if<protocol::StateSnapshotPayload>(&message.payload);

        if (!payload || payload->partIndex != 0 || payload->partCount != 1 ||
            payload->objects.size() != 2 ||
            payload->snapshotTick != message.header.hostTick)
        {
            return false;
        }

        World world{};
        world.hostTick = payload->snapshotTick;
        std::array<bool, 2> seen{};

        for (const auto& record : payload->objects)
        {
            if (record.objectId < 1 || record.objectId > 2 ||
                record.kind != protocol::NetObjectKind::PLAYER ||
                record.subType != 1 || record.alive != 1 ||
                record.fsmState != 0 || record.hp != 1 || record.maxHp != 1)
            {
                return false;
            }

            const auto index = static_cast<std::size_t>(record.objectId - 1);
            if (seen[index])
                return false;

            Paddle paddle{};
            paddle.position = record.pos;
            paddle.velocity = record.vel;

            if (!IsValidPaddle(paddle))
                return false;

            world.paddles[index] = paddle;
            seen[index] = true;
        }

        out = world;
        return true;
    }
}
