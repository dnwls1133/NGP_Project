#include <winsock2.h>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <system_error>

#include "HostTest.h"
#include "ClientTest.h"
#include "MessageTest.h"

#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr unsigned short TCP_PORT = 9000;
    constexpr unsigned short UDP_PORT = 9001;

    struct Options
    {
        bool host = false;
        std::uint32_t repeats = 10;
        bool rejectJoin = false;
        bool expectReject = false;
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
    }

    bool ParseCount(
        std::string_view text,
        std::uint32_t& out)
    {
        std::uint32_t value = 0;

        const auto result = std::from_chars(
            text.data(),
            text.data() + text.size(),
            value);

        if (result.ec != std::errc{} ||
            result.ptr != text.data() + text.size() ||
            value == 0 ||
            value > message_test::MAX_REPEATS)
        {
            return false;
        }

        out = value;
        return true;
    }

    bool ParseArguments(
        int argc,
        char* argv[],
        Options& options)
    {
        if (argc < 2)
            return false;

        const std::string_view mode = argv[1];

        if (mode == "host")
            options.host = true;
        else if (mode == "client")
            options.host = false;
        else
            return false;

        bool countSpecified = false;

        for (int i = 2; i < argc; ++i)
        {
            const std::string_view argument = argv[i];

            if (argument == "--count")
            {
                if (!options.host ||
                    countSpecified ||
                    i + 1 >= argc)
                {
                    return false;
                }

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
            {
                return false;
            }
        }

        // 거부 시험에서는 메시지 반복 시험을 실행하지 않는다.
        if (options.rejectJoin && countSpecified)
            return false;

        return true;
    }

    void PrintConfiguration(const Options& options)
    {
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
                std::cout << "Test: session flow + 19 message types\n"
                << "Repeat count: " << options.repeats << "\n";
        }
        else if (options.expectReject)
        {
            std::cout << "Test: expect JOIN rejection\n";
        }
        else
        {
            std::cout << "Test: session flow + 19 message types\n"
                << "Repeat count: supplied by host\n";
        }

        std::cout << "\n";
    }
}

int main(int argc, char* argv[])
{
    if (argc == 2 && std::string_view(argv[1]) == "--help")
    {
        PrintUsage();
        return 0;
    }

    Options options;

    if (!ParseArguments(argc, argv, options))
    {
        std::cerr << "Invalid arguments.\n\n";
        PrintUsage();
        return 1;
    }

    WSADATA wsaData{};

    const int startupError =
        WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (startupError != 0)
    {
        std::cerr << "WSAStartup failed: "
            << startupError << "\n";
        return 1;
    }

    PrintConfiguration(options);

    const bool passed = options.host
        ? RunHost(
            TCP_PORT,
            UDP_PORT,
            options.repeats,
            options.rejectJoin)
        : RunClient(
            TCP_PORT,
            UDP_PORT,
            options.expectReject);

    if (WSACleanup() == SOCKET_ERROR)
    {
        std::cerr << "WSACleanup failed: "
            << WSAGetLastError() << "\n";
        return 1;
    }

    if (!passed)
    {
        std::cerr << "[FAIL] NetHarness did not complete successfully.\n";
        return 1;
    }

    if (options.host && options.rejectJoin)
    {
        std::cout << "[HOST] REJECT 송신 절차 완료\n";
    }
    else if (options.expectReject)
    {
        std::cout << "[PASS] REJECT 수신 시험 완료\n";
    }
    else
    {
        std::cout << "[PASS] 접속·준비·시작 및 메시지 시험 완료\n";
    }

    return 0;
}
