#pragma once

#include "MessageTest.h"

namespace test_control
{
    enum class Command : std::uint32_t
    {
        Prepare = 1,     // 이번 타입과 N 전달
        Prepared = 2,    // 상대가 시험 준비 완료
        Go = 3,          // 전송 시작
        DataSent = 4,    // 이번 타입의 실제 송신 대기열 처리 완료
        Result = 5,      // 수신 측 비교 결과
        Advance = 6,     // 이번 타입 종료, 다음으로 이동 요청
        Advanced = 7,    // 이동 확인
        Finish = 8,      // 전체 시험 종료
        Finished = 9     // 전체 종료 확인
    };

    struct Control
    {
        Command command{};
        std::uint32_t caseIndex = 0;
        std::uint32_t repeats = 0;
        bool passed = false;
    };

    inline protocol::Message Make(
        const message_test::TestContext& context,
        Command command,
        std::uint32_t caseIndex,
        std::uint32_t repeats,
        bool passed = true)
    {
        protocol::Message message{};
        message.header.type = protocol::MessageType::HEARTBEAT;
        message.header.sessionId = context.sessionId;
        message.header.totalBytes = 32;

        message.header.sequence =
            0x80000000u |
            (static_cast<std::uint32_t>(command) << 24) |
            (caseIndex << 16) |
            (repeats << 1) |
            (passed ? 1u : 0u);

        message.payload = protocol::HeartbeatPayload{};
        return message;
    }

    inline bool Read(
        const protocol::Message& message,
        Control& out)
    {
        if (message.header.type != protocol::MessageType::HEARTBEAT ||
            !std::holds_alternative<protocol::HeartbeatPayload>(
                message.payload))
        {
            return false;
        }

        const auto sequence = message.header.sequence;

        constexpr std::uint32_t ALLOWED_BITS = 0x8F1F07FFu;

        if ((sequence & 0x80000000u) == 0 ||
            (sequence & ~ALLOWED_BITS) != 0)
        {
            return false;
        }

        const auto command = (sequence >> 24) & 0x0Fu;
        const auto caseIndex = (sequence >> 16) & 0x1Fu;
        const auto repeats = (sequence >> 1) & 0x3FFu;

        if (command < 1 || command > 9 ||
            caseIndex > message_test::CASES.size() ||
            repeats == 0 ||
            repeats > message_test::MAX_REPEATS)
        {
            return false;
        }

        out = {
            static_cast<Command>(command),
            caseIndex,
            repeats,
            (sequence & 1u) != 0
        };

        return true;
    }
}
