#include "ScriptedPeer.h"
#include "TestTransport.h"

#include <conio.h>
#include <array>
#include <chrono>
#include <iostream>
#include <string>

namespace
{
    using Clock = std::chrono::steady_clock;

    bool QuitRequested()
    {
        if (!_kbhit())
            return false;

        const int key = _getch();
        return key == 'q' || key == 'Q';
    }

    bool ReportError(const char* operation, int error)
    {
        std::cerr << "[PEER] " << operation << " failed: " << error << "\n";
        return false;
    }

    bool CreateListener(SOCKET& listener, unsigned short port)
    {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (listener == INVALID_SOCKET)
            return ReportError("socket", WSAGetLastError());

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);

        if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
            return ReportError("bind", WSAGetLastError());

        if (listen(listener, 1) == SOCKET_ERROR)
            return ReportError("listen", WSAGetLastError());

        if (!transport::SetNonBlocking(listener))
            return ReportError("SetNonBlocking", WSAGetLastError());

        return true;
    }

    bool WaitForClient(SOCKET listener, SOCKET& connected)
    {
        std::cout << "[PEER] Client 대기 중... (최대 30초)\n";

        const auto deadline = Clock::now() + std::chrono::seconds(30);

        while (Clock::now() < deadline)
        {
            if (QuitRequested())
            {
                std::cout << "[PEER] 사용자 취소\n";
                return false;
            }

            transport::SocketReady ready{};

            if (!transport::WaitForSockets(listener, INVALID_SOCKET, false, false, 50, ready))
                return ReportError("select", WSAGetLastError());

            if (!ready.tcpReadable)
                continue;

            connected = accept(listener, nullptr, nullptr);

            if (connected == INVALID_SOCKET)
            {
                const int error = WSAGetLastError();

                if (error == WSAEWOULDBLOCK)
                    continue;

                return ReportError("accept", error);
            }

            if (!transport::SetNonBlocking(connected))
                return ReportError("SetNonBlocking", WSAGetLastError());

            std::cout << "[PEER] Client 연결 수락\n";
            return true;
        }

        return ReportError("accept timeout", WSAETIMEDOUT);
    }

    bool ConnectToHost(SOCKET& connected, unsigned short port)
    {
        std::string ip;

        std::cout << "Host IP를 입력해 주십시오: ";

        if (!(std::cin >> ip))
            return false;

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) != 1)
        {
            std::cerr << "[PEER] Invalid IPv4 address\n";
            return false;
        }

        connected = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (connected == INVALID_SOCKET)
            return ReportError("socket", WSAGetLastError());

        const auto result = transport::ConnectTcp(connected, address, 5000);

        if (result.status != transport::IoStatus::Success)
            return ReportError("connect", result.errorCode);

        std::cout << "[PEER] Host 연결 완료\n";
        return true;
    }

    bool RunSilent(SOCKET connected)
    {
        constexpr int HOLD_MS = 7000;

        std::cout
            << "[PEER] 메시지를 보내지 않고 최대 7초 대기\n"
            << "[PEER] 받은 데이터에는 응답하지 않습니다.\n";

        const auto started = Clock::now();

        while (Clock::now() - started < std::chrono::milliseconds(HOLD_MS))
        {
            if (QuitRequested())
            {
                std::cout << "[PEER] 사용자 취소\n";
                return false;
            }

            transport::SocketReady ready{};

            if (!transport::WaitForSockets(connected, INVALID_SOCKET, false, false, 50, ready))
                return ReportError("select", WSAGetLastError());

            if (!ready.tcpReadable)
                continue;

            std::array<char, 4096> buffer{};
            const auto result = transport::ReceiveTcp(connected, buffer.data(), static_cast<int>(buffer.size()));

            if (result.status == transport::IoStatus::WouldBlock)
                continue;

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();

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

                return ReportError("recv", result.errorCode);
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
            connectionReady = CreateListener(listener, tcpPort) && WaitForClient(listener, connected);

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
