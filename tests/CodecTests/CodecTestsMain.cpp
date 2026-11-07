#include <span>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "../../shared/protocol/PacketCodec.h"
#include "../../shared/protocol/ProtocolTypes.h"
#include "../../net/SendContract.h"
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

    // 4. 11/04 짧은 헤더·길이·버전·종류·필드 범위 검사 및 최대 크기 경계 시험 묶음
    void RunValidationAndBoundarySuite(TestCounters& counters)
    {
        using namespace protocol;

        const Message sentinelMsg = fixtures::MakeValidPlayerInputMessage();
        const PacketHeader sentinelHdr = sentinelMsg.header;

        // (1) 31바이트 짧은 헤더 거부 fixture 확인 (DecodeHeader 및 Decod 모두 TRUNCATED)
        {
            PacketHeader hdrOut = sentinelHdr;
            Message msgOut = sentinelMsg;
            Check(CPacketCodec::DecodeHeader(fixtures::REJECT_TRUNCATED_31_BYTES, hdrOut)
                == CodecError::TRUNCATED
                && hdrOut == sentinelHdr
                && CPacketCodec::Decode(fixtures::REJECT_TRUNCATED_31_BYTES, msgOut)
                == CodecError::TRUNCATED
                && msgOut == sentinelMsg, "Boundary1104: 31-byte header fixture rejected with TRUNCATED in DecodeHeader & Decode",
                counters);
        }

        // (2) 매직, 미지원 버전(0, 2) 미지원 종류(0,20) 거부 fixture 확인
        {
            Message out = sentinelMsg;
            Check(CPacketCodec::Decode(fixtures::REJECT_BAD_MAGIC_BYTES, out) ==
            CodecError::BAD_MAGIC
                && out == sentinelMsg, "Boundary1104: REJECT_BAD_MAGIC_BYTES returns BAD_MAGIC"
                , counters);
            Check(CPacketCodec::Decode(fixtures::REJECT_UNSUPPORTED_VERSION_0_BYTES, out) == CodecError::UNSUPPORTED_VERSION
                && CPacketCodec::Decode(fixtures::REJECT_UNSUPPORTED_VERSION_2_BYTES, out) == CodecError::UNSUPPORTED_VERSION
                && out == sentinelMsg,
                "Boundary1104: Version 0 and 2 fixtures return UNSUPPORTED_VERSION", counters);
            Check(CPacketCodec::Decode(fixtures::REJECT_UNKNOWN_TYPE_0_BYTES, out) == CodecError::UNKNOWN_TYPE
                && CPacketCodec::Decode(fixtures::REJECT_UNKNOWN_TYPE_20_BYTES, out) == CodecError::UNKNOWN_TYPE
                && out == sentinelMsg,
                "Boundary1104: Type 0 and 20 fixtures return UNKNOWN_TYPE", counters);

        }

        // (3) 길이 불일치 거부 fixture 3종 (프레임 길이 불일치 / 메시지별 규격 길이 불일치 / 스냅샷 objectCount 불일치)
        {
            Message out = sentinelMsg;
            PacketHeader hdrOut = sentinelHdr;
            Check(CPacketCodec::Decode(fixtures::REJECT_TOTAL_BYTES_FRAME_MISMATCH_BYTES, out) == CodecError::INVALID_LENGTH
                && out == sentinelMsg,
                "Boundary1104: Header totalBytes(40) != frame size(39) returns INVALID_LENGTH", counters);
            Check(CPacketCodec::DecodeHeader(fixtures::REJECT_TYPE_LENGTH_MISMATCH_BYTES, hdrOut) == CodecError::INVALID_LENGTH
                && hdrOut == sentinelHdr
                && CPacketCodec::Decode(fixtures::REJECT_TYPE_LENGTH_MISMATCH_BYTES, out) == CodecError::INVALID_LENGTH
                && out == sentinelMsg,
                "Boundary1104: PLAYER_INPUT with 36-byte length returns INVALID_LENGTH", counters);
            Check(CPacketCodec::Decode(fixtures::REJECT_SNAPSHOT_OBJECT_COUNT_MISMATCH_BYTES, out) == CodecError::INVALID_LENGTH
                && out == sentinelMsg,
                "Boundary1104: STATE_SNAPSHOT objectCount mismatch returns INVALID_LENGTH", counters);
        }

        // (4) 복합 오류 시 PROTOCOL_V1.md 4절 판정 우선순위 확인
        //     본문 길이 불일치(10순위 INVALID_LENGTH) + roundId=0(11순위 INVALID_FIELD) 동시 발생 시 INVALID_LENGTH 우선
        {
            auto bytes = std::vector<std::uint8_t>(
                fixtures::REJECT_TYPE_LENGTH_MISMATCH_BYTES.begin(),
                fixtures::REJECT_TYPE_LENGTH_MISMATCH_BYTES.end());
            bytes[19] = 0x00; // roundId = 0 (INVALID_FIELD 조건 동시 주입)

            Message out = sentinelMsg;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_LENGTH
                && out == sentinelMsg,
                "Boundary1104: Length mismatch takes precedence over roundId=0 (INVALID_LENGTH before INVALID_FIELD)",
                counters);
        }

        // (5) UDP / TCP 최대 크기 경계 확인 (1200B vs 1201B, 65536B vs 65537B)
        {
            Message out = sentinelMsg;

            // UDP 정확히 1200B 프레임: 크기 상한(TOO_LARGE)은 통과하고 스냅샷 규격 길이
            // (1200-40=1160은 32의 배수 아님)로 INVALID_LENGTH 판정
            std::vector<std::uint8_t> udp1200(MAX_UDP_BYTES, 0);
            udp1200[0] = 0x56; udp1200[1] = 0x53;
            udp1200[2] = 0x00; udp1200[3] = 0x01;
            udp1200[4] = 0x00; udp1200[5] = 0x0C; // STATE_SNAPSHOT (UDP)
            udp1200[8] = 0x00; udp1200[9] = 0x00; udp1200[10] = 0x04; udp1200[11] = 0xB0; // 1200
            udp1200[19] = 0x01; // roundId = 1

            // TCP 정확히 65536B 프레임: 크기 상한(TOO_LARGE)은 통과하고 SPAWN 규격 길이(68B)
            // INVALID_LENGTH 판정
            std::vector<std::uint8_t> tcp65536(MAX_TCP_BYTES, 0);
            tcp65536[0] = 0x56; tcp65536[1] = 0x53;
            tcp65536[2] = 0x00; tcp65536[3] = 0x01;
            tcp65536[4] = 0x00; tcp65536[5] = 0x0D; // SPAWN (TCP)
            tcp65536[8] = 0x00; tcp65536[9] = 0x01; tcp65536[10] = 0x00; tcp65536[11] = 0x00; // 65536
            tcp65536[19] = 0x01; // roundId = 1

            Check(CPacketCodec::Decode(udp1200, out) == CodecError::INVALID_LENGTH
                && CPacketCodec::Decode(tcp65536, out) == CodecError::INVALID_LENGTH
                && out == sentinelMsg,
                "Boundary1104: Exact max size (UDP 1200B, TCP 65536B) is not rejected as TOO_LARGE",
                counters);
        }
    }

    // 5. 11/05 세션/플레이어/틱 필드 타입과 세션 메시지별 구조·경계값 시험 묶음
    void RunSessionMessageSuite(TestCounters& counters)
    {
        using namespace protocol;

        const Message sentinelMsg = fixtures::MakeValidPlayerInputMessage();
        std::vector<std::uint8_t> wire;
        Message decoded = sentinelMsg;
        // (1) 고정 바이트 대조: ACCEPT (44B) 및 START (44B)
        {
            const Message validAccept = fixtures::MakeValidAcceptMessage();
            const std::vector<std::uint8_t> expectedAccept(
                fixtures::VALID_ACCEPT_BYTES.begin(),
                fixtures::VALID_ACCEPT_BYTES.end());
            Check(CPacketCodec::Encode(validAccept, wire) == CodecError::NONE
                && wire == expectedAccept
                && CPacketCodec::Decode(fixtures::VALID_ACCEPT_BYTES, decoded) == CodecError::NONE
                && decoded == validAccept,
                "Session1105: ACCEPT matches fixed 44-byte fixture in Encode & Decode", counters);
            const Message validStart = fixtures::MakeValidStartMessage();
            const std::vector<std::uint8_t> expectedStart(
                fixtures::VALID_START_BYTES.begin(),
                fixtures::VALID_START_BYTES.end());
            Check(CPacketCodec::Encode(validStart, wire) == CodecError::NONE
                && wire == expectedStart
                && CPacketCodec::Decode(fixtures::VALID_START_BYTES, decoded) == CodecError::NONE
                && decoded == validStart,
                "Session1105: START matches fixed 44-byte fixture in Encode & Decode", counters);
        }
        // (2) sessionId 구조 경계값 검사 (JOIN은 0만 허용, 승인 후 메시지는 0 거부 / 비영 허용)
        //     실제 활성 세션 번호 일치 여부는 CSessionMgr가 담당하고 코덱은 0 여부만 구조 검사함
        {
            Message acceptZeroSession = fixtures::MakeValidAcceptMessage();
            acceptZeroSession.header.sessionId = 0;
            auto rawAcceptZeroSession = std::vector<std::uint8_t>(
                fixtures::VALID_ACCEPT_BYTES.begin(),
                fixtures::VALID_ACCEPT_BYTES.end());
            rawAcceptZeroSession[12] = 0x00;
            rawAcceptZeroSession[13] = 0x00;
            rawAcceptZeroSession[14] = 0x00;
            rawAcceptZeroSession[15] = 0x00;
            Message out = sentinelMsg;
            Check(CPacketCodec::Encode(acceptZeroSession, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Decode(rawAcceptZeroSession, out) == CodecError::INVALID_FIELD
                && out == sentinelMsg,
                "Session1105: Non-JOIN session message with sessionId=0 returns INVALID_FIELD",
                counters);
            // sessionId 최대 경계값(0xFFFFFFFF)과 hostTick 최대 경계값(0xFFFFFFFF) 왕복 확인
            Message maxBoundaryStart = fixtures::MakeValidStartMessage();
            maxBoundaryStart.header.sessionId = 0xFFFFFFFFu;
            maxBoundaryStart.header.sequence = 0xFFFFFFFFu;
            maxBoundaryStart.header.hostTick = 0xFFFFFFFFu;
            std::get<StartPayload>(maxBoundaryStart.payload).startHostTick = 0xFFFFFFFFu;
            Message roundTrip{};
            Check(CPacketCodec::Encode(maxBoundaryStart, wire) == CodecError::NONE
                && CPacketCodec::Decode(wire, roundTrip) == CodecError::NONE
                && roundTrip == maxBoundaryStart,
                "Session1105: u32 max boundary (0xFFFFFFFF) preserved for sessionId, sequence, and ticks",
                counters);
        }
        // (3) 로비·세션 생명주기 메시지(REJECT, READY, ROOM_STATE, LOAD, LOADED, HEARTBEAT, LEAVE, END) 왕복 검증
        {
            auto checkRoundTrip = [&](Message msg, std::uint32_t expectedBytes) -> bool
                {
                    msg.header.totalBytes = expectedBytes;
                    std::vector<std::uint8_t> bytes;
                    Message out{};
                    return CPacketCodec::Encode(msg, bytes) == CodecError::NONE
                        && bytes.size() == expectedBytes
                        && CPacketCodec::Decode(bytes, out) == CodecError::NONE
                        && out == msg;
                };
            Message rejectMsg{};
            rejectMsg.header.type = MessageType::REJECT;
            rejectMsg.header.sessionId = 0; // 발급 전 거절(sessionId=0) 허용 확인
            rejectMsg.payload = RejectPayload{ .reason = RejectReason::ROOM_FULL };
            Message readyMsg{};
            readyMsg.header.type = MessageType::READY;
            readyMsg.header.sessionId = 1000;
            readyMsg.payload = ReadyPayload{ .playerId = 1, .isReady = 1 };
            Message roomMsg{};
            roomMsg.header.type = MessageType::ROOM_STATE;
            roomMsg.header.sessionId = 1000;
            RoomStatePayload room{};
            room.slots[0] = RoomSlotState{ .playerId = 0, .connected = 1, .ready = 1, .udpRegistered = 1 };
            room.slots[1] = RoomSlotState{ .playerId = 1, .connected = 1, .ready = 0, .udpRegistered = 1 };
            roomMsg.payload = room;
            Message loadMsg{};
            loadMsg.header.type = MessageType::LOAD;
            loadMsg.header.sessionId = 1000;
            loadMsg.payload = LoadPayload{ .stageId = 1 };
            Message loadedMsg{};
            loadedMsg.header.type = MessageType::LOADED;
            loadedMsg.header.sessionId = 1000;
            loadedMsg.payload = LoadedPayload{ .playerId = 0 };
            Message hbMsg{};
            hbMsg.header.type = MessageType::HEARTBEAT;
            hbMsg.header.sessionId = 1000;
            hbMsg.payload = HeartbeatPayload{};
            Message leaveMsg{};
            leaveMsg.header.type = MessageType::LEAVE;
            leaveMsg.header.sessionId = 1000;
            leaveMsg.payload = LeavePayload{ .playerId = 1, .reason = DisconnectReason::USER_LEFT };
            Message endMsg{};
            endMsg.header.type = MessageType::END;
            endMsg.header.sessionId = 1000;
            endMsg.payload = EndPayload{ .reason = DisconnectReason::NORMAL };
            Check(checkRoundTrip(rejectMsg, 36)
                && checkRoundTrip(readyMsg, 36)
                && checkRoundTrip(roomMsg, 40)
                && checkRoundTrip(loadMsg, 36)
                && checkRoundTrip(loadedMsg, 36)
                && checkRoundTrip(hbMsg, 32)
                && checkRoundTrip(leaveMsg, 36)
                && checkRoundTrip(endMsg, 36),
                "Session1105: All lobby/session lifecycle messages round-trip with exact sizes",
                counters);
        }
        // (4) 플레이어 슬롯(0, 1 허용 / 2, 0xFF 거부) 및 ROOM_STATE 슬롯 순서 경계 검사
        {
            Message badReady{};
            badReady.header.type = MessageType::READY;
            badReady.header.sessionId = 1000;
            badReady.payload = ReadyPayload{ .playerId = 2, .isReady = 1 };
            Message badReadyInvalidSlot{};
            badReadyInvalidSlot.header.type = MessageType::READY;
            badReadyInvalidSlot.header.sessionId = 1000;
            badReadyInvalidSlot.payload = ReadyPayload{ .playerId = INVALID_PLAYER_SLOT, .isReady = 1 };
            Message badRoom{};
            badRoom.header.type = MessageType::ROOM_STATE;
            badRoom.header.sessionId = 1000;
            RoomStatePayload swappedRoom{};
            swappedRoom.slots[0].playerId = 1; // 슬롯 0 자리에 playerId=1이 들어가면 거부되어야 함
            swappedRoom.slots[1].playerId = 0;
            badRoom.payload = swappedRoom;
            Check(CPacketCodec::Encode(badReady, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badReadyInvalidSlot, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badRoom, wire) == CodecError::INVALID_FIELD,
                "Session1105: Out-of-range PlayerSlot (2, 0xFF) and swapped ROOM_STATE slots return INVALID_FIELD",
                counters);
        }
        // (5) LOAD stageId(=1만 허용) 및 START 주기(0Hz 거부) 경계 검사
        {
            Message badLoad0{};
            badLoad0.header.type = MessageType::LOAD;
            badLoad0.header.sessionId = 1000;
            badLoad0.payload = LoadPayload{ .stageId = 0 };
            Message badLoad2{};
            badLoad2.header.type = MessageType::LOAD;
            badLoad2.header.sessionId = 1000;
            badLoad2.payload = LoadPayload{ .stageId = 2 };
            Message badStartHz = fixtures::MakeValidStartMessage();
            std::get<StartPayload>(badStartHz.payload).simRateHz = 0;
            Check(CPacketCodec::Encode(badLoad0, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badLoad2, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badStartHz, wire) == CodecError::INVALID_FIELD,
                "Session1105: LOAD stageId!=1 and START 0Hz rate return INVALID_FIELD",
                counters);
        }
    }

    // 6. 11/05 상태 배치(SnapshotBatch) 구성·소유권·실패 기대 결과 시험 묶음
    //    (THREAD_QUEUE_BATCH_CONTRACT_1105.md  3~5절 및 9절 804~806 검증)
    void RunSnapshotBatchContractSuite(TestCounters& counters)
    {
        using namespace protocol;

        // 테스트용 정상 스냅샷 조각 생성
        auto makeSnapshotPart = [](
            std::uint32_t sessionId,
            std::uint32_t tick,
            std::uint8_t partIndex,
            std::uint8_t partCount,
            std::initializer_list<ObjectId> objectIds)->Message
            {
                Message msg{};
                msg.header.type = MessageType::STATE_SNAPSHOT;
                msg.header.sessionId = sessionId;
                msg.header.roundId = 1;
                msg.header.sequence = tick;
                msg.header.hostTick = tick;

                StateSnapshotPayload snap{};
                snap.snapshotTick = tick;
                snap.partIndex = partIndex;
                snap.partCount = partCount;
                for (ObjectId id : objectIds) {
                    ObjectStateRecord rec{};
                    rec.objectId = id;
                    rec.kind = NetObjectKind::MONSTER;
                    rec.subType = 0;
                    rec.alive = 1;
                    rec.hp = 10;
                    rec.maxHp = 10;
                    snap.objects.push_back(rec);
                }
                msg.header.totalBytes = static_cast<std::uint32_t>(
                    HEADER_BYTES + SNAPSHOT_HEADER_BYTES
                    + snap.objects.size() * OBJECT_STATE_RECORD_BYTES);

                msg.payload = std::move(snap);
                return msg;
            };

        // (1) 단건 EnqueueSend 경로 검증: STATE_SNAPSHOT 단건 등록 거부 및 채널 불일치 거부
        {
            const Message validInput = fixtures::MakeValidPlayerInputMessage();
            const Message validJoin = fixtures::MakeValidJoinMessage();
            const Message singleSnap = makeSnapshotPart(1000, 10, 0, 1, { 1, 2 });
            Check(net::ValidateSendRequest(net::Channel::Udp, validInput) == net::SendResult::Queued
                && net::ValidateSendRequest(net::Channel::Tcp, validJoin) == net::SendResult::Queued
                && net::ValidateSendRequest(net::Channel::Tcp, validInput) == net::SendResult::InvalidRequest
                && net::ValidateSendRequest(net::Channel::Udp, validJoin) == net::SendResult::InvalidRequest
                && net::ValidateSendRequest(net::Channel::Udp, singleSnap) == net::SendResult::InvalidRequest,
                "Batch1105: EnqueueSend rejects STATE_SNAPSHOT and mismatched Tcp/Udp channel with InvalidRequest",
                counters);
        }
        // (2) 정상 배치 검증: 객체 0개(빈 상태 1조각) 배치 및 2조각 분할 배치 통과
        {
            net::SnapshotBatch emptyWorldBatch{};
            emptyWorldBatch.parts.push_back(makeSnapshotPart(1000, 10, 0, 1, {}));
            net::SnapshotBatch twoPartBatch{};
            twoPartBatch.parts.push_back(makeSnapshotPart(1000, 11, 0, 2, { 1, 2 }));
            twoPartBatch.parts.push_back(makeSnapshotPart(1000, 11, 1, 2, { 3, 4 }));
            Check(net::ValidateSnapshotBatch(emptyWorldBatch) == net::SendResult::Queued
                && net::ValidateSnapshotBatch(twoPartBatch) == net::SendResult::Queued,
                "Batch1105: Valid empty-world (N=1) and multi-part (N=2) SnapshotBatch pass validation",
                counters);
        }
        // (3) B04 검증: 두 조각 배치 중 하나의 sessionId만 다르거나 hostTick/snapshotTick 불일치 시 InvalidRequest
        {
            net::SnapshotBatch mismatchedSessionBatch{};
            mismatchedSessionBatch.parts.push_back(makeSnapshotPart(1000, 10, 0, 2, { 1 }));
            mismatchedSessionBatch.parts.push_back(makeSnapshotPart(2000, 10, 1, 2, { 2 }));
            net::SnapshotBatch mismatchedTickBatch{};
            mismatchedTickBatch.parts.push_back(makeSnapshotPart(1000, 10, 0, 2, { 1 }));
            mismatchedTickBatch.parts.push_back(makeSnapshotPart(1000, 11, 1, 2, { 2 }));
            net::SnapshotBatch mismatchedPayloadTickBatch{};
            mismatchedPayloadTickBatch.parts.push_back(makeSnapshotPart(1000, 10, 0, 1, { 1 }));
            std::get<StateSnapshotPayload>(mismatchedPayloadTickBatch.parts[0].payload).snapshotTick = 9;
            Check(net::ValidateSnapshotBatch(mismatchedSessionBatch) == net::SendResult::InvalidRequest
                && net::ValidateSnapshotBatch(mismatchedTickBatch) == net::SendResult::InvalidRequest
                && net::ValidateSnapshotBatch(mismatchedPayloadTickBatch) == net::SendResult::InvalidRequest,
                "Batch1105 (B04): Mismatched sessionId or tick across batch parts returns InvalidRequest",
                counters);
        }
        // (4) B05 및 ObjectId 중복 검증: 빈 배치, partIndex 순서 오류, partCount 불일치, 배치 내 동일 ObjectId 중복
        {
            net::SnapshotBatch emptyBatch{};
            net::SnapshotBatch badPartIndexBatch{};
            badPartIndexBatch.parts.push_back(makeSnapshotPart(1000, 10, 0, 2, { 1 }));
            badPartIndexBatch.parts.push_back(makeSnapshotPart(1000, 10, 0, 2, { 2 })); // partIndex 중복(0, 0)
            net::SnapshotBatch badPartCountBatch{};
            badPartCountBatch.parts.push_back(makeSnapshotPart(1000, 10, 0, 3, { 1 })); // parts.size()=2인데 partCount=3
            badPartCountBatch.parts.push_back(makeSnapshotPart(1000, 10, 1, 3, { 2 }));
            net::SnapshotBatch duplicateObjectIdBatch{};
            duplicateObjectIdBatch.parts.push_back(makeSnapshotPart(1000, 10, 0, 2, { 1, 2 }));
            duplicateObjectIdBatch.parts.push_back(makeSnapshotPart(1000, 10, 1, 2, { 2, 3 })); // ObjectId 2 중복!
            Check(net::ValidateSnapshotBatch(emptyBatch) == net::SendResult::InvalidRequest
                && net::ValidateSnapshotBatch(badPartIndexBatch) == net::SendResult::InvalidRequest
                && net::ValidateSnapshotBatch(badPartCountBatch) == net::SendResult::InvalidRequest
                && net::ValidateSnapshotBatch(duplicateObjectIdBatch) == net::SendResult::InvalidRequest,
                "Batch1105 (B05): Invalid partIndex/partCount or duplicate ObjectId across parts returns InvalidRequest",
                counters);
        }
        // (5) B06 검증: 조각당 객체 37개(> 1,200B) 또는 조각 수 256개(> 255) 초과 시 TooLarge 반환
        {
            net::SnapshotBatch over36ObjectsBatch{};
            Message part37 = makeSnapshotPart(1000, 10, 0, 1, {});
            auto& objs = std::get<StateSnapshotPayload>(part37.payload).objects;
            for (ObjectId id = 1; id <= 37; ++id)
            {
                objs.push_back(ObjectStateRecord{
                    .objectId = id,
                    .kind = NetObjectKind::PROJ_MONSTER,
                    .alive = 1
                    });
            }
            over36ObjectsBatch.parts.push_back(std::move(part37));
            net::SnapshotBatch over255PartsBatch{};
            over255PartsBatch.parts.resize(256, makeSnapshotPart(1000, 10, 0, 1, {}));
            Check(net::ValidateSnapshotBatch(over36ObjectsBatch) == net::SendResult::TooLarge
                && net::ValidateSnapshotBatch(over255PartsBatch) == net::SendResult::TooLarge,
                "Batch1105 (B06): Over 36 objects in a part or 256 parts in a batch returns TooLarge",
                counters);
        }
    }

    // 7. 11/06 UDP_REGISTER / UDP_ACK 코덱 고정 바이트·토큰 직렬화·오류 필드 검사 묶음
    void RunUdpRegistrationCodecSuite(TestCounters& counters)
    {
        using namespace protocol;
        const Message sentinelMsg = fixtures::MakeValidPlayerInputMessage();
        std::vector<std::uint8_t> wire;
        Message decoded = sentinelMsg;
        // (1) UDP_REGISTER (44B) 고정 바이트 인코딩·디코딩 및 64비트 토큰 Big-Endian 직렬화 대조
        {
            const Message validReg = fixtures::MakeValidUdpRegisterMessage();
            const std::vector<std::uint8_t> expectedReg(
                fixtures::VALID_UDP_REGISTER_BYTES.begin(),
                fixtures::VALID_UDP_REGISTER_BYTES.end());
            Check(CPacketCodec::Encode(validReg, wire) == CodecError::NONE
                && wire == expectedReg
                && wire[36] == 0x01 && wire[37] == 0x02
                && wire[38] == 0x03 && wire[39] == 0x04
                && wire[40] == 0x05 && wire[41] == 0x06
                && wire[42] == 0x07 && wire[43] == 0x08
                && CPacketCodec::Decode(fixtures::VALID_UDP_REGISTER_BYTES, decoded) == CodecError::NONE
                && decoded == validReg,
                "UdpReg1106: UDP_REGISTER matches fixed 44-byte fixture and big-endian u64 token",
                counters);
        }
        // (2) UDP_ACK (36B) 고정 바이트 인코딩·디코딩 대조
        {
            const Message validAck = fixtures::MakeValidUdpAckMessage();
            const std::vector<std::uint8_t> expectedAck(
                fixtures::VALID_UDP_ACK_BYTES.begin(),
                fixtures::VALID_UDP_ACK_BYTES.end());
            Check(CPacketCodec::Encode(validAck, wire) == CodecError::NONE
                && wire == expectedAck
                && CPacketCodec::Decode(fixtures::VALID_UDP_ACK_BYTES, decoded) == CodecError::NONE
                && decoded == validAck,
                "UdpReg1106: UDP_ACK matches fixed 36-byte fixture in Encode & Decode",
                counters);
        }
        // (3) 토큰 누락(udpToken == 0) 거부 vs 비영(non-zero) 토큰 통과 경계 확인
        //     (비영 토큰의 실제 발급값 일치 및 송신 주소 대조는 CSessionMgr 담당)
        {
            Message acceptZeroToken = fixtures::MakeValidAcceptMessage();
            std::get<AcceptPayload>(acceptZeroToken.payload).udpToken = 0;
            Message regZeroToken = fixtures::MakeValidUdpRegisterMessage();
            std::get<UdpRegisterPayload>(regZeroToken.payload).udpToken = 0;
            auto rawRegZeroToken = std::vector<std::uint8_t>(
                fixtures::VALID_UDP_REGISTER_BYTES.begin(),
                fixtures::VALID_UDP_REGISTER_BYTES.end());
            for (std::size_t i = 36; i < 44; ++i)
                rawRegZeroToken[i] = 0x00;
            Message out = sentinelMsg;
            Check(CPacketCodec::Encode(acceptZeroToken, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(regZeroToken, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Decode(rawRegZeroToken, out) == CodecError::INVALID_FIELD
                && out == sentinelMsg,
                "UdpReg1106: Missing udpToken (0) in ACCEPT or UDP_REGISTER returns INVALID_FIELD",
                counters);
            // 0이 아닌 다른 토큰 값은 코덱 구조 검사를 통과해 CSessionMgr로 전달되어야 함
            Message regDifferentToken = fixtures::MakeValidUdpRegisterMessage();
            std::get<UdpRegisterPayload>(regDifferentToken.payload).udpToken = 0xFEDCBA9876543210ULL;
            Message roundTrip{};
            Check(CPacketCodec::Encode(regDifferentToken, wire) == CodecError::NONE
                && CPacketCodec::Decode(wire, roundTrip) == CodecError::NONE
                && roundTrip == regDifferentToken,
                "UdpReg1106: Non-zero different udpToken passes codec (token matching deferred to CSessionMgr)",
                counters);
        }
        // (4) UDP_REGISTER / UDP_ACK 필드 범위 및 예약 필드 오류 검사
        {
            Message badRegPlayer = fixtures::MakeValidUdpRegisterMessage();
            std::get<UdpRegisterPayload>(badRegPlayer.payload).playerId = 2;
            Message badRegReserved0 = fixtures::MakeValidUdpRegisterMessage();
            std::get<UdpRegisterPayload>(badRegReserved0.payload).reserved0 = 1;
            Message badRegReserved1 = fixtures::MakeValidUdpRegisterMessage();
            std::get<UdpRegisterPayload>(badRegReserved1.payload).reserved1 = 1;
            Message badAckPlayer = fixtures::MakeValidUdpAckMessage();
            std::get<UdpAckPayload>(badAckPlayer.payload).playerId = INVALID_PLAYER_SLOT;
            Message badAckBool = fixtures::MakeValidUdpAckMessage();
            std::get<UdpAckPayload>(badAckBool.payload).registered = 2;
            Message badAckReserved = fixtures::MakeValidUdpAckMessage();
            std::get<UdpAckPayload>(badAckReserved.payload).reserved = 1;
            Check(CPacketCodec::Encode(badRegPlayer, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badRegReserved0, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badRegReserved1, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badAckPlayer, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badAckBool, wire) == CodecError::INVALID_FIELD
                && CPacketCodec::Encode(badAckReserved, wire) == CodecError::INVALID_FIELD,
                "UdpReg1106: Invalid playerId, registered bool, or non-zero reserved in UDP_REGISTER/UDP_ACK returns INVALID_FIELD",
                counters);
        }
    }

    // 8. 11/06 FakeSendSink 기반 상태 배치 교체 및 큐 동작 독립 시험 묶음 (B01~B12)
    void RunFakeSendSinkBatchSuite(TestCounters& counters)
    {
        using namespace protocol;
        // 지정한 틱과 조각 수(기본 2조각)를 가진 정상 SnapshotBatch 생성 도우미
        auto makeBatch = [](
            std::uint32_t tick,
            std::uint8_t partCount = 2,
            std::uint32_t sessionId = 1000) -> net::SnapshotBatch
            {
                net::SnapshotBatch batch{};
                for (std::uint8_t idx = 0; idx < partCount; ++idx)
                {
                    Message msg{};
                    msg.header.type = MessageType::STATE_SNAPSHOT;
                    msg.header.sessionId = sessionId;
                    msg.header.roundId = 1;
                    msg.header.sequence = tick * 10 + idx;
                    msg.header.hostTick = tick;
                    StateSnapshotPayload snap{};
                    snap.snapshotTick = tick;
                    snap.partIndex = idx;
                    snap.partCount = partCount;
                    ObjectStateRecord obj{};
                    obj.objectId = static_cast<ObjectId>(idx + 1);
                    obj.kind = NetObjectKind::PLAYER;
                    obj.subType = 1;
                    obj.alive = 1;
                    obj.pos = FixedVec2{ static_cast<std::int32_t>(tick * 100 + idx), 2000 };
                    obj.hp = 10;
                    obj.maxHp = 10;
                    snap.objects.push_back(obj);
                    msg.header.totalBytes = static_cast<std::uint32_t>(
                        HEADER_BYTES + SNAPSHOT_HEADER_BYTES
                        + snap.objects.size() * OBJECT_STATE_RECORD_BYTES);
                    msg.payload = std::move(snap);
                    batch.parts.push_back(std::move(msg));
                }
                return batch;
            };
        auto makeHeartbeat = [](std::uint32_t sequence, std::uint32_t sessionId = 1000) -> Message
            {
                Message msg{};
                msg.header.type = MessageType::HEARTBEAT;
                msg.header.totalBytes = static_cast<std::uint32_t>(HEADER_BYTES);
                msg.header.sessionId = sessionId;
                msg.header.roundId = 1;
                msg.header.sequence = sequence;
                msg.payload = HeartbeatPayload{};
                return msg;
            };
        const net::SnapshotBatch batchA = makeBatch(10);
        const net::SnapshotBatch batchB = makeBatch(11);
        const net::SnapshotBatch batchC = makeBatch(12);
        // B01: 배치 A(틱 10) 등록, 아직 인출 안 함 -> Queued, 대기 A
        {
            net::FakeSendSink sink;
            Check(sink.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued
                && sink.PeekPendingBatch() == batchA,
                "FakeSink1106 (B01): Initial batch A(tick 10) is Queued and stored as pending",
                counters);
        }
        // B02: A 대기 중 B(틱 11) 등록 -> ReplacedPending, B 전체만 대기
        {
            net::FakeSendSink sink;
            Check(sink.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued
                && sink.EnqueueSnapshotBatch(batchB) == net::SendResult::ReplacedPending
                && sink.PeekPendingBatch() == batchB,
                "FakeSink1106 (B02): Registering B(tick 11) while A is pending returns ReplacedPending and keeps only B",
                counters);
        }
        // B03: A 인출 후 B(틱 11), C(틱 12) 등록 -> 전송 중 A 불변, 다음 대기는 C 전체
        {
            net::FakeSendSink sink;
            const bool queuedA = (sink.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued);
            const auto inFlight = sink.TakePendingBatchForSend();
            const bool bAndC = (sink.EnqueueSnapshotBatch(batchB) == net::SendResult::Queued)
                && (sink.EnqueueSnapshotBatch(batchC) == net::SendResult::ReplacedPending);
            Check(queuedA
                && inFlight.has_value() && *inFlight == batchA
                && bAndC
                && sink.PeekInFlightBatch() == batchA
                && sink.PeekPendingBatch() == batchC,
                "FakeSink1106 (B03): In-flight batch A remains immutable while pending B is replaced by C",
                counters);
        }
        // B04~B06: 대기 배치 A가 있을 때 오류 배치(세션 불일치/조각 번호 오류/크기 초과) 등록 시 기존 대기 A와 최신 틱 보존
        {
            auto checkRejectPreservesPending = [&](const net::SnapshotBatch& badBatch, net::SendResult expected) -> bool
                {
                    net::FakeSendSink sink;
                    if (sink.EnqueueSnapshotBatch(batchA) != net::SendResult::Queued)
                        return false;
                    if (sink.EnqueueSnapshotBatch(badBatch) != expected)
                        return false;
                    if (sink.PeekPendingBatch() != batchA)
                        return false;
                    // 기존 최신 틱(10)이 오염되지 않았으므로 틱 11인 batchB가 정상 ReplacedPending 되어야 함
                    return sink.EnqueueSnapshotBatch(batchB) == net::SendResult::ReplacedPending
                        && sink.PeekPendingBatch() == batchB;
                };
            net::SnapshotBatch badSession = makeBatch(20);
            badSession.parts[1].header.sessionId = 2000;
            net::SnapshotBatch badPartIndex = makeBatch(20);
            std::get<StateSnapshotPayload>(badPartIndex.parts[1].payload).partIndex = 0;
            net::SnapshotBatch tooManyParts = makeBatch(20, 1);
            tooManyParts.parts.resize(net::MAX_SNAPSHOT_PARTS_PER_BATCH + 1, tooManyParts.parts[0]);
            Check(checkRejectPreservesPending(badSession, net::SendResult::InvalidRequest)
                && checkRejectPreservesPending(badPartIndex, net::SendResult::InvalidRequest)
                && checkRejectPreservesPending(tooManyParts, net::SendResult::TooLarge),
                "FakeSink1106 (B04~B06): Rejected batch preserves existing pending batch and last accepted tick",
                counters);
        }
        // B07: 이미 수락한 틱(11) 또는 과거 틱(10) 재등록 -> StaleBatch, 대기·전송 데이터 유지
        {
            net::FakeSendSink sink;
            const bool step1 = (sink.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued);
            const auto inFlight = sink.TakePendingBatchForSend();
            const bool step2 = (sink.EnqueueSnapshotBatch(batchB) == net::SendResult::Queued);
            Check(step1 && inFlight.has_value() && step2
                && sink.EnqueueSnapshotBatch(batchB) == net::SendResult::StaleBatch
                && sink.EnqueueSnapshotBatch(batchA) == net::SendResult::StaleBatch
                && sink.PeekPendingBatch() == batchB
                && sink.PeekInFlightBatch() == batchA,
                "FakeSink1106 (B07): Same or older tick returns StaleBatch and preserves pending/in-flight batches",
                counters);
        }
        // B08: TCP 이벤트 E1·E2 사이 상태 배치 여러 번 교체 -> E1·E2 순서·개수 유지
        {
            net::FakeSendSink sink;
            const Message e1 = makeHeartbeat(1);
            const Message e2 = makeHeartbeat(2);
            const bool ok = sink.EnqueueSend(net::Channel::Tcp, e1) == net::SendResult::Queued
                && sink.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued
                && sink.EnqueueSnapshotBatch(batchB) == net::SendResult::ReplacedPending
                && sink.EnqueueSend(net::Channel::Tcp, e2) == net::SendResult::Queued
                && sink.EnqueueSnapshotBatch(batchC) == net::SendResult::ReplacedPending;
            const auto tcpQueue = sink.PeekSendQueue();
            Check(ok
                && tcpQueue.size() == 2
                && tcpQueue[0].channel == net::Channel::Tcp && tcpQueue[0].message == e1
                && tcpQueue[1].channel == net::Channel::Tcp && tcpQueue[1].message == e2
                && sink.PeekPendingBatch() == batchC,
                "FakeSink1106 (B08): TCP events E1 and E2 preserve FIFO order and count across batch replacements",
                counters);
        }
        // B09: 기존 배치를 빼고도 큐가 1,024건 초과 -> QueueFull, 기존 배치 유지 (동일 크기 교체는 허용)
        {
            net::FakeSendSink sink;
            bool ok = (sink.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued); // 2조각
            const std::size_t fillCount = net::MAX_SEND_QUEUE_ENTRIES - batchA.parts.size(); // 1022건
            for (std::size_t i = 0; i < fillCount && ok; ++i)
            {
                ok = (sink.EnqueueSend(net::Channel::Tcp, makeHeartbeat(static_cast<std::uint32_t>(i + 1)))
                    == net::SendResult::Queued);
            }
            // 현재 1022 + 2 = 1024건 가득 찬 상태:
            // - 단건 추가(1025건) -> QueueFull
            // - 3조각 배치로 교체 시도(1022 + 3 = 1025건) -> QueueFull 및 기존 batchA 유지
            // - 2조각 배치 B로 교체 시도(1022 + 2 = 1024건) -> ReplacedPending 성공
            const net::SnapshotBatch batch3Parts = makeBatch(20, 3);
            Check(ok
                && sink.EnqueueSend(net::Channel::Tcp, makeHeartbeat(9999)) == net::SendResult::QueueFull
                && sink.EnqueueSnapshotBatch(batch3Parts) == net::SendResult::QueueFull
                && sink.PeekPendingBatch() == batchA
                && sink.EnqueueSnapshotBatch(batchB) == net::SendResult::ReplacedPending
                && sink.PeekPendingBatch() == batchB,
                "FakeSink1106 (B09): Send queue limit (1024) enforced and preserves existing batch on QueueFull",
                counters);
        }
        // B10: 등록 성공 후 호출자가 원본 위치 값 변경 -> 내부에 수락한 위치 값은 변경되지 않음
        {
            net::FakeSendSink sink;
            net::SnapshotBatch callerBatch = batchA;
            const bool queued = (sink.EnqueueSnapshotBatch(callerBatch) == net::SendResult::Queued);
            // 호출자가 등록 직후 원본 객체 좌표를 임의로 수정
            std::get<StateSnapshotPayload>(callerBatch.parts[0].payload).objects[0].pos.x = 999999;
            Check(queued && sink.PeekPendingBatch() == batchA,
                "FakeSink1106 (B10): Modifying caller batch after EnqueueSnapshotBatch does not mutate queued copy",
                counters);
        }
        // B11 & B12: Stop 후 등록 거부(NotRunning) 및 새 세션 시작 후 이전 배치·틱 영향 없음
        {
            net::FakeSendSink sink;
            const bool queuedC = (sink.EnqueueSnapshotBatch(batchC) == net::SendResult::Queued); // 틱 12 등록
            sink.Stop();
            sink.Stop(); // 반복 Stop 호출 안전성 확인
            const Message validInput = fixtures::MakeValidPlayerInputMessage();
            const bool rejectedAfterStop =
                sink.EnqueueSnapshotBatch(batchB) == net::SendResult::NotRunning
                && sink.EnqueueSend(net::Channel::Udp, validInput) == net::SendResult::NotRunning
                && !sink.PeekPendingBatch().has_value()
                && sink.PeekSendQueue().empty();
            // 새 세션 시작 후 이전 틱(12)보다 작은 batchA(틱 10)를 등록해도 StaleBatch 없이 Queued 성공해야 함
            sink.StartNewSession();
            const bool acceptedInNewSession =
                sink.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued
                && sink.PeekPendingBatch() == batchA;
            Check(queuedC && rejectedAfterStop && acceptedInNewSession,
                "FakeSink1106 (B11~B12): Stop rejects new sends with NotRunning, and StartNewSession clears prior ticks",
                counters);
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
    RunValidationAndBoundarySuite(counters);
    RunSessionMessageSuite(counters);
    RunSnapshotBatchContractSuite(counters);
    RunUdpRegistrationCodecSuite(counters);
    RunFakeSendSinkBatchSuite(counters);

    std::cout << "----------------------------------------------\n";
    std::cout << "Passed:  " << counters.passed << '\n';
    std::cout << "Failed:  " << counters.failed << '\n';
    std::cout << "Pending: " << counters.pending << '\n';
    return (counters.failed == 0 && counters.pending == 0) ? 0 : 1;
}
