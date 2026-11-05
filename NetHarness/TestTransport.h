#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>
#include <functional>

#include "../shared/protocol/PacketCodec.h"
// #include "PacketCodec.h"

namespace transport
{
    enum class IoStatus
    {
        Success,
        WouldBlock,
        Closed,
        Error
    };

    struct IoResult
    {
        IoStatus status;
        int bytes;
        int errorCode;
    };

    struct SocketReady
    {
        bool tcpReadable = false;
        bool tcpWritable = false;
        bool udpReadable = false;
        bool udpWritable = false;
    };

    struct MessageResult
    {
        IoStatus status = IoStatus::Success;
        int socketError = 0;
        protocol::CodecError codecError = protocol::CodecError::NONE;
    };

    inline constexpr std::size_t MAX_QUEUE_MESSAGES = 1024;

    struct TcpState
    {
        std::vector<std::uint8_t> partialFrame;

        std::deque<std::vector<std::uint8_t>> sendQueue;

        std::size_t sendOffset = 0;

        std::deque<protocol::Message> received;
    };

    struct PendingDatagram
    {
        std::vector<std::uint8_t> bytes;
        sockaddr_in destination{};
    };

    struct ReceivedDatagram
    {
        protocol::Message message;
        sockaddr_in sender{};
    };

    struct UdpState
    {
        std::deque<PendingDatagram> sendQueue;
        std::deque<ReceivedDatagram> received;
    };

    using TcpSendFunction = std::function<IoResult(SOCKET, const char*, int)>;

    bool SetNonBlocking(SOCKET sock);

    // 소켓을 비블로킹으로 설정하고 제한 시간 내 TCP 연결을 시도
    // 소켓 생성과 종료는 호출자가 담당
    IoResult ConnectTcp(SOCKET sock, const sockaddr_in& destination, int timeoutMs = 5000);

    bool WaitForSockets(SOCKET tcpSocket, SOCKET udpSocket, bool watchTcpWrite, bool watchUdpWrite, int timeoutMs, SocketReady& ready);

    IoResult SendTcp(SOCKET sock, const char* data, int size);
    IoResult ReceiveTcp(SOCKET sock, char* buffer, int capacity);

    IoResult SendUdp(SOCKET sock, const sockaddr_in& destination, const char* data, int size);
    IoResult ReceiveUdp(SOCKET sock, sockaddr_in& sender, char* buffer, int capacity);

    void CloseSocket(SOCKET& sock);

    // Message를 인코딩해서 송신 대기열에 넣기
    MessageResult QueueTcpMessage(TcpState& state, const protocol::Message& message);
    MessageResult QueueUdpMessage(UdpState& state, const sockaddr_in& destination, const protocol::Message& message);

    // 쓰기 가능할 때 호출
    MessageResult FlushTcpSend(SOCKET sock, TcpState& state, const TcpSendFunction& sendFunction = SendTcp);
    MessageResult FlushUdpSend(SOCKET sock, UdpState& state);

    // 읽기 가능할 때 호출
    MessageResult ReceiveTcpMessages(SOCKET sock, TcpState& state);
    MessageResult ReceiveUdpMessage(SOCKET sock, UdpState& state);

    // TCP 바이트 조각에서 완성된 메시지를 추출
    MessageResult FeedTcpBytes(TcpState& state, std::span<const std::uint8_t> bytes);
}
