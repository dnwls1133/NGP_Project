#pragma once

#include "TestTransport.h"
#include <chrono>

namespace tcp_test
{
    using Clock = std::chrono::steady_clock;
    using protocol::Message;
    using protocol::MessageType;

    constexpr int NETWORK_TIMEOUT_MS = 10000;
    constexpr int ACCEPT_TIMEOUT_MS = 60000;

    struct TestSocket
    {
        SOCKET value = INVALID_SOCKET;

        TestSocket() = default;
        TestSocket(const TestSocket&) = delete;
        TestSocket& operator=(const TestSocket&) = delete;

        ~TestSocket() { transport::CloseSocket(value); }
    };

    bool ReportError(const char* operation, int error);

    bool QuitRequested();

    // ----------------------------------------------------------------------------

    bool WaitReady(SOCKET sock, bool writing, Clock::time_point deadline);

    bool OpenNetworkSocket(bool host, unsigned short port, TestSocket& connection);

    bool TransferExact(SOCKET sock, char* data, std::size_t size, bool sending);

    bool ReadHostAddress(sockaddr_in& out, unsigned short port);

    bool CreateListener(SOCKET& listener, unsigned short port);

    bool SendStepResult(SOCKET sock, bool passed);

    bool ReceiveStepResult(SOCKET sock);

    bool WaitForClient(SOCKET listener, SOCKET& connected);
}
