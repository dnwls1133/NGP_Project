#include "ScriptedPeer.h"
#include "TestTransport.h"
#include "TcpTestSupport.h"

#include <conio.h>
#include <array>
#include <chrono>
#include <iostream>
#include <string>

namespace
{
    bool ConnectToHost(SOCKET& connected, unsigned short port)
    {
        sockaddr_in address{};
        if (!tcp_test::ReadHostAddress(address, port))
            return false;

        connected = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (connected == INVALID_SOCKET)
            return tcp_test::ReportError("socket", WSAGetLastError());

        const auto result = transport::ConnectTcp(connected, address, 5000);

        if (result.status != transport::IoStatus::Success)
            return tcp_test::ReportError("connect", result.errorCode);

        std::cout << "[PEER] Host 연결 완료\n";
        return true;
    }

    bool RunSilent(SOCKET connected)
    {
        constexpr int HOLD_MS = 7000;

        std::cout
            << "[PEER] 메시지를 보내지 않고 최대 7초 대기\n"
            << "[PEER] 받은 데이터에는 응답하지 않습니다.\n";

        const auto started = tcp_test::Clock::now();

        while (tcp_test::Clock::now() - started < std::chrono::milliseconds(HOLD_MS))
        {
            if (tcp_test::QuitRequested())
            {
                std::cout << "[PEER] 사용자 취소\n";
                return false;
            }

            transport::SocketReady ready{};

            if (!transport::WaitForSockets(connected, INVALID_SOCKET, false, false, 50, ready))
                return tcp_test::ReportError("select", WSAGetLastError());

            if (!ready.tcpReadable)
                continue;

            std::array<char, 4096> buffer{};
            const auto result = transport::ReceiveTcp(connected, buffer.data(), static_cast<int>(buffer.size()));

            if (result.status == transport::IoStatus::WouldBlock)
                continue;

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tcp_test::Clock::now() - started).count();

            if (result.status == transport::IoStatus::Closed)
            {
                std::cout << "[PEER] 상대가 연결 종료. elapsed=" << elapsed << "ms\n";
                return true;
            }

            if (result.status == transport::IoStatus::Error)
            {
                if (result.errorCode == WSAECONNRESET || result.errorCode == WSAECONNABORTED)
                {
                    std::cout << "[PEER] 상대 연결 해제. error=" << result.errorCode << ", elapsed=" << elapsed << "ms\n";
                    return true;
                }

                return tcp_test::ReportError("recv", result.errorCode);
            }

            std::cout << "[PEER] " << result.bytes << " bytes 수신, 응답 생략\n";
        }

        std::cout << "[PEER] 7초 경과. 시험용 상대가 연결을 종료합니다.\n";
        return true;
    }
}

namespace scripted_peer
{
    bool Run(bool asHost, Behavior behavior, unsigned short tcpPort)
    {
        SOCKET listener = INVALID_SOCKET;
        SOCKET connected = INVALID_SOCKET;

        bool connectionReady = false;
        bool completed = false;

        std::cout
            << "[PEER] 시험용 " << (asHost ? "Host" : "Client") << "\n"
            << "[PEER] Q 키: 취소\n";

        if (asHost)
        {
            connectionReady = tcp_test::CreateListener(listener, tcpPort) && tcp_test::WaitForClient(listener, connected);

            transport::CloseSocket(listener);
        }
        else
        {
            connectionReady = ConnectToHost(connected, tcpPort);
        }

        if (connectionReady)
        {
            if (behavior == Behavior::Close)
            {
                std::cout << "[PEER] 연결 직후 소켓을 닫습니다.\n";
                completed = true;
            }
            else
            {
                completed = RunSilent(connected);
            }
        }

        // 생성된 소켓을 정리
        // 연결 준비 중 실패한 경우도 해야함
        transport::CloseSocket(connected);
        transport::CloseSocket(listener);

        return completed;
    }
}
