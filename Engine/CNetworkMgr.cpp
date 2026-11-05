#include "pch.h"
#include "CNetworkMgr.h"
#include "Common.h"
#include "../shared/protocol/PacketCodec.h"

#include <array>
#include <span>
#include <utility>
#include <iostream>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")

namespace network
{
    CNetworkMgr::CNetworkMgr() = default;

    CNetworkMgr::~CNetworkMgr()
    {
        Shutdown();
    }

    NetworkResult CNetworkMgr::Start(const NetworkConfig& config)
    {
        // 1. 이미 시작됐는지 확인
        if (netState.load() != NetworkState::Stopped || initialized || transportThread != nullptr)
            return NetworkResult::AlreadyRunning;

        // 2. 포트·역할·Client IP 검사
        if (config.tcpPort == 0 || config.udpPort == 0)
            return NetworkResult::InvalidArgument;

        if (config.role != NetworkRole::Host && config.role != NetworkRole::Client)
        {
            std::cerr << "[NETWORK] Invalid Network Role\n";
            return NetworkResult::InvalidArgument;
        }

        if (config.role == NetworkRole::Client)
        {
            if (config.connectTimeoutMs <= 0)
                return NetworkResult::InvalidArgument;

            in_addr address{};
            if (inet_pton(AF_INET, config.hostIp.c_str(), &address) != 1)
                return NetworkResult::InvalidArgument;
        }

        netState.store(NetworkState::Starting);

        // 3. WSAStartup
        WSADATA wsa{};
        const int startupError = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (startupError != 0)
        {
            std::cerr << "WSAStartup failed: " << startupError << "\n";
            netState.store(NetworkState::Stopped);
            return NetworkResult::StartupFailed;
        }
        initialized = true;

        // 4. this->config에 설정 복사
        this->config = config;

        // 5. 중지 플래그 해제
        stopRequested.store(false);

        // 6. 통신 스레드 생성
        //    생성 실패 시 WSACleanup 등 시작 과정 정리
        {
            std::lock_guard lock(queueMutex);
            recvQueue.clear();
            networkEventQueue.clear();
        }

        transportThread = CreateThread(nullptr, 0, ThreadEntry, this, 0, nullptr);
        if (transportThread == nullptr)
        {
            const DWORD error = GetLastError();
            std::cerr << "CreateThread failed: " << error << "\n";

            Shutdown();
            return NetworkResult::StartupFailed;
        }

        // 7. 시작 요청 성공 반환
        return NetworkResult::Success;
    }

    DWORD WINAPI CNetworkMgr::ThreadEntry(LPVOID parameter)
    {
        auto* manager = static_cast<CNetworkMgr*>(parameter);
        manager->NetworkThread();
        return 0;
    }

    void CNetworkMgr::Shutdown()
    {
        // Start와 Shutdown은 메인 스레드에서만
        stopRequested.store(true);

        if (transportThread != nullptr)
        {
            const DWORD result = WaitForSingleObject(transportThread, INFINITE);

            if (result != WAIT_OBJECT_0)
            {
                std::cerr << "Thread wait failed: " << GetLastError() << "\n";
                return;
            }

            CloseHandle(transportThread);
            transportThread = nullptr;
        }

        // 통신 스레드 종료 전 소켓 정리
        if (initialized)
        {
            if (WSACleanup() == SOCKET_ERROR)
                std::cerr << "WSACleanup failed: " << WSAGetLastError() << "\n";

            initialized = false;
        }

        netState.store(NetworkState::Stopped);
    }

    NetworkResult CNetworkMgr::StartHost(unsigned short tcpPort, unsigned short udpPort)
    {
        NetworkConfig config;
        config.role = NetworkRole::Host;
        config.tcpPort = tcpPort;
        config.udpPort = udpPort;

        return Start(config);
    }

    NetworkResult CNetworkMgr::StartClient(const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort)
    {
        NetworkConfig config;
        config.role = NetworkRole::Client;
        config.hostIp = hostIp;
        config.tcpPort = tcpPort;
        config.udpPort = udpPort;

        return Start(config);
    }

    NetworkResult CNetworkMgr::SendTcp(const protocol::Message& message)
    {
        const auto type = message.header.type;

        if (type < protocol::MessageType::JOIN || type > protocol::MessageType::END)
            return NetworkResult::InvalidArgument;

        if (protocol::IsUdpMessageType(type))
            return NetworkResult::WrongTransport;

        std::lock_guard lock(queueMutex);

        if (netState.load() != NetworkState::Connected || stopRequested.load())
            return NetworkResult::NotConnected;

        if (sendRequestQueue.size() >= MAX_SEND_REQUESTS)
            return NetworkResult::QueueFull;

        // TCP는 이미 연결된 상대에게 보낸다.
        sendRequestQueue.push_back(SendRequest{ TransportType::TCP, {}, message });

        return NetworkResult::Success;
    }

    NetworkResult CNetworkMgr::SendUdp(const sockaddr_in& destination, const protocol::Message& message)
    {
        const auto type = message.header.type;

        if (type < protocol::MessageType::JOIN || type > protocol::MessageType::END)
            return NetworkResult::InvalidArgument;

        if (!protocol::IsUdpMessageType(type))
            return NetworkResult::WrongTransport;

        if (destination.sin_family != AF_INET || destination.sin_port == 0)
            return NetworkResult::InvalidArgument;

        std::lock_guard lock(queueMutex);

        if (netState.load() != NetworkState::Connected || stopRequested.load())
            return NetworkResult::NotConnected;

        if (sendRequestQueue.size() >= MAX_SEND_REQUESTS)
            return NetworkResult::QueueFull;

        sendRequestQueue.push_back(SendRequest{ TransportType::UDP, destination, message });

        return NetworkResult::Success;
    }

    bool CNetworkMgr::TryReceive(ReceivedMessage& out)
    {
        std::lock_guard lock(queueMutex);

        if (recvQueue.empty())
            return false;

        out = std::move(recvQueue.front());
        recvQueue.pop_front();

        return true;
    }

    bool CNetworkMgr::TryPopEvent(NetworkEvent& out)
    {
        std::lock_guard lock(queueMutex);

        if (networkEventQueue.empty())
            return false;

        out = std::move(networkEventQueue.front());
        networkEventQueue.pop_front();

        return true;
    }

    void CNetworkMgr::NetworkThread()
    {
        using Clock = std::chrono::steady_clock;

        const auto closeSocket = [](SOCKET& socketHandle) {
            if (socketHandle == INVALID_SOCKET)
                return;

            closesocket(socketHandle);
            socketHandle = INVALID_SOCKET;
            };

        const auto setNonBlocking = [](SOCKET socketHandle) {
            u_long enabled = 1;
            return ioctlsocket(socketHandle, FIONBIO, &enabled) != SOCKET_ERROR;
            };

        const auto bindLocal = [](SOCKET socketHandle, unsigned short port) {
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_ANY);
            address.sin_port = htons(port);

            return bind(socketHandle, reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) != SOCKET_ERROR;
            };

        const auto reportError = [this](int socketError) {
            PushEvent(NetworkEvent{ NetworkEventType::Error, peerTcpAddress, socketError });
            };

        const auto markConnected = [this]() {
            netState.store(NetworkState::Connected);
            PushEvent(NetworkEvent{ NetworkEventType::Connected, peerTcpAddress });
            };

        const auto run = [&]() {
            const bool isHost = config.role == NetworkRole::Host;
            Clock::time_point connectDeadline{};

            udpSO = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

            if (udpSO == INVALID_SOCKET || !bindLocal(udpSO, isHost ? config.udpPort : 0) || !setNonBlocking(udpSO))
            {
                reportError(WSAGetLastError());
                return;
            }

            if (isHost)
            {
                listenSO_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

                if (listenSO_tcp == INVALID_SOCKET || !bindLocal(listenSO_tcp, config.tcpPort) || listen(listenSO_tcp, 1) == SOCKET_ERROR || !setNonBlocking(listenSO_tcp))
                {
                    reportError(WSAGetLastError());
                    return;
                }

                netState.store(NetworkState::Listening);
                PushEvent(NetworkEvent{ NetworkEventType::Listening });
            }
            else
            {
                peerTcpAddress.sin_family = AF_INET;
                peerTcpAddress.sin_port = htons(config.tcpPort);

                if (inet_pton(AF_INET, config.hostIp.c_str(), &peerTcpAddress.sin_addr) != 1)
                {
                    reportError(WSAEINVAL);
                    return;
                }

                tcpSO = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

                if (tcpSO == INVALID_SOCKET || !setNonBlocking(tcpSO))
                {
                    reportError(WSAGetLastError());
                    return;
                }

                netState.store(NetworkState::Connecting);
                connectDeadline = Clock::now() + std::chrono::milliseconds(config.connectTimeoutMs);

                const int result = connect(tcpSO, reinterpret_cast<const sockaddr*>(&peerTcpAddress),
                    sizeof(peerTcpAddress));

                if (result == 0)
                    markConnected();

                else
                {
                    const int error = WSAGetLastError();

                    if (error != WSAEWOULDBLOCK)
                    {
                        reportError(error);
                        return;
                    }
                }
            }

            std::deque<SendRequest> requests;

            while (!stopRequested.load())
            {
                {
                    std::lock_guard lock(queueMutex);
                    requests.swap(sendRequestQueue);
                }

                for (const auto& request : requests)
                {
                    if (stopRequested.load())
                        return;

                    const bool isTcp = request.transport == TransportType::TCP;
                    const std::size_t pendingCount = isTcp ? tcpSendQueue.size() : udpSendQueue.size();

                    const sockaddr_in& destination = isTcp ? peerTcpAddress : request.destination;

                    std::vector<std::uint8_t> bytes;
                    const auto error = protocol::CPacketCodec::Encode(request.message, bytes);

                    if (error != protocol::CodecError::NONE)
                    {
                        PushEvent(NetworkEvent{ NetworkEventType::Error, destination, 0, error });
                        continue;
                    }

                    if (pendingCount >= MAX_SEND_REQUESTS)
                    {
                        PushEvent(NetworkEvent{ NetworkEventType::Error, destination, WSAENOBUFS });
                        return;
                    }

                    if (isTcp)
                        tcpSendQueue.push_back(std::move(bytes));
                    else
                        udpSendQueue.push_back(PendingDatagram{ destination, std::move(bytes) });
                }
                requests.clear();

                const NetworkState state = netState.load();

                fd_set readSet;
                fd_set writeSet;
                fd_set errorSet;
                FD_ZERO(&readSet);
                FD_ZERO(&writeSet);
                FD_ZERO(&errorSet);

                FD_SET(udpSO, &readSet);

                if (!udpSendQueue.empty())
                    FD_SET(udpSO, &writeSet);

                if (state == NetworkState::Listening)
                    FD_SET(listenSO_tcp, &readSet);
                else if (state == NetworkState::Connecting)
                {
                    FD_SET(tcpSO, &writeSet);
                    FD_SET(tcpSO, &errorSet);
                }
                else if (state == NetworkState::Connected)
                {
                    FD_SET(tcpSO, &readSet);

                    if (!tcpSendQueue.empty())
                        FD_SET(tcpSO, &writeSet);
                }

                timeval timeout{};
                timeout.tv_usec = 20'000;

                const int selected = select(0, &readSet, &writeSet, &errorSet, &timeout);

                if (selected == SOCKET_ERROR)
                {
                    reportError(WSAGetLastError());
                    return;
                }

                if (stopRequested.load())
                    return;

                if (state == NetworkState::Connecting)
                {
                    if (FD_ISSET(tcpSO, &writeSet) || FD_ISSET(tcpSO, &errorSet))
                    {
                        int socketError = 0;
                        int optionSize = sizeof(socketError);

                        if (getsockopt(tcpSO, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &optionSize) == SOCKET_ERROR)
                        {
                            reportError(WSAGetLastError());
                            return;
                        }

                        if (socketError != 0)
                        {
                            reportError(socketError);
                            return;
                        }

                        markConnected();
                    }
                    else if (Clock::now() >= connectDeadline)
                    {
                        reportError(WSAETIMEDOUT);
                        return;
                    }
                }
                else if (state == NetworkState::Listening && FD_ISSET(listenSO_tcp, &readSet))
                {
                    sockaddr_in peer{};
                    int peerSize = sizeof(peer);
                    tcpSO = accept(listenSO_tcp, reinterpret_cast<sockaddr*>(&peer), &peerSize);

                    if (tcpSO == INVALID_SOCKET)
                    {
                        const int error = WSAGetLastError();

                        if (error == WSAEWOULDBLOCK)
                            continue;

                        reportError(error);
                        return;
                    }

                    peerTcpAddress = peer;

                    if (!setNonBlocking(tcpSO))
                    {
                        reportError(WSAGetLastError());
                        return;
                    }

                    closeSocket(listenSO_tcp);
                    markConnected();
                }
                else if (state == NetworkState::Connected)
                {
                    if (FD_ISSET(tcpSO, &writeSet))
                        FlushTcpSend();

                    if (stopRequested.load())
                        return;

                    if (FD_ISSET(tcpSO, &readSet))
                        ReceiveTcp();
                }

                if (stopRequested.load())
                    return;

                if (FD_ISSET(udpSO, &writeSet))
                    FlushUdpSend();

                if (stopRequested.load())
                    return;

                if (FD_ISSET(udpSO, &readSet))
                    ReceiveUdp();
            }
        };
        run();

        {
            std::lock_guard lock(queueMutex);
            stopRequested.store(true);
            sendRequestQueue.clear();
        }

        closeSocket(listenSO_tcp);
        closeSocket(tcpSO);
        closeSocket(udpSO);

        tcpSendQueue.clear();
        tcpSendOffset = 0;
        tcpPartialFrame.clear();
        udpSendQueue.clear();
        peerTcpAddress = {};

        netState.store(NetworkState::Stopped);
    }

    void CNetworkMgr::FlushTcpSend()
    {
        if (tcpSendQueue.empty())
            return;

        const auto stopWithError = [this](int error) {
            PushEvent(NetworkEvent{ NetworkEventType::Error, peerTcpAddress, error });
            stopRequested.store(true);
            };

        const auto& frame = tcpSendQueue.front();

        if (tcpSendOffset >= frame.size())
        {
            stopWithError(WSAEINVAL);
            return;
        }

        const std::size_t remaining = frame.size() - tcpSendOffset;
        const char* data = reinterpret_cast<const char*>(frame.data() + tcpSendOffset);
        const int sent = send(tcpSO, data, static_cast<int>(remaining), 0);

        if (sent == SOCKET_ERROR)
        {
            const int error = WSAGetLastError();

            if (error != WSAEWOULDBLOCK)
                stopWithError(error);

            return;
        }

        if (sent == 0)
        {
            stopWithError(WSAEINVAL);
            return;
        }

        tcpSendOffset += static_cast<std::size_t>(sent);

        if (tcpSendOffset == frame.size())
        {
            tcpSendQueue.pop_front();
            tcpSendOffset = 0;
        }
    }

    void CNetworkMgr::ReceiveTcp()
    {
        const auto stopWithError = [this](int socketError, protocol::CodecError codecError = protocol::CodecError::NONE) {
            PushEvent(NetworkEvent{ NetworkEventType::Error, peerTcpAddress, socketError, codecError });
            stopRequested.store(true);
            };

        std::array<std::uint8_t, 4096> buffer{};
        const int received = recv(tcpSO, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);

        if (received == SOCKET_ERROR)
        {
            const int error = WSAGetLastError();

            if (error != WSAEWOULDBLOCK)
                stopWithError(error);

            return;
        }

        if (received == 0)
        {
            if (!tcpPartialFrame.empty())
                PushEvent(NetworkEvent{ NetworkEventType::Error, peerTcpAddress, 0, protocol::CodecError::TRUNCATED });

            PushEvent(NetworkEvent{ NetworkEventType::Disconnected, peerTcpAddress });
            stopRequested.store(true);
            return;
        }

        std::span<const std::uint8_t> remaining(buffer.data(), static_cast<std::size_t>(received));

        const auto appendUntil = [this, &remaining](std::size_t targetBytes)
            {
                const std::size_t needed = targetBytes - tcpPartialFrame.size();
                const std::size_t count = (std::min)(needed, remaining.size());
                const auto chunk = remaining.first(count);

                tcpPartialFrame.insert(tcpPartialFrame.end(), chunk.begin(), chunk.end());
                remaining = remaining.subspan(count);
            };

        while (!remaining.empty())
        {
            if (tcpPartialFrame.size() < protocol::HEADER_BYTES)
            {
                appendUntil(protocol::HEADER_BYTES);

                if (tcpPartialFrame.size() < protocol::HEADER_BYTES)
                    return;
            }

            protocol::PacketHeader header{};
            const auto headerError = protocol::CPacketCodec::DecodeHeader(tcpPartialFrame, header);

            if (headerError != protocol::CodecError::NONE)
            {
                stopWithError(0, headerError);
                return;
            }

            if (protocol::IsUdpMessageType(header.type))
            {
                stopWithError(WSAEPROTOTYPE);
                return;
            }

            appendUntil(header.totalBytes);

            if (tcpPartialFrame.size() < header.totalBytes)
                return;

            protocol::Message message{};
            const auto decodeError = protocol::CPacketCodec::Decode(tcpPartialFrame, message);

            if (decodeError != protocol::CodecError::NONE)
            {
                stopWithError(0, decodeError);
                return;
            }

            {
                std::lock_guard lock(queueMutex);
                recvQueue.push_back(ReceivedMessage{ TransportType::TCP, peerTcpAddress, std::move(message) });
            }

            tcpPartialFrame.clear();
        }
    }

    void CNetworkMgr::FlushUdpSend()
    {
        if (udpSendQueue.empty())
            return;

        const auto& datagram = udpSendQueue.front();
        const int size = static_cast<int>(datagram.bytes.size());

        const int sent = sendto(udpSO, reinterpret_cast<const char*>(datagram.bytes.data()), size, 0, reinterpret_cast<const sockaddr*>(&datagram.destination), sizeof(datagram.destination));

        if (sent == size)
        {
            udpSendQueue.pop_front();
            return;
        }

        const int error = sent == SOCKET_ERROR ? WSAGetLastError() : WSAEMSGSIZE;

        if (error == WSAEWOULDBLOCK)
            return;

        NetworkEvent event{};
        event.type = NetworkEventType::Error;
        event.peer = datagram.destination;
        event.socketError = error;

        udpSendQueue.pop_front();

        PushEvent(event);

        if (error == WSAENOTSOCK || error == WSAESHUTDOWN || error == WSANOTINITIALISED || error == WSAENETDOWN)
            stopRequested.store(true);
    }

    void CNetworkMgr::ReceiveUdp()
    {
        std::array<std::uint8_t, protocol::MAX_UDP_BYTES + 1> buffer{};
        sockaddr_in sender{};
        int senderSize = sizeof(sender);

        const int received = recvfrom(udpSO, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&sender), &senderSize);

        if (received == SOCKET_ERROR)
        {
            const int error = WSAGetLastError();

            if (error == WSAEWOULDBLOCK || error == WSAEMSGSIZE)
                return;

            NetworkEvent event{};
            event.type = NetworkEventType::Error;
            event.socketError = error;

            PushEvent(event);

            // 목적지 포트 오류, 이벤트로 전달 후 계속 통신
            if (error != WSAECONNRESET)
                stopRequested.store(true);

            return;
        }

        const auto byteCount = static_cast<std::size_t>(received);

        if (byteCount < protocol::HEADER_BYTES || byteCount > protocol::MAX_UDP_BYTES)
            return;

        protocol::Message message{};
        const std::span<const std::uint8_t> bytes(buffer.data(), byteCount);

        if (protocol::CPacketCodec::Decode(bytes, message) != protocol::CodecError::NONE)
            return;

        if (!protocol::IsUdpMessageType(message.header.type))
            return;

        std::lock_guard lock(queueMutex);
        recvQueue.push_back(ReceivedMessage{ TransportType::UDP, sender, std::move(message) });
    }

    void CNetworkMgr::PushEvent(const NetworkEvent& event)
    {
        std::lock_guard lock(queueMutex);
        networkEventQueue.push_back(event);
    }
}
