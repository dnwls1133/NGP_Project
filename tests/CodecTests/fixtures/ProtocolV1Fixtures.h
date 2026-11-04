#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "../../../shared/protocol/ProtocolTypes.h"

namespace protocol::fixtures
{
    // 고정 정상 바이트 1: JOIN (32바이트)
    // magic = 0x5653, version = 1, type = JOIN, flags = 0,
    // totalBytes = 32, sessionId = 0, roundId = 1, sequence = 0, hostTick = 0, reserved = 0
    inline constexpr std::array<std::uint8_t, 32> VALID_JOIN_BYTES = {
        0x56, 0x53, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    inline Message MakeValidJoinMessage()
    {
        Message msg{};
        msg.header.magic = MAGIC;
        msg.header.version = VERSION;
        msg.header.type = MessageType::JOIN;
        msg.header.flags = FLAGS_NONE;
        msg.header.totalBytes = 32;
        msg.header.sessionId = 0;
        msg.header.roundId = 1;
        msg.header.sequence = 0;
        msg.header.hostTick = 0;
        msg.header.reserved = RESERVED_VALUE;
        msg.payload = JoinPayload{};
        return msg;
    }
    // 고정 정상 바이트 2: PLAYER_INPUT (40바이트)
    // sequence = 0x01020304 -> 오프셋 20..23 바이트가 01 02 03 04 인지 검산
    // sessionId = 1000 (0x000003E8), playerId = 1, moveBits = UP|LEFT (0x05), fireHeld = 1
    inline constexpr std::array<std::uint8_t, 40> VALID_PLAYER_INPUT_BYTES = {
        0x56, 0x53, 0x00, 0x01, 0x00, 0x0B, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x03, 0xE8,
        0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04
    };

    inline Message MakeValidPlayerInputMessage()
    {
        Message msg{};
        msg.header.magic = MAGIC;
        msg.header.version = VERSION;
        msg.header.type = MessageType::PLAYER_INPUT;
        msg.header.flags = FLAGS_NONE;
        msg.header.totalBytes = 40;
        msg.header.sessionId = 1000;
        msg.header.roundId = 1;
        msg.header.sequence = 0x01020304u;
        msg.header.hostTick = 0;
        msg.header.reserved = RESERVED_VALUE;
        PlayerInputPayload input{};
        input.playerId = 1;
        input.moveBits = MoveBit::UP | MoveBit::LEFT;
        input.fireHeld = 1;
        input.reserved = 0;
        input.inputSequence = 0x01020304u;
        msg.payload = input;
        return msg;
    }
    // 고정 정상 바이트 3: SPAWN (68 바이트, 음수 i32 속도 및 발사자.피해량 포함)
    // - 부호 있는 32비트 정수(FixedVec2 음수 좌표/속도)가 2의 보수 Big-Endian으로
    //   정확히 직렬화되는지 검증하기 위한 정답 바이트 배열입니다.
    inline constexpr std::array<std::uint8_t, 68> VALID_SPAWN_BYTES = {
        // Header (32B)
        0x56, 0x53, 0x00, 0x01, 0x00, 0x0D, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x03, 0xE8,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00,
        // Payload (36B)
        0x00, 0x00, 0x00, 0x0F, // eventId = 15
        0x00, 0x00, 0x00, 0x2A, // objectId = 42
        0x03, 0x06, 0x00, 0x00, // kind=3(PROJ_PLAYER), subType=6, ownerPlayerId=0, patternType=0
        0x00, 0x00, 0xFA, 0x00, // pos.x = 64000
        0x00, 0x01, 0x11, 0x70, // pos.y = 70000
        0xFF, 0xFF, 0xFF, 0x06, // vel.x = -250 (2의 보수 Big-Endian)
        0xFF, 0xFD, 0xB6, 0x10, // vel.y = -150000 (2의 보수 Big-Endian)
        0x00, 0x00, 0x00, 0x01, // hp = 1
        0x00, 0x00, 0x00, 0x02  // damage = 2
    };
    inline Message MakeValidSpawnMessage()
    {
        Message msg{};
        msg.header.magic = MAGIC;
        msg.header.version = VERSION;
        msg.header.type = MessageType::SPAWN;
        msg.header.flags = FLAGS_NONE;
        msg.header.totalBytes = 68;
        msg.header.sessionId = 1000;
        msg.header.roundId = 1;
        msg.header.sequence = 7;
        msg.header.hostTick = 120;
        msg.header.reserved = RESERVED_VALUE;
        SpawnPayload spawn{};
        spawn.eventId = 15;
        spawn.objectId = 42;
        spawn.kind = NetObjectKind::PROJ_PLAYER;
        spawn.subType = 6;
        spawn.ownerPlayerId = 0;
        spawn.patternType = 0;
        spawn.pos = FixedVec2{ 64'000, 70'000 };
        spawn.vel = FixedVec2{ -250, -150'000 };
        spawn.hp = 1;
        spawn.damage = 2;
        msg.payload = spawn;
        return msg;
    }
    inline std::vector<std::uint8_t> CopyValidJoinBytes()
    {
        return std::vector<std::uint8_t>(
            VALID_JOIN_BYTES.begin(),
            VALID_JOIN_BYTES.end());
    }
}
