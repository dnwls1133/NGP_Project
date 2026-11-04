#include "TcpFramingTest.h"
#include "TestTransport.h"
#include "TcpTestSupport.h"

#include <initializer_list>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
#include <chrono>
#include <iterator>
#include <string>

namespace
{
    using Bytes = std::vector<std::uint8_t>;
    using protocol::CodecError;
    constexpr int NETWORK_TIMEOUT_MS = 10000;
    constexpr int ACCEPT_TIMEOUT_MS = 60000;

    struct Fixture
    {
        tcp_test::Message message;
        Bytes bytes;
    };

    struct Step
    {
        Bytes input;
        std::vector<tcp_test::Message> expectedMessages;
        Bytes expectedPartial;
        CodecError expectedError = CodecError::NONE;
    };

    struct Counters
    {
        int passed = 0;
        int failed = 0;
    };

    struct NetworkCase
    {
        const char* name;
        std::vector<Step> steps;
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
        a.message.header.type = tcp_test::MessageType::JOIN;
        a.message.header.sessionId = 0;
        a.message.header.sequence = 11;
        a.message.payload = protocol::JoinPayload{};

        // B: READY 36바이트
        b.message.header.type = tcp_test::MessageType::READY;
        b.message.header.sessionId = 1000;
        b.message.header.sequence = 22;

        protocol::ReadyPayload ready{};
        ready.playerId = 1;
        ready.isReady = 1;
        b.message.payload = ready;

        // C: ACCEPT 44바이트
        c.message.header.type = tcp_test::MessageType::ACCEPT;
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

    bool SendStepBytes(SOCKET sock, const Bytes& bytes)
    {
        // TransferExact의 공용 인터페이스에 맞춘 송신용 복사본.
        Bytes copy = bytes;

        return tcp_test::TransferExact(sock, reinterpret_cast<char*>(copy.data()), copy.size(), true);
    }

    std::size_t CountBufferedBytes(const transport::TcpState& state)
    {
        std::size_t size = state.partialFrame.size();

        for (const auto& message : state.received)
            size += message.header.totalBytes;

        return size;
    }

    bool ReceiveStepBytes(SOCKET sock, transport::TcpState& state, std::size_t inputSize, transport::MessageResult& result)
    {
        result = {};

        const std::size_t target = CountBufferedBytes(state) + inputSize;
        const auto deadline = tcp_test::Clock::now() + std::chrono::milliseconds(NETWORK_TIMEOUT_MS);

        while (CountBufferedBytes(state) < target)
        {
            if (!tcp_test::WaitReady(sock, false, deadline))
                return false;

            result = transport::ReceiveTcpMessages(sock, state);

            if (result.status == transport::IoStatus::WouldBlock)
                continue;

            // 코덱 오류도 검증 필요
            // 결과 보존 후 CheckStep에서 예상한 오류인지 비교
            if (result.status == transport::IoStatus::Error && result.socketError == 0 && result.codecError != CodecError::NONE)
                return true;

            // 소켓 오류 or 예상하지 않은 연결 종료 -> 수신 실패
            if (result.status != transport::IoStatus::Success)
            {
                std::cerr
                    << "[NETWORK] Receive failed"
                    << ", status=" << static_cast<int>(result.status)
                    << ", socketError=" << result.socketError
                    << ", codecError=" << static_cast<int>(result.codecError)
                    << "\n";

                return false;
            }
        }

        if (CountBufferedBytes(state) != target)
        {
            std::cerr << "[FAIL] 예상한 수신 바이트 수와 다름\n";
            return false;
        }

        return true;
    }

    std::vector<NetworkCase> BuildNetworkCases(const Fixture& a, const Fixture& b, const Fixture& c)
    {
        std::vector<NetworkCase> cases;

        cases.push_back({
            "N01 Single frame",
            {
                { a.bytes, { a.message }, {} }
            }
            });

        cases.push_back({
            "N02 Split header",
            {
                { Slice(c.bytes, 0, 10), {}, Slice(c.bytes, 0, 10) },
                { Slice(c.bytes, 10, 34), { c.message }, {} }
            }
            });

        cases.push_back({
            "N03 Header boundary",
            {
                { Slice(c.bytes, 0, 32), {}, Slice(c.bytes, 0, 32) },
                { Slice(c.bytes, 32, 12), { c.message }, {} }
            }
            });

        cases.push_back({
            "N04 Split body",
            {
                { Slice(c.bytes, 0, 37), {}, Slice(c.bytes, 0, 37) },
                { Slice(c.bytes, 37, 7), { c.message }, {} }
            }
            });

        NetworkCase byteCase{ "N05 One byte at a time", {} };

        for (std::size_t i = 0; i < c.bytes.size(); ++i)
        {
            Step step;
            step.input = Slice(c.bytes, i, 1);

            if (i + 1 == c.bytes.size())
                step.expectedMessages.push_back(c.message);
            else
                step.expectedPartial = Slice(c.bytes, 0, i + 1);

            byteCase.steps.push_back(std::move(step));
        }

        cases.push_back(std::move(byteCase));

        cases.push_back({
            "N06 Multiple frames",
            {
                { Concat({ a.bytes, b.bytes, c.bytes }), { a.message, b.message, c.message }, {} }
            }
            });

        cases.push_back({
            "N07 Complete plus partial",
            {
                { Concat({ a.bytes, Slice(c.bytes, 0, 9) }), { a.message }, Slice(c.bytes, 0, 9) },
                { Slice(c.bytes, 9, 35), { a.message, c.message }, {} }
            }
            });

        cases.push_back({
            "N08 Tail plus next frame",
            {
                { Slice(c.bytes, 0, 40), {}, Slice(c.bytes, 0, 40) },
                { Concat({ Slice(c.bytes, 40, 4), b.bytes }), { c.message, b.message }, {} }
            }
            });

        cases.push_back({
            "N09 Header-only frame",
            {
                { Slice(a.bytes, 0, 31), {}, Slice(a.bytes, 0, 31) },
                { Slice(a.bytes, 31, 1), { a.message }, {} }
            }
            });

        const Bytes shortLength = ChangeTotalBytes(a.bytes, 31);
        cases.push_back({
            "N10 Reject short length",
            {
                { shortLength, {}, shortLength, CodecError::INVALID_LENGTH }
            }
            });

        const Bytes oversized = ChangeTotalBytes(a.bytes, 65537);
        cases.push_back({
            "N11 Reject oversized length",
            {
                { oversized, {}, oversized, CodecError::TOO_LARGE }
            }
            });

        return cases;
    }

    bool RunNetworkDirection(SOCKET sock, bool sending, const char* direction, const std::vector<NetworkCase>& cases)
    {
        std::cout
            << "\n=== " << direction << " ===\n"
            << "Local role: " << (sending ? "sender" : "receiver") << "\n";

        std::size_t passedCases = 0;

        for (const auto& test : cases)
        {
            // 매번 새로운 프레이밍 상태로 시작
            transport::TcpState state;

            std::cout << "[TEST] " << test.name << "\n";

            for (std::size_t i = 0; i < test.steps.size(); ++i)
            {
                const auto& step = test.steps[i];

                if (sending)
                {
                    if (!SendStepBytes(sock, step.input))
                        return false;

                    // 검증 완료 후 다음 조각을 보내기
                    if (!tcp_test::ReceiveStepResult(sock))
                    {
                        std::cerr << "[FAIL] " << test.name << ", step=" << i + 1 << "\n";
                        return false;
                    }
                }
                else
                {
                    transport::MessageResult result{};

                    const bool received = ReceiveStepBytes(sock, state, step.input.size(), result);
                    const bool matched = received && CheckStep(state, result, step);

                    if (!tcp_test::SendStepResult(sock, matched))
                        return false;

                    if (!matched)
                    {
                        std::cerr << "[FAIL] " << test.name << ", step=" << i + 1 << "\n";
                        return false;
                    }

                    std::cout
                        << "  step=" << i + 1
                        << ", messages=" << state.received.size()
                        << ", partial=" << state.partialFrame.size()
                        << " PASS\n";
                }
            }

            ++passedCases;

            std::cout
                << "[PASS] " << test.name
                << (sending ? " (receiver confirmed)" : " (locally verified)")
                << "\n";
        }

        std::cout
            << "[" << direction << "] passed=" << passedCases
            << " failed=0\n";

        return true;
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

    const auto cases = BuildNetworkCases(a, b, c);

    for (const auto& test : cases)
        RunCase(test.name, test.steps, counters);

    // 빈 입력의 상태 보존은 함수 단위로 확인한다.
    RunCase("U12 Empty input", {
        { {}, {}, {} },
        { Slice(c.bytes, 0, 10), {}, Slice(c.bytes, 0, 10) },
        { {}, {}, Slice(c.bytes, 0, 10) },
        { Slice(c.bytes, 10, 34), { c.message }, {} }
        }, counters);

    std::cout
        << "\nSummary: total=" << counters.passed + counters.failed
        << " passed=" << counters.passed
        << " failed=" << counters.failed << "\n";

    return counters.failed == 0;
}

bool RunTcpFramingNetworkTests(bool host, unsigned short port)
{
    Fixture a;
    Fixture b;
    Fixture c;

    if (!PrepareFixtures(a, b, c))
        return false;

    const auto cases = BuildNetworkCases(a, b, c);

    tcp_test::TestSocket connection;

    if (!tcp_test::OpenNetworkSocket(host, port, connection))
    {
        std::cerr << "[FAIL] TCP 연결 준비 실패\n";
        return false;
    }

    // 서로 프레이밍 시험 모드 확인
    char hello[] = { 'F', 'S', '0', '2' };
    char peerHello[4]{};

    if (!tcp_test::TransferExact(connection.value, hello, sizeof(hello), true))
        return false;

    if (!tcp_test::TransferExact(connection.value, peerHello, sizeof(peerHello), false))
        return false;

    if (!std::equal(std::begin(hello), std::end(hello), std::begin(peerHello)))
    {
        std::cerr << "[FAIL] 시험 모드 또는 버전 불일치\n";
        return false;
    }

    // 1단계: Host 송신, Client 검증
    if (!RunNetworkDirection(connection.value, host, "Host -> Client", cases))
        return false;

    // 2단계: Client 송신, Host 검증
    if (!RunNetworkDirection(connection.value, !host, "Client -> Host", cases))
        return false;

    std::cout << "\n[SUITE] PASS\n";
    return true;
}
