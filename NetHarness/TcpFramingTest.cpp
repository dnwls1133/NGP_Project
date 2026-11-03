#include "TcpFramingTest.h"
#include "TestTransport.h"

#include <initializer_list>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
    using Bytes = std::vector<std::uint8_t>;
    using protocol::CodecError;
    using protocol::Message;
    using protocol::MessageType;

    struct Fixture
    {
        Message message;
        Bytes bytes;
    };

    struct Step
    {
        Bytes input;
        std::vector<Message> expectedMessages;
        Bytes expectedPartial;
        CodecError expectedError = CodecError::NONE;
    };

    struct Counters
    {
        int passed = 0;
        int failed = 0;
    };

    Bytes Slice(const Bytes& bytes, std::size_t offset, std::size_t count)
    {
        return Bytes(bytes.begin() + offset, bytes.begin() + offset + count);
    }

    Bytes Concat(std::initializer_list<Bytes> parts)
    {
        Bytes result;

        for (const auto& part : parts)
            result.insert(result.end(), part.begin(), part.end());

        return result;
    }

    bool EncodeFixture(Fixture& fixture, std::uint32_t expectedBytes)
    {
        fixture.message.header.totalBytes = expectedBytes;

        const auto error = protocol::CPacketCodec::Encode(fixture.message, fixture.bytes);

        if (error != CodecError::NONE)
        {
            std::cerr << "[SETUP FAIL] Encode error=" << static_cast<int>(error) << "\n";
            return false;
        }

        if (fixture.bytes.size() != expectedBytes)
        {
            std::cerr
                << "[SETUP FAIL] Expected bytes=" << expectedBytes
                << ", actual=" << fixture.bytes.size() << "\n";
            return false;
        }

        return true;
    }

    bool PrepareFixtures(Fixture& a, Fixture& b, Fixture& c)
    {
        // A: JOIN 32바이트
        a.message.header.type = MessageType::JOIN;
        a.message.header.sessionId = 0;
        a.message.header.sequence = 11;
        a.message.payload = protocol::JoinPayload{};

        // B: READY 36바이트
        b.message.header.type = MessageType::READY;
        b.message.header.sessionId = 1000;
        b.message.header.sequence = 22;

        protocol::ReadyPayload ready{};
        ready.playerId = 1;
        ready.isReady = 1;
        b.message.payload = ready;

        // C: ACCEPT 44바이트
        c.message.header.type = MessageType::ACCEPT;
        c.message.header.sessionId = 1000;
        c.message.header.sequence = 33;

        protocol::AcceptPayload accepted{};
        accepted.assignedPlayerId = 1;
        accepted.udpToken = 0x0102030405060708ULL;
        c.message.payload = accepted;

        return EncodeFixture(a, 32) && EncodeFixture(b, 36) && EncodeFixture(c, 44);
    }

    Bytes ChangeTotalBytes(Bytes bytes, std::uint32_t totalBytes)
    {
        // 공통 헤더 오프셋 8의 u32 필드만 시험 목적으로 변경한다.
        bytes[8] = static_cast<std::uint8_t>(totalBytes >> 24);
        bytes[9] = static_cast<std::uint8_t>(totalBytes >> 16);
        bytes[10] = static_cast<std::uint8_t>(totalBytes >> 8);
        bytes[11] = static_cast<std::uint8_t>(totalBytes);

        return bytes;
    }

    bool CheckStep(const transport::TcpState& state, const transport::MessageResult& result, const Step& step)
    {
        const auto expectedStatus = step.expectedError == CodecError::NONE
            ? transport::IoStatus::Success
            : transport::IoStatus::Error;

        const bool resultMatches =
            result.status == expectedStatus &&
            result.socketError == 0 &&
            result.codecError == step.expectedError;

        const bool messagesMatch =
            state.received.size() == step.expectedMessages.size() &&
            std::equal(state.received.begin(), state.received.end(), step.expectedMessages.begin());

        const bool partialMatches = state.partialFrame == step.expectedPartial;

        if (resultMatches && messagesMatch && partialMatches)
            return true;

        std::cerr
            << "  Expected status=" << static_cast<int>(expectedStatus) << ", actual=" << static_cast<int>(result.status) << "\n"
            << "  Expected codecError=" << static_cast<int>(step.expectedError) << ", actual=" << static_cast<int>(result.codecError) << ", socketError=" << result.socketError << "\n"
            << "  Expected messages=" << step.expectedMessages.size() << ", actual=" << state.received.size() << "\n"
            << "  Message fields/order match=" << (messagesMatch ? "yes" : "no") << "\n"
            << "  Expected partial bytes=" << step.expectedPartial.size() << ", actual=" << state.partialFrame.size() << "\n"
            << "  Partial contents match=" << (partialMatches ? "yes" : "no") << "\n";

        return false;
    }

    void RunCase(const char* name, const std::vector<Step>& steps, Counters& counters)
    {
        transport::TcpState state;

        for (std::size_t i = 0; i < steps.size(); ++i)
        {
            const auto& step = steps[i];
            const auto result = transport::FeedTcpBytes(state, std::span<const std::uint8_t>(step.input));

            if (!CheckStep(state, result, step))
            {
                std::cerr << "[FAIL] " << name << ", step=" << i + 1 << "\n";
                ++counters.failed;
                return;
            }
        }

        std::cout << "[PASS] " << name << "\n";
        ++counters.passed;
    }
}

bool RunTcpFramingTests()
{
    Fixture a;
    Fixture b;
    Fixture c;

    std::cout
        << "=== TCP framing unit tests ===\n"
        << "Mode: direct byte input, no socket communication\n\n";

    if (!PrepareFixtures(a, b, c))
        return false;

    Counters counters;

    // F01: 정상 메시지 한 개
    RunCase("F01 Single frame", { { a.bytes, { a.message }, {} } }, counters);

    // F02: 헤더 중간에서 분할
    RunCase("F02 Split header", {
        { Slice(c.bytes, 0, 10), {}, Slice(c.bytes, 0, 10) },
        { Slice(c.bytes, 10, 34), { c.message }, {} }
        }, counters);

    // F03: 헤더와 본문 경계에서 분할
    RunCase("F03 Header boundary", {
        { Slice(c.bytes, 0, 32), {}, Slice(c.bytes, 0, 32) },
        { Slice(c.bytes, 32, 12), { c.message }, {} }
        }, counters);

    // F04: 본문 중간에서 분할
    RunCase("F04 Split body", {
        { Slice(c.bytes, 0, 37), {}, Slice(c.bytes, 0, 37) },
        { Slice(c.bytes, 37, 7), { c.message }, {} }
        }, counters);

    // F05: 한 바이트씩 전달
    std::vector<Step> byteSteps;

    for (std::size_t i = 0; i < c.bytes.size(); ++i)
    {
        Step step;
        step.input = Slice(c.bytes, i, 1);

        if (i + 1 == c.bytes.size())
            step.expectedMessages.push_back(c.message);
        else
            step.expectedPartial = Slice(c.bytes, 0, i + 1);

        byteSteps.push_back(std::move(step));
    }

    RunCase("F05 One byte at a time", byteSteps, counters);

    // F06: 본문 없는 메시지의 마지막 헤더 바이트
    RunCase("F06 Header-only frame", {
        { Slice(a.bytes, 0, 31), {}, Slice(a.bytes, 0, 31) },
        { Slice(a.bytes, 31, 1), { a.message }, {} }
        }, counters);

    // F07: 여러 메시지가 한 번에 도착
    RunCase("F07 Multiple frames", {
        { Concat({ a.bytes, b.bytes, c.bytes }),
          { a.message, b.message, c.message }, {} }
        }, counters);

    // F08: 완성 메시지와 다음 메시지 일부가 함께 도착
    RunCase("F08 Complete plus partial", {
        { Concat({ a.bytes, Slice(c.bytes, 0, 9) }),
          { a.message }, Slice(c.bytes, 0, 9) },

        { Slice(c.bytes, 9, 35),
          { a.message, c.message }, {} }
        }, counters);

    // F09: 이전 메시지의 끝과 다음 메시지가 함께 도착
    RunCase("F09 Tail plus next frame", {
        { Slice(c.bytes, 0, 40), {}, Slice(c.bytes, 0, 40) },

        { Concat({ Slice(c.bytes, 40, 4), b.bytes }),
          { c.message, b.message }, {} }
        }, counters);

    // F10: 빈 입력이 기존 누적 상태를 변경하지 않음
    RunCase("F10 Empty input", {
        { {}, {}, {} },
        { Slice(c.bytes, 0, 10), {}, Slice(c.bytes, 0, 10) },
        { {}, {}, Slice(c.bytes, 0, 10) },
        { Slice(c.bytes, 10, 34), { c.message }, {} }
        }, counters);

    // F11: 전체 길이가 헤더 크기보다 작음
    const Bytes shortLength = ChangeTotalBytes(a.bytes, 31);

    RunCase("F11 Reject short length", {
        { shortLength, {}, shortLength, CodecError::INVALID_LENGTH }
        }, counters);

    // F12: 최대 TCP 프레임 크기 초과
    const Bytes oversized = ChangeTotalBytes(a.bytes, 65537);

    RunCase("F12 Reject oversized length", {
        { oversized, {}, oversized, CodecError::TOO_LARGE }
        }, counters);

    std::cout
        << "\nSummary: total=" << counters.passed + counters.failed
        << " passed=" << counters.passed
        << " failed=" << counters.failed << "\n";

    return counters.failed == 0;
}
