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

    // ─────────────────────────────────────────────────────────────────────────
    // 11/04 오류 분류 및 거부(Reject) 고정 바이트 Fixture 모음
    // ─────────────────────────────────────────────────────────────────────────

    // 거부 Fixture 1: 31바이트 단축 헤더 (32B 미만 -> CodecError::TRUNCATED)
    inline constexpr std::array<std::uint8_t, 31> REJECT_TRUNCATED_31_BYTES = {
        0x56, 0x53, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    // 거부 Fixture 2: 잘못된 매직 넘버 (0x0000 != 0x5653 -> CodecError::BAD_MAGIC)
    inline constexpr std::array<std::uint8_t, 32> REJECT_BAD_MAGIC_BYTES = {
        0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    // 거부 Fixture 3: 미지원 버전 0 및 2 (version != 1 -> CodecError::UNSUPPORTED_VERSION)
    inline constexpr std::array<std::uint8_t, 32> REJECT_UNSUPPORTED_VERSION_0_BYTES = {
        0x56, 0x53, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    inline constexpr std::array<std::uint8_t, 32> REJECT_UNSUPPORTED_VERSION_2_BYTES = {
        0x56, 0x53, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    // 거부 Fixture 4: 미지원 메시지 종류 0 및 20 (1~19 범위 밖 -> CodecError::UNKNOWN_TYPE)
    inline constexpr std::array<std::uint8_t, 32> REJECT_UNKNOWN_TYPE_0_BYTES = {
        0x56, 0x53, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    inline constexpr std::array<std::uint8_t, 32> REJECT_UNKNOWN_TYPE_20_BYTES = {
        0x56, 0x53, 0x00, 0x01, 0x00, 0x14, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    // 거부 Fixture 5: 헤더 totalBytes(40B)와 실제 수신 프레임 길이(39B) 불일치 (-> CodecError::INVALID_LENGTH)
    inline constexpr std::array<std::uint8_t, 39> REJECT_TOTAL_BYTES_FRAME_MISMATCH_BYTES = {
        0x56, 0x53, 0x00, 0x01, 0x00, 0x0B, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x03, 0xE8,
        0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x00, 0x01, 0x02, 0x03
    };
    // 거부 Fixture 6: PLAYER_INPUT(규격상 40B)인데 totalBytes=36B, 프레임 길이도 36B인 본문 길이 불일치 (-> CodecError::INVALID_LENGTH)
    inline constexpr std::array<std::uint8_t, 36> REJECT_TYPE_LENGTH_MISMATCH_BYTES = {
        0x56, 0x53, 0x00, 0x01, 0x00, 0x0B, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x03, 0xE8,
        0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x00
    };
    // 거부 Fixture 7: STATE_SNAPSHOT 총 72B(객체 1개 크기)인데 본문 objectCount=2로 기록된 길이 불일치 (-> CodecError::INVALID_LENGTH)
    inline constexpr std::array<std::uint8_t, 72> REJECT_SNAPSHOT_OBJECT_COUNT_MISMATCH_BYTES = {
        // Header (32B): type = STATE_SNAPSHOT(12), totalBytes = 72 (0x48)
        0x56, 0x53, 0x00, 0x01, 0x00, 0x0C, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x03, 0xE8,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00,
        // Snapshot Header (8B): snapshotTick=100, partIndex=0, partCount=1, objectCount=2(불일치!)
        0x00, 0x00, 0x00, 0x64, 0x00, 0x01, 0x00, 0x02,
        // Object Record 1개분 (32B)
        0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x27, 0x10, 0x00, 0x00, 0x4E, 0x20,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03
    };

    // ────────────────────────────────────────────────────────────
    // 11/05 세션·시작 메시지 고정 바이트 Fixture 모음
    // ────────────────────────────────────────────────────────────

    // 고정 정상 바이트 4: ACCEPT (44바이트)
    // sessionId = 1000(0x000003E8), sequence = 1, assignedPlayerId = 1, maxPlayers = 2,
    // reserved = 0, udpToken = 0x0102030405060708
    inline constexpr std::array<std::uint8_t, 44> VALID_ACCEPT_BYTES = {
        // Header (32B): type = ACCEPT(2), totalBytes = 44 (0x2C)
        0x56, 0x53, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x03, 0xE8,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Payload (12B)
        0x01, 0x02, 0x00, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };

    inline Message MakeValidAcceptMessage()
    {
        Message msg{};
        msg.header.magic = MAGIC;
        msg.header.version = VERSION;
        msg.header.type = MessageType::ACCEPT;
        msg.header.flags = FLAGS_NONE;
        msg.header.totalBytes = 44;
        msg.header.sessionId = 1000;
        msg.header.roundId = 1;
        msg.header.sequence = 1;
        msg.header.hostTick = 0;
        msg.header.reserved = RESERVED_VALUE;

        AcceptPayload accept{};
        accept.assignedPlayerId = 1;
        accept.maxPlayers = MAX_PROTOCOL_PLAYERS;
        accept.reserved = 0;
        accept.udpToken = 0x0102030405060708ULL;
        msg.payload = accept;
        return msg;
    }

    // 고정 정상 바이트 5: START (44바이트)
    // sessionId = 1000(0x000003E8), sequence = 5, hostTick = 120(0x78),
    // startHostTick = 120(0x78), simRateHz = 60(0x003C), stateRateHz = 20(0x0014), inputRateHz = 30(0x001E)
    inline constexpr std::array<std::uint8_t, 44> VALID_START_BYTES = {
        // Header (32B): type = START(8), totalBytes = 44 (0x2C)
        0x56, 0x53, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x2C, 0x00, 0x00, 0x03, 0xE8,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05,
        0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00,
        // Payload (12B)
        0x00, 0x00, 0x00, 0x78, // startHostTick = 120
        0x00, 0x3C,             // simRateHz = 60
        0x00, 0x14,             // stateRateHz = 20
        0x00, 0x1E,             // inputRateHz = 30
        0x00, 0x00              // reserved = 0
    };
    inline Message MakeValidStartMessage()
    {
        Message msg{};
        msg.header.magic = MAGIC;
        msg.header.version = VERSION;
        msg.header.type = MessageType::START;
        msg.header.flags = FLAGS_NONE;
        msg.header.totalBytes = 44;
        msg.header.sessionId = 1000;
        msg.header.roundId = 1;
        msg.header.sequence = 5;
        msg.header.hostTick = 120;
        msg.header.reserved = RESERVED_VALUE;
        StartPayload start{};
        start.startHostTick = 120;
        start.simRateHz = 60;
        start.stateRateHz = 20;
        start.inputRateHz = 30;
        start.reserved = 0;
        msg.payload = start;
        return msg;
    }
}
