#include "TcpTestSupport.h"

#include <iostream>
#include <conio.h>
#include <string>

namespace tcp_test
{
    bool ReportError(const char* operation, int error)
    {
        std::cerr << "[PEER] " << operation << " failed: " << error << "\n";
        return false;
    }

    bool QuitRequested()
    {
        if (!_kbhit())
            return false;

        const int key = _getch();
        return key == 'q' || key == 'Q';
    }

    // ----------------------------------------------------------------

    bool WaitReady(SOCKET sock, bool writing, Clock::time_point deadline)
    {
        while (Clock::now() < deadline)
        {
            transport::SocketReady ready{};

            if (!transport::WaitForSockets(sock, INVALID_SOCKET, writing, false, 50, ready))
            {
                std::cerr << "[NETWORK] select error=" << WSAGetLastError() << "\n";
                return false;
            }

            if (writing ? ready.tcpWritable : ready.tcpReadable)
                return true;
        }

        std::cerr << "[NETWORK] Timeout\n";
        return false;
    }

    bool CreateListener(SOCKET& listener, unsigned short port)
    {
        if (listener != INVALID_SOCKET)
            return ReportError("CreateListener", WSAEINVAL);

        TestSocket temp;
        temp.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (temp.value == INVALID_SOCKET)
            return ReportError("socket", WSAGetLastError());

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);

        if (bind(temp.value, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
            return ReportError("bind", WSAGetLastError());

        if (listen(temp.value, 1) == SOCKET_ERROR)
            return ReportError("listen", WSAGetLastError());

        if (!transport::SetNonBlocking(temp.value))
            return ReportError("SetNonBlocking", WSAGetLastError());

        listener = temp.value;
        temp.value = INVALID_SOCKET;

        return true;
    }

    bool ReadHostAddress(sockaddr_in& out, unsigned short port)
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

        out = address;
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
                return tcp_test::ReportError("select", WSAGetLastError());

            if (!ready.tcpReadable)
                continue;

            connected = accept(listener, nullptr, nullptr);

            if (connected == INVALID_SOCKET)
            {
                const int error = WSAGetLastError();

                if (error == WSAEWOULDBLOCK)
                    continue;

                return tcp_test::ReportError("accept", error);
            }

            if (!transport::SetNonBlocking(connected))
                return tcp_test::ReportError("SetNonBlocking", WSAGetLastError());

            std::cout << "[PEER] Client 연결 수락\n";
            return true;
        }

        return tcp_test::ReportError("accept timeout", WSAETIMEDOUT);
    }
    
    bool OpenNetworkSocket(bool host, unsigned short port, TestSocket& connection)
    {
        if (host)
        {
            TestSocket listener{};
            if (!CreateListener(listener.value, port))
                return false;

            std::cout
                << "[HOST] TCP port: " << port << "\n"
                << "Client 대기 중... (최대 60초)\n";

            const auto deadline = Clock::now() + std::chrono::milliseconds(ACCEPT_TIMEOUT_MS);

            if (!WaitReady(listener.value, false, deadline))
                return false;

            connection.value = accept(listener.value, nullptr, nullptr);

            if (connection.value == INVALID_SOCKET)
                return false;

            if (!transport::SetNonBlocking(connection.value))
                return false;

            std::cout << "[HOST] Client 연결 완료\n";
            return true;
        }

        
        sockaddr_in address{};
        if (!ReadHostAddress(address, port))
            return false;

        connection.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connection.value == INVALID_SOCKET)
            return false;

        const auto result = transport::ConnectTcp(connection.value, address, NETWORK_TIMEOUT_MS);

        if (result.status != transport::IoStatus::Success)
        {
            std::cerr << "[CLIENT] Connect error=" << result.errorCode << "\n";
            return false;
        }

        std::cout << "[CLIENT] Host 연결 완료\n";
        return true;
    }

    bool TransferExact(SOCKET sock, char* data, std::size_t size, bool sending)
    {
        const auto deadline = Clock::now() + std::chrono::milliseconds(NETWORK_TIMEOUT_MS);
        std::size_t offset = 0;

        while (offset < size)
        {
            if (!WaitReady(sock, sending, deadline))
                return false;

            const int remaining = static_cast<int>(size - offset);

            const auto result = sending
                ? transport::SendTcp(sock, data + offset, remaining)
                : transport::ReceiveTcp(sock, data + offset, remaining);

            if (result.status == transport::IoStatus::WouldBlock)
                continue;

            if (result.status != transport::IoStatus::Success || result.bytes <= 0)
            {
                std::cerr << "[NETWORK] " << (sending ? "Send" : "Receive") << " failed, status=" << static_cast<int>(result.status) << ", socketError=" << result.errorCode << "\n";
                return false;
            }

            offset += static_cast<std::size_t>(result.bytes);
        }

        return true;
    }

    bool SendStepResult(SOCKET sock, bool passed)
    {
        char result = passed ? 1 : 0;
        return TransferExact(sock, &result, 1, true);
    }

    bool ReceiveStepResult(SOCKET sock)
    {
        char result = 0;

        if (!TransferExact(sock, &result, 1, false))
            return false;

        if (result != 1)
        {
            std::cerr << "[FAIL] 상대방의 복원 검증 실패\n";
            return false;
        }

        return true;
    }
}
