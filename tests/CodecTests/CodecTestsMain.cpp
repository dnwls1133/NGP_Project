#include <span>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "../../shared/protocol/PacketCodec.h"
#include "../../shared/protocol/ProtocolTypes.h"
#include "fixtures/ProtocolV1Fixtures.h"

namespace
{
    // 테스트 통과(passed), 실패(failed), 미구현 대기(pending) 개수를 집계하는 구조체
    struct TestCounters
    {
        int passed = 0;
        int failed = 0;
        int pending = 0;
    };

    // 조건이 참이면 통과, 거짓이면 실패를 출력하고 카운터를 올리는 함수
    void Check(bool condition, const std::string_view& name, TestCounters& counters)
    {
        if (condition)
        {
            ++counters.passed;
            std::cout << "[PASS] " << name << std::endl;
            
        }
        else
        {
            ++counters.failed;
            std::cout << "[FAIL] " << name << std::endl;
        }
    }

    // 1. 11/01 공통 계약 상수·구조·실패 시 out 보존 규칙 점검
    void RunContractSanityChecks(TestCounters& counters)
    {
        using namespace protocol;

        Check(MAGIC == 0x5653, "Contract: MAGIC is 0x5653 ('VS')", counters);
        Check(VERSION == 1, "Contract: VERSION is 1", counters);
        Check(HEADER_BYTES == 32, "Contract: HEADER_BYTES is 32", counters);
        Check(MAX_UDP_BYTES == 1200, "Contract: MAX_UDP_BYTES is 1200", counters);
        Check(MAX_TCP_BYTES == 65536, "Contract: MAX_TCP_BYTES is 65536", counters);
        Check(MAX_SNAPSHOT_OBJECTS_PER_PACKET == 36,
            "Contract: Up to 36 object records fit in one 1200B UDP snapshot", counters);

        // 실패 시 기존 출력(out) 값이 보존되는지 확인
        std::vector<std::uint8_t> sentinelBytes = { 0xAA, 0xBB, 0xCC };
        const Message joinMsg = fixtures::MakeValidJoinMessage();
        const CodecError encErr = CPacketCodec::Encode(joinMsg, sentinelBytes);
        if (encErr != CodecError::NONE)
        {
            const std::vector<std::uint8_t> expected = { 0xAA, 0xBB, 0xCC };
            Check(sentinelBytes == expected,
                "Contract: Encode failure preserves existing out vector", counters);
        }

        Message sentinelMsg = fixtures::MakeValidPlayerInputMessage();
        const Message expectedMsg = sentinelMsg;
        const std::uint8_t shortByte = 0x56;
        const CodecError decErr = CPacketCodec::Decode(
            std::span<const std::uint8_t>(&shortByte, 1), sentinelMsg);
        if (decErr != CodecError::NONE)
        {
            Check(sentinelMsg == expectedMsg,
                "Contract: Decode failure preserves existing out Message", counters);
        }
    }

    // 2. 11/02 인코딩 고정 바이트,폭,바이트, 순서, 길이, 필드 범위 시험
    void RunEncodeVectorSuite(TestCounters& counters)
    {
        using namespace protocol;
        std::vector<std::uint8_t> encoded;
        const Message validJoin = fixtures::MakeValidJoinMessage();
        const CodecError probe = CPacketCodec::Encode(validJoin, encoded);
        if (probe == CodecError::UNSUPPORTED_MESSAGE)
        {
            ++counters.pending;
            std::cout << "[PENDING] CPacketCodec::Encode is stubbed.\n";
            return;
        }
        // (1) 고정 정상 바이트 1: JOIN (32바이트 정답 배열과 100% 일치하는지 확인)
        const std::vector<std::uint8_t> expectedJoin(
            fixtures::VALID_JOIN_BYTES.begin(),
            fixtures::VALID_JOIN_BYTES.end());
        Check(probe == CodecError::NONE && encoded == expectedJoin,
            "Encode: JOIN matches fixed 32-byte fixture", counters);
        // (2) 고정 정상 바이트 2: PLAYER_INPUT (40바이트, sequence=0x01020304가 Big-Endian 01 02 03 04로 들어갔는지 확인)
        const Message validInput = fixtures::MakeValidPlayerInputMessage();
        const CodecError encInputErr = CPacketCodec::Encode(validInput, encoded);
        const std::vector<std::uint8_t> expectedInput(
            fixtures::VALID_PLAYER_INPUT_BYTES.begin(),
            fixtures::VALID_PLAYER_INPUT_BYTES.end());
        Check(encInputErr == CodecError::NONE
            && encoded == expectedInput
            && encoded[20] == 0x01 && encoded[21] == 0x02
            && encoded[22] == 0x03 && encoded[23] == 0x04,
            "Encode: PLAYER_INPUT matches fixed 40-byte fixture and big-endian u32", counters);
        // (3) 고정 정상 바이트 3: SPAWN (68바이트, 음수 i32 고정소수점 좌표/속도 직렬화 확인)
        const Message validSpawn = fixtures::MakeValidSpawnMessage();
        const CodecError encSpawnErr = CPacketCodec::Encode(validSpawn, encoded);
        const std::vector<std::uint8_t> expectedSpawn(
            fixtures::VALID_SPAWN_BYTES.begin(),
            fixtures::VALID_SPAWN_BYTES.end());
        Check(encSpawnErr == CodecError::NONE && encoded == expectedSpawn,
            "Encode: SPAWN matches fixed 68-byte fixture (signed i32 FixedVec2)", counters);
        // (4) STATE_SNAPSHOT 경계 시험: 객체 36개 = 정확히 1192바이트 (UDP 1200B 이하 통과)
        {
            Message snapMsg{};
            snapMsg.header.type = MessageType::STATE_SNAPSHOT;
            snapMsg.header.sessionId = 1000;
            snapMsg.header.roundId = 1;
            snapMsg.header.sequence = 10;
            snapMsg.header.hostTick = 300;
            StateSnapshotPayload snap{};
            snap.snapshotTick = 300;
            snap.partIndex = 0;
            snap.partCount = 1;
            snap.objects.resize(MAX_SNAPSHOT_OBJECTS_PER_PACKET);
            for (std::size_t i = 0; i < snap.objects.size(); ++i)
            {
                snap.objects[i].objectId = static_cast<ObjectId>(i + 1);
                snap.objects[i].kind = NetObjectKind::PROJ_MONSTER;
                snap.objects[i].alive = 1;
            }
            snapMsg.payload = snap;
            const CodecError err36 = CPacketCodec::Encode(snapMsg, encoded);
            Check(err36 == CodecError::NONE && encoded.size() == 1192,
                "Encode: STATE_SNAPSHOT with 36 objects encodes to 1192 bytes (<= 1200B)",
                counters);
            // 37개 = 1224바이트 (> 1200B 이므로 TOO_LARGE 에러를 반환하고 기존 out을 보존해야 함)
            const std::vector<std::uint8_t> preserved = encoded;
            std::get<StateSnapshotPayload>(snapMsg.payload).objects.push_back(
                ObjectStateRecord{ .objectId = 37 });
            const CodecError err37 = CPacketCodec::Encode(snapMsg, encoded);
            Check(err37 == CodecError::TOO_LARGE && encoded == preserved,
                "Encode: STATE_SNAPSHOT with 37 objects returns TOO_LARGE and preserves out",
                counters);
        }
        // (5) 인코딩 입력 검증: header.type(ACCEPT)과 payload 종류(JoinPayload) 불일치 거부
        {
            const std::vector<std::uint8_t> preserved = encoded;
            Message mismatchMsg = fixtures::MakeValidJoinMessage();
            mismatchMsg.header.type = MessageType::ACCEPT;
            Check(CPacketCodec::Encode(mismatchMsg, encoded) == CodecError::INVALID_FIELD
                && encoded == preserved,
                "Encode: Header/payload type mismatch returns INVALID_FIELD", counters);
        }
        // (6) 인코딩 입력 검증: JOIN에서 sessionId != 0 거부
        {
            const std::vector<std::uint8_t> preserved = encoded;
            Message badJoin = fixtures::MakeValidJoinMessage();
            badJoin.header.sessionId = 99;
            Check(CPacketCodec::Encode(badJoin, encoded) == CodecError::INVALID_FIELD
                && encoded == preserved,
                "Encode: JOIN with non-zero sessionId returns INVALID_FIELD", counters);
        }
        // (7) 인코딩 입력 검증: PLAYER_INPUT 미정의 moveBits 상위 비트(0x10) 거부
        {
            const std::vector<std::uint8_t> preserved = encoded;
            Message badInput = fixtures::MakeValidPlayerInputMessage();
            std::get<PlayerInputPayload>(badInput.payload).moveBits = 0x10;
            Check(CPacketCodec::Encode(badInput, encoded) == CodecError::INVALID_FIELD
                && encoded == preserved,
                "Encode: PLAYER_INPUT with invalid moveBits returns INVALID_FIELD", counters);
        }
    }
    // 3. 11/03~11/04 디코딩 및 오류 분류 시험 묶음 (내일 Decode 구현 시 자동 활성화)
    void RunDecodeVectorSuite(TestCounters& counters)
    {
        using namespace protocol;
        Message decoded{};
        const CodecError probe = CPacketCodec::Decode(
            fixtures::VALID_JOIN_BYTES, decoded);
        if (probe == CodecError::UNSUPPORTED_MESSAGE)
        {
            ++counters.pending;
            std::cout << "[PENDING] CPacketCodec::Decode is stubbed "
                "(scheduled for 11/03~11/04).\n";
            return;
        }
        const Message validJoin = fixtures::MakeValidJoinMessage();
        Check(probe == CodecError::NONE && decoded == validJoin,
            "Decode: Fixed 32-byte JOIN matches expected fields", counters);
        const Message validInput = fixtures::MakeValidPlayerInputMessage();
        const CodecError decInputErr = CPacketCodec::Decode(
            fixtures::VALID_PLAYER_INPUT_BYTES, decoded);
        Check(decInputErr == CodecError::NONE && decoded == validInput,
            "Decode: Fixed 40-byte PLAYER_INPUT matches expected fields", counters);
        const Message validSpawn = fixtures::MakeValidSpawnMessage();
        const CodecError decSpawnErr = CPacketCodec::Decode(
            fixtures::VALID_SPAWN_BYTES, decoded);
        Check(decSpawnErr == CodecError::NONE && decoded == validSpawn,
            "Decode: Fixed 68-byte SPAWN matches expected fields", counters);
        // 왕복(Encode -> Decode) 검증: STATE_SNAPSHOT, COMBAT_EVENT
        {
            Message combatMsg{};
            combatMsg.header.type = MessageType::COMBAT_EVENT;
            combatMsg.header.totalBytes = 60;
            combatMsg.header.sessionId = 1000;
            combatMsg.header.roundId = 1;
            combatMsg.header.sequence = 25;
            combatMsg.header.hostTick = 450;

            CombatEventPayload combat{};
            combat.eventId = 9;
            combat.kind = CombatEventKind::DAMAGE_APPLIED;
            combat.instigatorPlayerId = 0;
            combat.updatedPlayerLevel = 2;
            combat.sourceObjectId = 101;
            combat.targetObjectId = 202;
            combat.confirmedHp = 35;
            combat.scoreDelta = 100;
            combat.teamScore = 1500;
            combatMsg.payload = combat;

            std::vector<std::uint8_t> wire;
            Message roundTripOut{};
            Check(CPacketCodec::Encode(combatMsg, wire) == CodecError::NONE
                && CPacketCodec::Decode(wire, roundTripOut) == CodecError::NONE
                && roundTripOut == combatMsg,
                "Decode: COMBAT_EVENT round-trip preserves all header and payload fields",
                counters);

        }

        // 본문 필드 오류 시 부분 결과 노출 방지 (헤더는 정상이나 본문 moveBits가 잘못된 경우 out 보존
        {
            auto badBodyBytes = std::vector<std::uint8_t>(
                fixtures::VALID_PLAYER_INPUT_BYTES.begin(),
                fixtures::VALID_PLAYER_INPUT_BYTES.end());
            badBodyBytes[33] = 0x80; // 허용되지 않는 moveBits 주입

            Message out = validJoin;
            Check(CPacketCodec::Decode(badBodyBytes, out) == CodecError::INVALID_FIELD
                && out == validJoin,
                "Decode: Body validation failure does not expose partial header to out",
                counters);
        }


        // 단축 입력: 길이 0~31 모두 TRUNCATED
        bool allTruncatedRejected = true;
        for (std::size_t len = 0; len < HEADER_BYTES; ++len)
        {
            Message out = validInput;
            const std::span<const std::uint8_t> slice(
                fixtures::VALID_JOIN_BYTES.data(), len);
            if (CPacketCodec::Decode(slice, out) != CodecError::TRUNCATED
                || !(out == validInput))
            {
                allTruncatedRejected = false;
                break;
            }
        }
        Check(allTruncatedRejected,
            "Decode: Length 0..31 returns TRUNCATED and preserves out", counters);
        // magic 오류
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[0] = 0x00;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::BAD_MAGIC
                && out == validInput,
                "Decode: Byte 0 mismatch returns BAD_MAGIC", counters);
        }
        // 버전 오류 (version = 2)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[3] = 0x02;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::UNSUPPORTED_VERSION
                && out == validInput,
                "Decode: version=2 returns UNSUPPORTED_VERSION", counters);
        }
        // 종류 오류 (type = 65535)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[4] = 0xFF;
            bytes[5] = 0xFF;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::UNKNOWN_TYPE
                && out == validInput,
                "Decode: type=65535 returns UNKNOWN_TYPE", counters);
        }
        // 미정 플래그 비트 (flags = 1)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[7] = 0x01;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_FLAGS
                && out == validInput,
                "Decode: flags=1 returns INVALID_FLAGS", counters);
        }
        // 예약 필드 오류 (reserved = 1)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[31] = 0x01;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_RESERVED
                && out == validInput,
                "Decode: reserved=1 returns INVALID_RESERVED", counters);
        }
        // 전체 길이 오류 (totalBytes = 31)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[11] = 0x1F;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_LENGTH
                && out == validInput,
                "Decode: totalBytes=31 returns INVALID_LENGTH", counters);
        }
        // 뜻밖의 본문 (빈 본문 JOIN에 1바이트 추가 + totalBytes도 33으로 변경)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[11] = 0x21; // 33
            bytes.push_back(0x00);
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_LENGTH
                && out == validInput,
                "Decode: 33-byte JOIN with unexpected body returns INVALID_LENGTH", counters);
        }
        // UDP 크기 한계 (STATE_SNAPSHOT으로 totalBytes = 1201)
        {
            std::vector<std::uint8_t> bytes(1201, 0);
            bytes[0] = 0x56; bytes[1] = 0x53;
            bytes[2] = 0x00; bytes[3] = 0x01;
            bytes[4] = 0x00; bytes[5] = 0x0C; // STATE_SNAPSHOT (12)
            bytes[8] = 0x00; bytes[9] = 0x00; bytes[10] = 0x04; bytes[11] = 0xB1; // 1201
            bytes[19] = 0x01; // roundId = 1
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::TOO_LARGE
                && out == validInput,
                "Decode: 1201-byte UDP message returns TOO_LARGE", counters);
        }
        // TCP 크기 한계 (SPAWN으로 totalBytes = 65537)
        {
            std::vector<std::uint8_t> bytes(65537, 0);
            bytes[0] = 0x56; bytes[1] = 0x53;
            bytes[2] = 0x00; bytes[3] = 0x01;
            bytes[4] = 0x00; bytes[5] = 0x0D; // SPAWN (13)
            bytes[8] = 0x00; bytes[9] = 0x01; bytes[10] = 0x00; bytes[11] = 0x01; // 65537
            bytes[19] = 0x01; // roundId = 1
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::TOO_LARGE
                && out == validInput,
                "Decode: 65537-byte TCP message returns TOO_LARGE", counters);
        }
    }
}


int main()
{
    TestCounters counters{};

    std::cout << "=== Virus Striker Codec Tests (Protocol v1) ===\n";
    RunContractSanityChecks(counters);
    RunEncodeVectorSuite(counters);
    RunDecodeVectorSuite(counters);


    std::cout << "----------------------------------------------\n";
    std::cout << "Passed:  " << counters.passed << '\n';
    std::cout << "Failed:  " << counters.failed << '\n';
    std::cout << "Pending: " << counters.pending << '\n';
    return (counters.failed == 0 && counters.pending == 0) ? 0 : 1;
}
