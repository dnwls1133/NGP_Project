#pragma once

#include "TestTransport.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace message_test
{
    enum class PeerRole
    {
        Host,
        Client
    };

    struct CaseDefinition
    {
        protocol::MessageType type;
        const char* name;
        PeerRole sender;
        std::uint32_t totalBytes;
    };

    // 배열 순서대로 시험한다.
    extern const std::array<CaseDefinition, 19> CASES;

    inline constexpr std::uint32_t MAX_REPEATS = 1000;

    // 실제 접속 승인·UDP 등록에서 공유한 값을 전달한다.
    struct TestContext
    {
        std::uint32_t sessionId = 0;
        protocol::UdpToken udpToken = 0;
    };

    struct TestStats
    {
        std::size_t queued = 0;       // 송신 대기열 등록 횟수
        std::size_t received = 0;     // 검사 함수에 전달된 전체 수신 수
        std::size_t unique = 0;       // 유효한 반복 번호의 최초 수신 수
        std::size_t matched = 0;      // 기대값과 일치
        std::size_t mismatched = 0;   // 필드 값 불일치
        std::size_t duplicates = 0;   // 같은 반복 번호 중복
        std::size_t unexpected = 0;   // 타입·경로·순번 오류
    };

    // 현재 시험 중인 메시지 타입 하나의 상태
    struct CaseState
    {
        protocol::MessageType type = protocol::MessageType::JOIN;
        std::uint32_t repeats = 0;
        TestContext context{};
        TestStats stats{};

        std::vector<bool> seen;
        std::chrono::steady_clock::time_point deadline{};
    };

    const CaseDefinition* FindCase(protocol::MessageType type);

    // index는 0부터 시작한다.
    bool BuildTestMessage(protocol::MessageType type, std::uint32_t index, const TestContext& context, protocol::Message& out);

    bool BeginCase(CaseState& state, protocol::MessageType type, std::uint32_t repeats, const TestContext& context, int timeoutMs = 5000);

    bool IsSender(const CaseState& state, PeerRole role);

    // 한 번 호출할 때 시험 메시지 한 개 등록
    transport::MessageResult QueueNextTestMessage(CaseState& state, PeerRole role, transport::TcpState& tcp, transport::UdpState& udp, const sockaddr_in& peerUdpAddress);

    // UDP 주소 검사는 호출자가 먼저 수행한다.
    bool RecordReceived(CaseState& state, const protocol::Message& message, bool arrivedViaUdp);

    bool ReceiveComplete(const CaseState& state);
    bool TimedOut(const CaseState& state);
    bool Passed(const CaseState& state);

    void PrintResult(const CaseState& state, PeerRole role);
}
