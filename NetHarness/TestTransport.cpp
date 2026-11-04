#include "TestTransport.h"

#include <algorithm>
#include <array>
#include <utility>

#pragma comment(lib, "Ws2_32.lib")

namespace
{
    transport::IoResult SocketErrorResult()
    {
        const int error = WSAGetLastError();

        if (error == WSAEWOULDBLOCK)
            return { transport::IoStatus::WouldBlock, 0, 0 };

        return { transport::IoStatus::Error, 0, error };
    }

    transport::IoResult InvalidArgumentResult()
    {
        return { transport::IoStatus::Error, 0, WSAEINVAL };
    }

    transport::MessageResult TransportFailure(int socketError = 0, protocol::CodecError codecError = protocol::CodecError::NONE)
    {
        return { transport::IoStatus::Error, socketError, codecError };
    }

    transport::MessageResult FromIoResult(const transport::IoResult& result)
    {
        return { result.status, result.errorCode, protocol::CodecError::NONE };
    }

    std::uint32_t ReadTotalBytes(const std::vector<std::uint8_t>& header)
    {
        return
            (static_cast<std::uint32_t>(header[8]) << 24) |
            (static_cast<std::uint32_t>(header[9]) << 16) |
            (static_cast<std::uint32_t>(header[10]) << 8) |
            static_cast<std::uint32_t>(header[11]);
    }
}


namespace transport
{
    bool SetNonBlocking(SOCKET sock)
    {
        u_long enabled = 1;
        return ioctlsocket(sock, FIONBIO, &enabled) != SOCKET_ERROR;
    }

    bool WaitForSockets(SOCKET tcpSocket, SOCKET udpSocket, bool watchTcpWrite, bool watchUdpWrite, int timeoutMs, SocketReady& ready)
    {
        ready = {};

        if (timeoutMs < 0 || (tcpSocket == INVALID_SOCKET && udpSocket == INVALID_SOCKET))
        {
            WSASetLastError(WSAEINVAL);
            return false;
        }

        fd_set readSet;
        fd_set writeSet;

        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);

        if (tcpSocket != INVALID_SOCKET)
        {
            FD_SET(tcpSocket, &readSet);

            if (watchTcpWrite)
                FD_SET(tcpSocket, &writeSet);
        }

        if (udpSocket != INVALID_SOCKET)
        {
            FD_SET(udpSocket, &readSet);

            if (watchUdpWrite)
                FD_SET(udpSocket, &writeSet);
        }

        timeval timeout{};
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        const bool watchWrite = (tcpSocket != INVALID_SOCKET && watchTcpWrite) || (udpSocket != INVALID_SOCKET && watchUdpWrite);

        const int result = select(0, &readSet, watchWrite ? &writeSet : nullptr, nullptr, &timeout);

        if (result == SOCKET_ERROR)
            return false;

        if (result == 0)
            return true;

        if (tcpSocket != INVALID_SOCKET)
        {
            ready.tcpReadable = FD_ISSET(tcpSocket, &readSet) != 0;

            ready.tcpWritable = watchTcpWrite && FD_ISSET(tcpSocket, &writeSet) != 0;
        }

        if (udpSocket != INVALID_SOCKET)
        {
            ready.udpReadable = FD_ISSET(udpSocket, &readSet) != 0;

            ready.udpWritable = watchUdpWrite && FD_ISSET(udpSocket, &writeSet) != 0;
        }

        return true;
    }

    IoResult SendTcp(SOCKET sock, const char* data, int size)
    {
        if (data == nullptr || size <= 0)
            return InvalidArgumentResult();

        const int sent = send(sock, data, size, 0);

        if (sent == SOCKET_ERROR)
            return SocketErrorResult();

        if (sent == 0)
            return { IoStatus::Error, 0, 0 };

        return { IoStatus::Success, sent, 0 };
    }

    IoResult ReceiveTcp(SOCKET sock, char* buffer, int capacity)
    {
        if (buffer == nullptr || capacity <= 0)
            return InvalidArgumentResult();

        const int received = recv(sock, buffer, capacity, 0);

        if (received == SOCKET_ERROR)
            return SocketErrorResult();

        if (received == 0)
            return { IoStatus::Closed, 0, 0 };

        return { IoStatus::Success, received, 0 };
    }

    IoResult SendUdp(SOCKET sock, const sockaddr_in& destination, const char* data, int size)
    {
        if (data == nullptr || size <= 0)
            return InvalidArgumentResult();

        const int sent = sendto(sock, data, size, 0, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));

        if (sent == SOCKET_ERROR)
            return SocketErrorResult();

        if (sent != size)
            return { IoStatus::Error, sent, WSAEMSGSIZE };

        return { IoStatus::Success, sent, 0 };
    }

    IoResult ReceiveUdp(SOCKET sock, sockaddr_in& sender, char* buffer, int capacity)
    {
        if (buffer == nullptr || capacity <= 0)
            return InvalidArgumentResult();

        sockaddr_in source{};
        int addressSize = sizeof(source);

        const int received = recvfrom(sock, buffer, capacity, 0, reinterpret_cast<sockaddr*>(&source), &addressSize);

        if (received == SOCKET_ERROR)
            return SocketErrorResult();

        sender = source;

        return { IoStatus::Success, received, 0 };
    }

    void CloseSocket(SOCKET& sock)
    {
        if (sock != INVALID_SOCKET)
        {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
    }

    MessageResult QueueTcpMessage(TcpState& state, const protocol::Message& message)
    {
        if (protocol::IsUdpMessageType(message.header.type))
            return TransportFailure(WSAEPROTOTYPE);

        if (state.sendQueue.size() >= MAX_QUEUE_MESSAGES)
            return TransportFailure(WSAENOBUFS);

        std::vector<std::uint8_t> bytes;

        const auto error = protocol::CPacketCodec::Encode(message, bytes);

        if (error != protocol::CodecError::NONE)
            return TransportFailure(0, error);

        if (bytes.size() < protocol::HEADER_BYTES || bytes.size() > protocol::MAX_TCP_BYTES)
            return TransportFailure(WSAEMSGSIZE);

        state.sendQueue.push_back(std::move(bytes));
        return {};
    }

    MessageResult QueueUdpMessage(UdpState& state, const sockaddr_in& destination, const protocol::Message& message)
    {
        if (!protocol::IsUdpMessageType(message.header.type))
            return TransportFailure(WSAEPROTOTYPE);

        if (state.sendQueue.size() >= MAX_QUEUE_MESSAGES)
            return TransportFailure(WSAENOBUFS);

        std::vector<std::uint8_t> bytes;

        const auto error = protocol::CPacketCodec::Encode(message, bytes);

        if (error != protocol::CodecError::NONE)
            return TransportFailure(0, error);

        if (bytes.size() < protocol::HEADER_BYTES || bytes.size() > protocol::MAX_UDP_BYTES)
            return TransportFailure(WSAEMSGSIZE);

        state.sendQueue.push_back({ std::move(bytes), destination });

        return {};
    }
    MessageResult FlushTcpSend(SOCKET sock, TcpState& state)
    {
        if (state.sendQueue.empty())
            return {};

        const auto& frame = state.sendQueue.front();

        const auto result = SendTcp(sock, reinterpret_cast<const char*>(frame.data() + state.sendOffset), static_cast<int>(frame.size() - state.sendOffset));

        if (result.status != IoStatus::Success)
            return FromIoResult(result);

        state.sendOffset += result.bytes;

        if (state.sendOffset == frame.size())
        {
            state.sendQueue.pop_front();
            state.sendOffset = 0;
        }

        return {};
    }

    MessageResult FlushUdpSend(SOCKET sock, UdpState& state)
    {
        if (state.sendQueue.empty())
            return {};

        const auto& datagram = state.sendQueue.front();

        const auto result = SendUdp(sock, datagram.destination, reinterpret_cast<const char*>(datagram.bytes.data()), static_cast<int>(datagram.bytes.size()));

        if (result.status != IoStatus::Success)
            return FromIoResult(result);

        state.sendQueue.pop_front();
        return {};
    }

    MessageResult ReceiveTcpMessages(SOCKET sock, TcpState& state)
    {
        std::array<char, 4096> buffer{};

        const auto result = ReceiveTcp(sock, buffer.data(), static_cast<int>(buffer.size()));

        if (result.status == IoStatus::Closed)
        {
            if (!state.partialFrame.empty())
                return TransportFailure(0, protocol::CodecError::TRUNCATED);

            return FromIoResult(result);
        }

        if (result.status != IoStatus::Success)
            return FromIoResult(result);

        return FeedTcpBytes(state, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(buffer.data()), static_cast<std::size_t>(result.bytes)));
    }

    MessageResult ReceiveUdpMessage(SOCKET sock, UdpState& state)
    {
        std::array<char, protocol::MAX_UDP_BYTES + 1> buffer{};
        sockaddr_in sender{};

        const auto result = ReceiveUdp(sock, sender, buffer.data(), static_cast<int>(buffer.size()));

        if (result.status != IoStatus::Success)
            return FromIoResult(result);

        if (result.bytes < static_cast<int>(protocol::HEADER_BYTES))
            return TransportFailure(0, protocol::CodecError::TRUNCATED);


        if (result.bytes > static_cast<int>(protocol::MAX_UDP_BYTES))
            return TransportFailure(0, protocol::CodecError::TOO_LARGE);

        protocol::Message message;

        const auto error = protocol::CPacketCodec::Decode(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(buffer.data()), static_cast<std::size_t>(result.bytes)), message);

        if (error != protocol::CodecError::NONE)
            return TransportFailure(0, error);

        if (!protocol::IsUdpMessageType(message.header.type))
            return TransportFailure(WSAEPROTOTYPE);

        if (state.received.size() >= MAX_QUEUE_MESSAGES)
            return TransportFailure(WSAENOBUFS);

        state.received.push_back({ std::move(message), sender });

        return {};
    }

    MessageResult FeedTcpBytes(TcpState& state, std::span<const std::uint8_t> bytes)
    {
        while (!bytes.empty())
        {
            if (state.partialFrame.size() < protocol::HEADER_BYTES)
            {
                const std::size_t count = (std::min)(protocol::HEADER_BYTES - state.partialFrame.size(), bytes.size());

                state.partialFrame.insert(state.partialFrame.end(), bytes.begin(), bytes.begin() + count);

                bytes = bytes.subspan(count);

                if (state.partialFrame.size() < protocol::HEADER_BYTES)
                    return {};
            }

            const std::uint32_t totalBytes = ReadTotalBytes(state.partialFrame);

            if (totalBytes < protocol::HEADER_BYTES)
                return TransportFailure(0, protocol::CodecError::INVALID_LENGTH);

            if (totalBytes > protocol::MAX_TCP_BYTES)
                return TransportFailure(0, protocol::CodecError::TOO_LARGE);

            const std::size_t count = (std::min)(static_cast<std::size_t>(totalBytes) - state.partialFrame.size(), bytes.size());

            state.partialFrame.insert(state.partialFrame.end(), bytes.begin(), bytes.begin() + count);

            bytes = bytes.subspan(count);

            if (state.partialFrame.size() < totalBytes)
                return {};

            protocol::Message message;

            const auto error = protocol::CPacketCodec::Decode(std::span<const std::uint8_t>(state.partialFrame.data(), state.partialFrame.size()), message);

            if (error != protocol::CodecError::NONE)
                return TransportFailure(0, error);

            if (protocol::IsUdpMessageType(message.header.type))
                return TransportFailure(WSAEPROTOTYPE);

            if (state.received.size() >= MAX_QUEUE_MESSAGES)
                return TransportFailure(WSAENOBUFS);

            state.received.push_back(std::move(message));
            state.partialFrame.clear();
        }

        return {};
    }
}
