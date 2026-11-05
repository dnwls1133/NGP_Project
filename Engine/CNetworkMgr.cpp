#include "pch.h"
#include "CNetworkMgr.h"
#include "Common.h"
#include "../net/SendContract.h"
#include "../shared/protocol/PacketCodec.h"

#include <array>
#include <span>
#include <utility>
#include <iostream>
#include <chrono>
#include <exception>
#include <new>

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
        if (netState.load() != NetworkState::Stopped || initialized || transportThread.joinable())
            return NetworkResult::AlreadyRunning;

        // 2. 포트·역할·Client IP 검사
        if (config.tcpPort == 0 || config.udpPort == 0)
            return NetworkResult::InvalidArgument;

        if (config.role != NetworkRole::Host && config.role != NetworkRole::Client)
        {
            std::cerr << "[NETWORK] Invalid Network Role\n";
            return NetworkResult::InvalidArgument;
        }

        sockaddr_in initialUdpPeer{};
        if (config.role == NetworkRole::Client)
        {
            if (config.connectTimeoutMs <= 0)
                return NetworkResult::InvalidArgument;

            in_addr address{};
            if (inet_pton(AF_INET, config.hostIp.c_str(), &address) != 1)
                return NetworkResult::InvalidArgument;

            initialUdpPeer.sin_family = AF_INET;
            initialUdpPeer.sin_port = htons(config.udpPort);
            initialUdpPeer.sin_addr = address;
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

        try
        {
            this->config = config;
            stopRequested.store(false);

            {
                std::lock_guard lock(sendMutex);
                udpPeerAddress = initialUdpPeer;
            }

            {
                std::lock_guard lock(receiveMutex);
                recvQueue.clear();
                networkEventQueue.clear();
                receiveOverflow.store(false);
                receiveOverflowReported = false;
            }

            transportThread = std::thread(&CNetworkMgr::NetworkThread, this);
        }
        catch (const std::exception& error)
        {
            std::cerr << "[NETWORK] Thread startup failed: " << error.what() << '\n';
            Shutdown();
            return NetworkResult::StartupFailed;
        }

        return NetworkResult::Success;
    }

    void CNetworkMgr::Shutdown()
    {
        // Start와 Shutdown은 메인 스레드에서만
        stopRequested.store(true);

        if (transportThread.joinable())
            transportThread.join();

        {
            std::lock_guard lock(sendMutex);
            udpPeerAddress = {};
        }

        // 통신 스레드가 소켓을 닫고 반환, Winsock 정리
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

        return EnqueueSendRequest(SendRequest{ TransportType::TCP, {}, message });
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

        return EnqueueSendRequest(SendRequest{ TransportType::UDP, destination, message });
    }

    NetworkResult CNetworkMgr::SendUdp(const protocol::Message& message)
    {
        sockaddr_in destination{};
        {
            std::lock_guard lock(sendMutex);

            if (netState.load() != NetworkState::Connected || stopRequested.load())
                return NetworkResult::NotConnected;

            if (udpPeerAddress.sin_family != AF_INET)
                return NetworkResult::UdpPeerNotSet;

            destination = udpPeerAddress;
        }

        return SendUdp(destination, message);
    }

    net::SendResult CNetworkMgr::EnqueueSend(net::Channel channel, const protocol::Message& message)
    {
        if (netState.load() != NetworkState::Connected || stopRequested.load())
            return net::SendResult::NotRunning;

        try
        {
            const auto validation = net::ValidateSendRequest(channel, message);
            if (validation != net::SendResult::Queued)
                return validation;

            const auto result = channel == net::Channel::Tcp ? SendTcp(message) : SendUdp(message);

            switch (result)
            {
            case NetworkResult::Success:
                return net::SendResult::Queued;
            case NetworkResult::NotConnected:
                return net::SendResult::NotRunning;
            case NetworkResult::QueueFull:
                return net::SendResult::QueueFull;
            default:
                return net::SendResult::InvalidRequest;
            }
        }
        catch (const std::bad_alloc&)
        {
            return net::SendResult::NoMemory;
        }
    }

    net::SendResult CNetworkMgr::EnqueueSnapshotBatch(const net::SnapshotBatch& batch)
    {
        if (netState.load() != NetworkState::Connected || stopRequested.load())
            return net::SendResult::NotRunning;

        try
        {
            const auto validation = net::ValidateSnapshotBatch(batch);
            if (validation != net::SendResult::Queued)
                return validation;

            PendingSnapshotBatch request{ {}, batch };
            const auto& header = request.batch.parts.front().header;

            std::lock_guard lock(sendMutex);

            if (netState.load() != NetworkState::Connected || stopRequested.load())
                return net::SendResult::NotRunning;

            if (config.role != NetworkRole::Host || udpPeerAddress.sin_family != AF_INET)
                return net::SendResult::InvalidRequest;

            const auto last = lastSnapshotTickBySession.find(header.sessionId);
            if (last != lastSnapshotTickBySession.end() && header.hostTick <= last->second)
                return net::SendResult::StaleBatch;

            if (sendRequestQueue.size() + request.batch.parts.size() > MAX_SEND_REQUESTS)
                return net::SendResult::QueueFull;

            request.destination = udpPeerAddress;
            const bool replaced = pendingSnapshotBatch.has_value();
            // 맵 삽입에서 할당이 실패하면 기존 대기 배치는 그대로 유지된다.
            lastSnapshotTickBySession.insert_or_assign(header.sessionId, header.hostTick);
            pendingSnapshotBatch = std::move(request);

            return replaced ? net::SendResult::ReplacedPending : net::SendResult::Queued;
        }
        catch (const std::bad_alloc&)
        {
            return net::SendResult::NoMemory;
        }
    }

    NetworkResult CNetworkMgr::SetUdpPeer(const sockaddr_in& address)
    {
        if (config.role != NetworkRole::Host || address.sin_family != AF_INET ||
            address.sin_port == 0 || address.sin_addr.s_addr == htonl(INADDR_ANY))
        {
            return NetworkResult::InvalidArgument;
        }

        std::lock_guard lock(sendMutex);

        if (netState.load() != NetworkState::Connected || stopRequested.load())
            return NetworkResult::NotConnected;

        udpPeerAddress = address;
        return NetworkResult::Success;
    }

    bool CNetworkMgr::TryReceive(ReceivedMessage& out)
    {
        std::lock_guard lock(receiveMutex);

        if (recvQueue.empty())
            return false;

        out = std::move(recvQueue.front());
        recvQueue.pop_front();

        return true;
    }

    bool CNetworkMgr::TryPopEvent(NetworkEvent& out)
    {
        std::lock_guard lock(receiveMutex);

        if (!networkEventQueue.empty())
        {
            out = std::move(networkEventQueue.front());
            networkEventQueue.pop_front();
            return true;
        }

        if (!receiveOverflow.load() || receiveOverflowReported)
            return false;

        out = NetworkEvent{ NetworkEventType::Error, {}, WSAENOBUFS };
        receiveOverflowReported = true;
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
            std::optional<ActiveSnapshotBatch> activeSnapshot;

            while (!stopRequested.load())
            {
                {
                    std::lock_guard lock(sendMutex);
                    requests.swap(sendRequestQueue);

                    if (!activeSnapshot)
                    {
                        auto pending = TakePendingSnapshotBatchLocked();

                        if (pending)
                            activeSnapshot.emplace(ActiveSnapshotBatch{ std::move(*pending), 0, {} });
                    }
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

                if (!udpSendQueue.empty() || activeSnapshot)
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

                if (!stopRequested.load() && activeSnapshot && FD_ISSET(udpSO, &writeSet))
                {
                    FlushSnapshotBatch(*activeSnapshot);
                    if (activeSnapshot->nextPart == activeSnapshot->pending.batch.parts.size())
                        activeSnapshot.reset();
                }
            }
            };
        run();

        {
            std::lock_guard lock(sendMutex);
            stopRequested.store(true);
            sendRequestQueue.clear();
            pendingSnapshotBatch.reset();
            lastSnapshotTickBySession.clear();
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

        const auto& frame = tcpSendQueue.front();

        if (tcpSendOffset >= frame.size())
        {
            StopWithTcpError(WSAEINVAL);
            return;
        }

        const std::size_t remaining = frame.size() - tcpSendOffset;
        const char* data = reinterpret_cast<const char*>(frame.data() + tcpSendOffset);
        const int sent = send(tcpSO, data, static_cast<int>(remaining), 0);

        if (sent == SOCKET_ERROR)
        {
            const int error = WSAGetLastError();

            if (error != WSAEWOULDBLOCK)
                StopWithTcpError(error);

            return;
        }

        if (sent == 0)
        {
            StopWithTcpError(WSAEINVAL);
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
        std::array<std::uint8_t, 4096> buffer{};
        const int received = recv(tcpSO, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);

        if (received == SOCKET_ERROR)
        {
            const int error = WSAGetLastError();

            if (error != WSAEWOULDBLOCK)
                StopWithTcpError(error);

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
                StopWithTcpError(0, headerError);
                return;
            }

            if (protocol::IsUdpMessageType(header.type))
            {
                StopWithTcpError(WSAEPROTOTYPE);
                return;
            }

            appendUntil(header.totalBytes);

            if (tcpPartialFrame.size() < header.totalBytes)
                return;

            protocol::Message message{};
            const auto decodeError = protocol::CPacketCodec::Decode(tcpPartialFrame, message);

            if (decodeError != protocol::CodecError::NONE)
            {
                StopWithTcpError(0, decodeError);
                return;
            }

            if (!PushReceived(ReceivedMessage{ TransportType::TCP, peerTcpAddress, std::move(message) }))
                return;

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

    void CNetworkMgr::FlushSnapshotBatch(ActiveSnapshotBatch& active)
    {
        constexpr std::size_t MAX_PARTS_PER_ITERATION = 8;
        const auto& parts = active.pending.batch.parts;
        const auto& destination = active.pending.destination;

        for (std::size_t sentParts = 0; sentParts < MAX_PARTS_PER_ITERATION &&
            active.nextPart < parts.size() && !stopRequested.load(); ++sentParts)
        {
            if (active.encodedPart.empty())
            {
                const auto error = protocol::CPacketCodec::Encode(parts[active.nextPart], active.encodedPart);
                if (error != protocol::CodecError::NONE)
                {
                    PushEvent(NetworkEvent{ NetworkEventType::Error, destination, 0, error });
                    stopRequested.store(true);
                    return;
                }
            }

            const int size = static_cast<int>(active.encodedPart.size());
            const int sent = sendto(udpSO, reinterpret_cast<const char*>(active.encodedPart.data()),
                size, 0, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));

            if (sent != size)
            {
                const int error = sent == SOCKET_ERROR ? WSAGetLastError() : WSAEMSGSIZE;
                if (error != WSAEWOULDBLOCK)
                {
                    PushEvent(NetworkEvent{ NetworkEventType::Error, destination, error });
                    stopRequested.store(true);
                }
                return;
            }

            ++active.nextPart;
            active.encodedPart.clear();
        }
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

        PushReceived(ReceivedMessage{ TransportType::UDP, sender, std::move(message) });
    }

    bool CNetworkMgr::IsReceiveQueueFullLocked() const
    {
        return recvQueue.size() + networkEventQueue.size() >= net::MAX_RECV_QUEUE_ENTRIES;
    }

    void CNetworkMgr::PushEvent(const NetworkEvent& event)
    {
        std::lock_guard lock(receiveMutex);

        if (IsReceiveQueueFullLocked())
        {
            RequestStopOnReceiveOverflow();
            return;
        }

        networkEventQueue.push_back(event);
    }

    bool CNetworkMgr::PushReceived(ReceivedMessage message)
    {
        std::lock_guard lock(receiveMutex);

        if (IsReceiveQueueFullLocked())
        {
            RequestStopOnReceiveOverflow();
            return false;
        }

        recvQueue.push_back(std::move(message));
        return true;
    }

    void CNetworkMgr::RequestStopOnReceiveOverflow()
    {
        receiveOverflow.store(true);
        stopRequested.store(true);
    }

    NetworkResult CNetworkMgr::EnqueueSendRequest(SendRequest request)
    {
        std::lock_guard lock(sendMutex);

        if (netState.load() != NetworkState::Connected || stopRequested.load())
            return NetworkResult::NotConnected;

        const std::size_t pendingParts = pendingSnapshotBatch ? pendingSnapshotBatch->batch.parts.size() : 0;
        if (sendRequestQueue.size() + pendingParts >= MAX_SEND_REQUESTS)
            return NetworkResult::QueueFull;

        sendRequestQueue.push_back(std::move(request));

        return NetworkResult::Success;
    }

    void CNetworkMgr::StopWithTcpError(int socketError, protocol::CodecError codecError)
    {
        PushEvent(NetworkEvent{ NetworkEventType::Error, peerTcpAddress, socketError, codecError });

        stopRequested.store(true);
    }

    std::optional<CNetworkMgr::PendingSnapshotBatch> CNetworkMgr::TakePendingSnapshotBatchLocked()
    {
        return std::exchange(pendingSnapshotBatch, std::nullopt);
    }
}
