#include "TcpSendTest.h"
#include "TcpTestSupport.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

namespace tcp_send_test
{
    bool Check(bool condition, const char* description)
    {
        if (!condition)
            std::cerr << "[FAIL] " << description << "\n";

        return condition;
    }

    bool PrepareMessages(transport::TcpState& state, std::vector<protocol::Message>& expected)
    {
        protocol::Message accepted{};
        accepted.header.type = protocol::MessageType::ACCEPT;
        accepted.header.totalBytes = 44;
        accepted.header.sessionId = 1000;
        accepted.header.sequence = 1;

        protocol::AcceptPayload acceptPayload{};
        acceptPayload.assignedPlayerId = 1;
        acceptPayload.udpToken = 0x0102030405060708ULL;
        accepted.payload = acceptPayload;

        protocol::Message ready{};
        ready.header.type = protocol::MessageType::READY;
        ready.header.totalBytes = 36;
        ready.header.sessionId = 1000;
        ready.header.sequence = 2;

        protocol::ReadyPayload readyPayload{};
        readyPayload.playerId = 1;
        readyPayload.isReady = 1;
        ready.payload = readyPayload;

        expected = { accepted, ready };

        for (const auto& message : expected)
        {
            const auto result = transport::QueueTcpMessage(state, message);

            if (!Check(result.status == transport::IoStatus::Success, "시험 메시지 큐에 등록"))
                return false;
        }

        return true;
    }

    bool RunSender(SOCKET sock, transport::TcpState& state)
    {
        transport::TcpState empty;
        bool called = false;

        const auto emptyResult = transport::FlushTcpSend(sock, empty,
            [&](SOCKET, const char*, int) -> transport::IoResult
            {
                called = true;
                return { transport::IoStatus::Error, 0, WSAEINVAL };
            });

        // 큐가 비었으면 보낼 필요 없으니 바로 리턴
        if (!Check(emptyResult.status == transport::IoStatus::Success && !called && empty.sendQueue.empty() && empty.sendOffset == 0, "빈 큐 처리"))
            return false;

        std::cout << "[PASS] 빈 큐에서 송신 호출 없음\n";

        int calls = 0;
        int partialCount = 0;
        int wouldBlockCount = 0;

        const auto deadline = tcp_test::Clock::now() + std::chrono::milliseconds(tcp_test::NETWORK_TIMEOUT_MS);

        while (!state.sendQueue.empty())
        {
            if (!tcp_test::WaitReady(sock, true, deadline))
                return false;

            // 호출 전 상태 저장
            auto expectedQueue = state.sendQueue;
            const std::size_t beforeOffset = state.sendOffset;
            const std::size_t frameSize = expectedQueue.front().size();

            bool requestMatches = false;
            transport::IoResult actual{ transport::IoStatus::Error, 0, WSAEINVAL };

            const auto result = transport::FlushTcpSend(sock, state,
                [&](SOCKET target, const char* data, int size) -> transport::IoResult
                {
                    ++calls;

                    const char* expectedData = reinterpret_cast<const char*>(state.sendQueue.front().data() + beforeOffset);

                    requestMatches = target == sock && data == expectedData && size == static_cast<int>(frameSize - beforeOffset);

                    // 테스트를 위해 두 번째 호출에서만 송신 없이 WouldBlock을 반환
                    if (calls == 2)
                    {
                        actual = { transport::IoStatus::WouldBlock, 0, 0 };
                        return actual;
                    }

                    // 나머지 최대 10 바이트씩
                    actual = transport::SendTcp(target, data, (std::min)(size, 10));
                    return actual;
                });

            if (!Check(requestMatches, "남은 데이터 시작 위치와 요청 길이"))
                return false;

            if (actual.status == transport::IoStatus::WouldBlock)
            {
                if (!Check(result.status == transport::IoStatus::WouldBlock && state.sendQueue == expectedQueue && state.sendOffset == beforeOffset, "WouldBlock 후 큐와 오프셋 유지"))
                    return false;

                ++wouldBlockCount;

                std::cout
                    << "[PASS] WouldBlock: offset=" << state.sendOffset
                    << ", remaining=" << frameSize - state.sendOffset << "\n";

                continue;
            }

            if (!Check(actual.status == transport::IoStatus::Success && result.status == transport::IoStatus::Success && actual.bytes > 0, "실제 TCP 송신"))
                return false;

            std::size_t expectedOffset = beforeOffset + static_cast<std::size_t>(actual.bytes);

            if (expectedOffset == frameSize)
            {
                expectedQueue.pop_front();
                expectedOffset = 0;
            }
            else
            {
                ++partialCount;
            }

            if (!Check(state.sendQueue == expectedQueue && state.sendOffset == expectedOffset, "부분 송신 후 큐와 오프셋 갱신"))
                return false;

            std::cout << "[PASS] sent=" << actual.bytes << ", queue=" << state.sendQueue.size() << ", offset=" << state.sendOffset << "\n";
        }

        if (!Check(partialCount > 0 && wouldBlockCount > 0, "부분 송신과 WouldBlock 발생"))
            return false;

        // 송신 오류 시 데이터 보존을 검사
        // 실제 소켓에 데이터를 보내진 않음
        transport::TcpState errorState;
        errorState.sendQueue.push_back({ 1, 2, 3, 4 });
        errorState.sendOffset = 1;

        const auto beforeError = errorState.sendQueue;

        const auto errorResult = transport::FlushTcpSend(sock, errorState,
            [](SOCKET, const char*, int) -> transport::IoResult
            {
                return { transport::IoStatus::Error, 0, WSAECONNRESET };
            });

        if (!Check(errorResult.status == transport::IoStatus::Error && errorResult.socketError == WSAECONNRESET && errorState.sendQueue == beforeError && errorState.sendOffset == 1, "송신 오류 반환과 잔여 데이터 보존"))
            return false;

        std::cout << "[PASS] 송신 오류 시 큐와 오프셋 유지\n";

        return tcp_test::ReceiveStepResult(sock);
    }

    bool RunReceiver(SOCKET sock, const std::vector<protocol::Message>& expected)
    {
        transport::TcpState state;

        const auto deadline = tcp_test::Clock::now() + std::chrono::milliseconds(tcp_test::NETWORK_TIMEOUT_MS);

        while (state.received.size() < expected.size())
        {
            if (!tcp_test::WaitReady(sock, false, deadline))
                return false;

            const auto result = transport::ReceiveTcpMessages(sock, state);

            if (result.status == transport::IoStatus::WouldBlock)
                continue;

            if (result.status != transport::IoStatus::Success)
            {
                std::cerr << "[NETWORK] status=" << static_cast<int>(result.status) << ", socketError=" << result.socketError << ", codecError=" << static_cast<int>(result.codecError) << "\n";
                return false;
            }
        }

        const bool matched = state.received.size() == expected.size() && std::equal(state.received.begin(), state.received.end(), expected.begin()) && state.partialFrame.empty();

        if (!tcp_test::SendStepResult(sock, matched))
            return false;

        if (!Check(matched, "수신 메시지 개수·순서·필드·잔여 버퍼"))
            return false;

        std::cout << "[PASS] received=" << state.received.size() << ", partial=" << state.partialFrame.size() << ", 전체 필드 일치\n";

        return true;
    }
}

bool RunTcpSendTests(bool host, unsigned short port)
{
    tcp_test::TestSocket connection;

    if (!tcp_test::OpenNetworkSocket(host, port, connection))
        return false;

    // 다른 모드와 연결 방지
    char hello[] = { 'T', 'S', '0', '1' };
    char peerHello[4]{};

    if (!tcp_test::TransferExact(connection.value, hello, sizeof(hello), true))
        return false;

    if (!tcp_test::TransferExact(connection.value, peerHello, sizeof(peerHello), false))
        return false;

    if (!std::equal(hello, hello + sizeof(hello), peerHello))
    {
        std::cerr << "[FAIL] 부분 송신 시험 모드 불일치\n";
        return false;
    }

    for (int direction = 0; direction < 2; ++direction)
    {
        const bool sending = direction == 0 ? host : !host;
        const char* name = direction == 0 ? "Host -> Client" : "Client -> Host";

        transport::TcpState state;
        std::vector<protocol::Message> expected;

        if (!tcp_send_test::PrepareMessages(state, expected))
            return false;

        std::cout
            << "\n=== " << name << " ===\n"
            << "Local role: " << (sending ? "sender" : "receiver") << "\n";

        const bool passed = sending ? tcp_send_test::RunSender(connection.value, state) : tcp_send_test::RunReceiver(connection.value, expected);

        if (!passed)
            return false;

        std::cout << "[" << name << "] PASS\n";
    }

    std::cout << "\n[SUITE] PASS\n";
    return true;
}
