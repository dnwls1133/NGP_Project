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
    struct TestCounters
    {
        int passed = 0;
        int failed = 0;
        int pending = 0;
    };

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
    // 2. 11/02~11/04 코덱 고정 바이트 및 오류 분류 시험 묶음
    void RunCodecVectorSuite(TestCounters& counters)
    {
        using namespace protocol;
        std::vector<std::uint8_t> encoded;
        const Message validJoin = fixtures::MakeValidJoinMessage();
        const CodecError probe = CPacketCodec::Encode(validJoin, encoded);
        if (probe == CodecError::UNSUPPORTED_MESSAGE)
        {
            ++counters.pending;
            std::cout << "[PENDING] CPacketCodec::Encode/Decode is stubbed "
                "(scheduled for 11/02~11/04).\n";
            return;
        }
        // 정상 JOIN 인코딩
        const std::vector<std::uint8_t> expectedJoin(
            fixtures::VALID_JOIN_BYTES.begin(),
            fixtures::VALID_JOIN_BYTES.end());
        Check(probe == CodecError::NONE && encoded == expectedJoin,
            "Codec: Encode JOIN matches fixed 32-byte fixture", counters);
        // 정상 JOIN 디코딩
        Message decoded{};
        const CodecError decJoinErr = CPacketCodec::Decode(
            fixtures::VALID_JOIN_BYTES, decoded);
        Check(decJoinErr == CodecError::NONE && decoded == validJoin,
            "Codec: Decode fixed 32-byte JOIN matches expected fields", counters);
        // 정상 PLAYER_INPUT 인코딩 (sequence 0x01020304 big-endian 확인)
        const Message validInput = fixtures::MakeValidPlayerInputMessage();
        const CodecError encInputErr = CPacketCodec::Encode(validInput, encoded);
        const std::vector<std::uint8_t> expectedInput(
            fixtures::VALID_PLAYER_INPUT_BYTES.begin(),
            fixtures::VALID_PLAYER_INPUT_BYTES.end());
        Check(encInputErr == CodecError::NONE && encoded == expectedInput,
            "Codec: Encode PLAYER_INPUT matches fixed 40-byte fixture", counters);
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
            "Codec: Length 0..31 returns TRUNCATED and preserves out", counters);
        // magic 오류
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[0] = 0x00;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::BAD_MAGIC
                && out == validInput,
                "Codec: Byte 0 mismatch returns BAD_MAGIC", counters);
        }
        // 버전 오류 (version = 2)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[3] = 0x02;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::UNSUPPORTED_VERSION
                && out == validInput,
                "Codec: version=2 returns UNSUPPORTED_VERSION", counters);
        }
        // 종류 오류 (type = 65535)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[4] = 0xFF;
            bytes[5] = 0xFF;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::UNKNOWN_TYPE
                && out == validInput,
                "Codec: type=65535 returns UNKNOWN_TYPE", counters);
        }
        // 미정 플래그 비트 (flags = 1)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[7] = 0x01;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_FLAGS
                && out == validInput,
                "Codec: flags=1 returns INVALID_FLAGS", counters);
        }
        // 예약 필드 오류 (reserved = 1)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[31] = 0x01;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_RESERVED
                && out == validInput,
                "Codec: reserved=1 returns INVALID_RESERVED", counters);
        }
        // 전체 길이 오류 (totalBytes = 31)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[11] = 0x1F;
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_LENGTH
                && out == validInput,
                "Codec: totalBytes=31 returns INVALID_LENGTH", counters);
        }
        // 뜻밖의 본문 (빈 본문 JOIN에 1바이트 추가 + totalBytes도 33으로 변경)
        {
            auto bytes = fixtures::CopyValidJoinBytes();
            bytes[11] = 0x21; // 33
            bytes.push_back(0x00);
            Message out = validInput;
            Check(CPacketCodec::Decode(bytes, out) == CodecError::INVALID_LENGTH
                && out == validInput,
                "Codec: 33-byte JOIN with unexpected body returns INVALID_LENGTH", counters);
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
                "Codec: 1201-byte UDP message returns TOO_LARGE", counters);
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
                "Codec: 65537-byte TCP message returns TOO_LARGE", counters);
        }
    }
}


int main()
{
    TestCounters counters{};

    std::cout << "=== Virus Striker Codec Tests (Protocol v1) ===\n";
    RunContractSanityChecks(counters);
    RunCodecVectorSuite(counters);

    std::cout << "----------------------------------------------\n";
    std::cout << "Passed:  " << counters.passed << '\n';
    std::cout << "Failed:  " << counters.failed << '\n';
    std::cout << "Pending: " << counters.pending << '\n';
    return (counters.failed == 0 && counters.pending == 0) ? 0 : 1;
}
