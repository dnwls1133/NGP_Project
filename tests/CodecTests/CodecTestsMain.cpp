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

    std::cout << "----------------------------------------------\n";
    std::cout << "Passed:  " << counters.passed << '\n';
    std::cout << "Failed:  " << counters.failed << '\n';
    std::cout << "Pending: " << counters.pending << '\n';
    return (counters.failed == 0 && counters.pending == 0) ? 0 : 1;
}
