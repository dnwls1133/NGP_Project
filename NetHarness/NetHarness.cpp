#include <winsock2.h>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <system_error>

#include "HostTest.h"
#include "ClientTest.h"
#include "MessageTest.h"
#include "ScriptedPeer.h"
#include "TcpFramingTest.h"
#include "TcpSendTest.h"
#include "NetworkMgrTest.h"
#include "PongTest.h"
#include "SessionDataTest.h"

#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr unsigned short TCP_PORT = 40000;
    constexpr unsigned short UDP_PORT = 40001;

    struct Options
    {
        bool host = false;
        std::uint32_t repeats = 10;
        bool rejectJoin = false;
        bool expectReject = false;

        bool scriptedPeer = false;
        scripted_peer::Behavior peerBehavior = scripted_peer::Behavior::Close;
    };

    void PrintUsage()
    {
        std::cout
            << "Usage:\n"
            << "  NetHarness.exe host [--count N]\n"
            << "  NetHarness.exe client\n"
            << "\n"
            << "Reject test:\n"
            << "  NetHarness.exe host --reject\n"
            << "  NetHarness.exe client --expect-reject\n"
            << "\n"
            << "N: 1.." << message_test::MAX_REPEATS << "\n";

        std::cout
            << "\nScripted peer:\n"
            << "  NetHarness.exe peer-host close\n"
            << "  NetHarness.exe peer-host silent\n"
            << "  NetHarness.exe peer-client close\n"
            << "  NetHarness.exe peer-client silent\n";

        std::cout
            << "\nTCP framing unit test (no sockets):\n"
            << "  NetHarness.exe framing-test\n";

        std::cout
            << "\nTCP framing network test:\n"
            << "  NetHarness.exe framing-host\n"
            << "  NetHarness.exe framing-client\n";

        std::cout
            << "\nTCP partial send test:\n"
            << "  NetHarness.exe send-host\n"
            << "  NetHarness.exe send-client\n";

        std::cout
            << "\nCNetworkMgr Test:\n"
            << "  NetHarness.exe mgr-host\n"
            << "  NetHarness.exe mgr-client <ip>\n"
            << "  NetHarness.exe mgr-stop-test\n"
            << "  NetHarness.exe mgr-overflow-test\n"
            << "  NetHarness.exe mgr-queue-host\n"
            << "  NetHarness.exe mgr-queue-client <ip>\n"
            << "  NetHarness.exe mgr-batch-test\n"
            << "  NetHarness.exe mgr-batch-host\n"
            << "  NetHarness.exe mgr-batch-client <ip>\n"
            << "  NetHarness.exe mgr-active-stop-test\n";


        std::cout
            << "\nCSessionMgr Test:\n"
            << "  NetHarness.exe session-host [--heartbeat|--heartbeat-silent]\n"
            << "  NetHarness.exe session-client <ip> [--heartbeat|--heartbeat-silent]\n"
            << "  NetHarness.exe session-bad-token-client <ip>\n"
            << "  NetHarness.exe session-room-test\n"
            << "  NetHarness.exe session-retry-client <ip>\n"
            << "  NetHarness.exe session-connect-failure-client <ip>\n"
            << "  NetHarness.exe flow-host\n"
            << "  NetHarness.exe flow-client <Host-IP>\n";

        std::cout
            << "\nSession data test:\n"
            << "  NetHarness.exe session-data-host [--spawn|--lifecycle]\n"
            << "  NetHarness.exe session-data-client <Host-IP> [--spawn|--lifecycle]\n"
            << "  NetHarness.exe session-data-compare <host-trace> <client-trace>\n"
            << "  NetHarness.exe snapshot-replay-test\n";

        std::cout
            << "\nConcurrent queue test:\n"
            << "  NetHarness.exe mgr-concurrent-host\n"
            << "  NetHarness.exe mgr-concurrent-client <Host-IP>\n";
    }

    bool ParseCount(std::string_view text, std::uint32_t& out)
    {
        std::uint32_t value = 0;

        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

        if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 || value > message_test::MAX_REPEATS)
        {
            return false;
        }

        out = value;
        return true;
    }

    bool ParseArguments(int argc, char* argv[], Options& options)
    {
        if (argc < 2)
            return false;

        const std::string_view mode = argv[1];

        if (mode == "host")
            options.host = true;

        else if (mode == "client")
            options.host = false;

        else if (mode == "peer-host" || mode == "peer-client")
        {
            if (argc != 3)
                return false;

            options.scriptedPeer = true;
            options.host = mode == "peer-host";

            const std::string_view behavior = argv[2];

            if (behavior == "close")
                options.peerBehavior = scripted_peer::Behavior::Close;
            else if (behavior == "silent")
                options.peerBehavior = scripted_peer::Behavior::Silent;
            else
                return false;

            return true;
        }
        else
            return false;

        bool countSpecified = false;

        for (int i = 2; i < argc; ++i)
        {
            const std::string_view argument = argv[i];

            if (argument == "--count")
            {
                if (!options.host || countSpecified || i + 1 >= argc)
                    return false;

                if (!ParseCount(argv[++i], options.repeats))
                    return false;

                countSpecified = true;
            }
            else if (argument == "--reject")
            {
                if (!options.host || options.rejectJoin)
                    return false;

                options.rejectJoin = true;
            }
            else if (argument == "--expect-reject")
            {
                if (options.host || options.expectReject)
                    return false;

                options.expectReject = true;
            }
            else
                return false;
        }

        // 거부 시험에서는 메시지 반복 시험을 실행하지 않는다.
        if (options.rejectJoin && countSpecified)
            return false;

        return true;
    }

    void PrintConfiguration(const Options& options)
    {
        if (options.scriptedPeer)
        {
            std::cout
                << "=== ScriptedPeer ===\n"
                << "Role: " << (options.host ? "host" : "client") << "\n"
                << "TCP port: " << TCP_PORT << "\n"
                << "Behavior: " << (options.peerBehavior == scripted_peer::Behavior::Close ? "close" : "silent") << "\n\n";

            return;
        }

        std::cout
            << "=== NetHarness ===\n"
            << "Mode: " << (options.host ? "host" : "client") << "\n"
            << "TCP port: " << TCP_PORT << "\n"
            << "UDP port: " << UDP_PORT << "\n";

        if (options.host)
        {
            if (options.rejectJoin)
                std::cout << "Test: JOIN rejection\n";
            else
                std::cout
                << "Test: session flow + 19 message types\n"
                << "Repeat count: " << options.repeats << "\n";
        }
        else if (options.expectReject)
        {
            std::cout << "Test: expect JOIN rejection\n";
        }
        else
        {
            std::cout
                << "Test: session flow + 19 message types\n"
                << "Repeat count: supplied by host\n";
        }

        std::cout << "\n";
    }
    int InvalidArguments()
    {
        std::cerr << "Invalid arguments.\n\n";
        PrintUsage();
        return 1;
    }

    int RunSessionCommand(bool host, int argc, char* argv[])
    {
        using namespace network_mgr_test;

        const int requiredArgc = host ? 2 : 3;
        auto mode = SessionHeartbeatTest::None;
        if (argc > requiredArgc)
        {
            const std::string_view option = argv[requiredArgc];
            if (option == "--heartbeat")
                mode = SessionHeartbeatTest::Observe;
            else if (option == "--heartbeat-silent")
                mode = SessionHeartbeatTest::Silent;
            else
                return InvalidArguments();
        }

        return RunSessionApproval(host, host ? "" : argv[2], TCP_PORT, UDP_PORT, SessionJoinTest::Normal, mode) ? 0 : 1;
    }

    int RunSessionDataCommand(bool host, int argc, char* argv[])
    {
        using session_data_test::Mode;

        const int requiredArgc = host ? 2 : 3;
        auto mode = Mode::Data;

        if (argc > requiredArgc)
        {
            const std::string_view option = argv[requiredArgc];

            if (option == "--spawn")
                mode = Mode::Spawn;
            else if (option == "--lifecycle")
                mode = Mode::Lifecycle;
            else
                return InvalidArguments();
        }

        return session_data_test::Run(host, host ? "" : argv[2], TCP_PORT, UDP_PORT, mode) ? 0 : 1;
    }

    int RunTcpNetworkCommand(bool host, bool sendTest)
    {
        WSADATA wsaData{};
        const int startupError = WSAStartup(MAKEWORD(2, 2), &wsaData);

        if (startupError != 0)
        {
            std::cerr << "WSAStartup failed: " << startupError << '\n';
            return 1;
        }

        const bool passed = sendTest ? RunTcpSendTests(host, TCP_PORT) : RunTcpFramingNetworkTests(host, TCP_PORT);

        if (WSACleanup() == SOCKET_ERROR)
        {
            std::cerr << "WSACleanup failed: " << WSAGetLastError() << '\n';
            return 1;
        }

        if (!passed)
            std::cerr << "[SUITE] FAIL\n";

        return passed ? 0 : 1;
    }

    std::optional<int> TryRunCommand(int argc, char* argv[])
    {
        using namespace network_mgr_test;

        if (argc < 2)
            return std::nullopt;

        struct Command
        {
            std::string_view name;
            int minArgc;
            int maxArgc;
            int (*run)(int, char**);
        };

        static constexpr Command commands[] = {
            { "--help", 2, 2, [](int, char**) { PrintUsage(); return 0; } },
            { "framing-test", 2, 2, [](int, char**) { return RunTcpFramingTests() ? 0 : 1; } },
            { "framing-host", 2, 2, [](int, char**) { return RunTcpNetworkCommand(true, false); } },
            { "framing-client", 2, 2, [](int, char**) { return RunTcpNetworkCommand(false, false); } },
            { "send-host", 2, 2, [](int, char**) { return RunTcpNetworkCommand(true, true); } },
            { "send-client", 2, 2, [](int, char**) { return RunTcpNetworkCommand(false, true); } },
            { "mgr-overflow-test", 2, 2, [](int, char**) { return RunReceiveOverflow(TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "mgr-stop-test", 2, 2, [](int, char**) { return RunListeningShutdown(TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "mgr-active-stop-test", 2, 2, [](int, char**) { return RunActiveShutdown(TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "mgr-host", 2, 2, [](int, char**) { return Run(true, "", TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "mgr-client", 3, 3, [](int, char** args) { return Run(false, args[2], TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "mgr-queue-host", 2, 2, [](int, char**) { return RunQueueTest(true, "", TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "mgr-queue-client", 3, 3, [](int, char** args) { return RunQueueTest(false, args[2], TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "mgr-batch-test", 2, 2, [](int, char**) { return RunSnapshotBatchTest() ? 0 : 1; } },
            { "mgr-batch-host", 2, 2, [](int, char**) { return Run(true, "", TCP_PORT, UDP_PORT, TestMode::SnapshotBatch) ? 0 : 1; } },
            { "mgr-batch-client", 3, 3, [](int, char** args) { return Run(false, args[2], TCP_PORT, UDP_PORT, TestMode::SnapshotBatch) ? 0 : 1; } },
            { "mgr-concurrent-host", 2, 2, [](int, char**) { return Run(true, "", TCP_PORT, UDP_PORT, TestMode::Concurrent) ? 0 : 1; } },
            { "mgr-concurrent-client", 3, 3, [](int, char** args) { return Run(false, args[2], TCP_PORT, UDP_PORT, TestMode::Concurrent) ? 0 : 1; } },
            { "session-host", 2, 3, [](int count, char** args) { return RunSessionCommand(true, count, args); } },
            { "session-client", 3, 4, [](int count, char** args) { return RunSessionCommand(false, count, args); } },
            { "session-bad-token-client", 3, 3, [](int, char** args) { return RunSessionBadTokenClient(args[2], TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "session-room-test", 2, 2, [](int, char**) { return RunSessionRoomLifecycle(TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "session-retry-client", 3, 3, [](int, char** args) { return RunSessionApproval(false, args[2], TCP_PORT, UDP_PORT, SessionJoinTest::InvalidIp) ? 0 : 1; } },
            { "session-connect-failure-client", 3, 3, [](int, char** args) { return RunSessionApproval(false, args[2], TCP_PORT, UDP_PORT, SessionJoinTest::ConnectionRefused) ? 0 : 1; } },
            { "flow-host", 2, 2, [](int, char**) { return Run(true, "", TCP_PORT, UDP_PORT, TestMode::GameFlow) ? 0 : 1; } },
            { "flow-client", 3, 3, [](int, char** args) { return Run(false, args[2], TCP_PORT, UDP_PORT, TestMode::GameFlow) ? 0 : 1; } },
            { "pong", 2, 2, [](int, char**) { return pong_test::Run(TCP_PORT, UDP_PORT) ? 0 : 1; } },
            { "session-data-host", 2, 3, [](int count, char** args) { return RunSessionDataCommand(true, count, args); } },
            { "session-data-client", 3, 4, [](int count, char** args) { return RunSessionDataCommand(false, count, args); } },
            { "session-data-compare", 4, 4, [](int, char** args) { return session_data_test::Compare(args[2], args[3]) ? 0 : 1; } },
            { "snapshot-replay-test", 2, 2, [](int, char**) { return session_data_test::RunSnapshotReplayTest() ? 0 : 1; } },
        };

        const std::string_view name = argv[1];
        for (const auto& command : commands)
        {
            if (command.name != name)
                continue;

            if (argc < command.minArgc || argc > command.maxArgc)
                return InvalidArguments();

            return command.run(argc, argv);
        }

        return std::nullopt;
    }
}

int main(int argc, char* argv[])
{
    if (const auto result = TryRunCommand(argc, argv))
        return *result;

    Options options;

    if (!ParseArguments(argc, argv, options))
        return InvalidArguments();

    WSADATA wsaData{};

    const int startupError = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (startupError != 0)
    {
        std::cerr << "WSAStartup failed: " << startupError << "\n";
        return 1;
    }

    PrintConfiguration(options);

    bool passed = false;

    if (options.scriptedPeer)
        passed = scripted_peer::Run(options.host, options.peerBehavior, TCP_PORT);

    else if (options.host)
        passed = RunHost(TCP_PORT, UDP_PORT, options.repeats, options.rejectJoin);

    else
        passed = RunClient(TCP_PORT, UDP_PORT, options.expectReject);

    if (WSACleanup() == SOCKET_ERROR)
    {
        std::cerr << "WSACleanup failed: " << WSAGetLastError() << "\n";
        return 1;
    }

    if (!passed)
    {
        std::cerr << "[FAIL] NetHarness did not complete successfully.\n";
        return 1;
    }

    if (options.scriptedPeer)
        std::cout << "[PEER] 지정된 동작 완료. 시험 대상의 로그를 확인하십시오.\n";

    else if (options.host && options.rejectJoin)
        std::cout << "[HOST] REJECT 송신 절차 완료\n";

    else if (options.expectReject)
        std::cout << "[PASS] REJECT 수신 시험 완료\n";

    else
        std::cout << "[PASS] 접속·준비·시작 및 메시지 시험 완료\n";

    return 0;
}
