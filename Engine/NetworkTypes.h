#pragma once
#include "../shared/protocol/ProtocolTypes.h"

#include <WinSock2.h>
#include <string>
#include <cstdint>

namespace network
{
    enum class NetworkRole
    {
        Host,
        Client
    };

    enum class NetworkState
    {
        Stopped,        // 중지
        Starting,       // 시작 준비 중
        Listening,      // 연결 대기
        Connecting,     // 연결 중
        Connected       // 연결 됨
    };

    enum class TransportType
    {
        TCP = IPPROTO_TCP,
        UDP = IPPROTO_UDP
    };

    struct NetworkConfig
    {
        NetworkRole role = NetworkRole::Client;

        std::string hostIp = "127.0.0.1";
        std::uint16_t tcpPort = 40000;
        std::uint16_t udpPort = 40001;

        int connectTimeoutMs = 5000;
    };

    struct ReceivedMessage
    {
        TransportType transport = TransportType::TCP;
        sockaddr_in sender{};
        protocol::Message message{};
    };

    enum class NetworkEventType
    {
        Listening,
        Connected,
        Disconnected,
        Error
    };

    struct NetworkEvent
    {
        NetworkEventType type = NetworkEventType::Error;
        sockaddr_in peer{};
        int socketError = 0;
        protocol::CodecError codecError = protocol::CodecError::NONE;
    };

    enum class NetworkResult
    {
        Success,
        NotInitialized,
        AlreadyRunning,
        NotConnected,
        InvalidArgument,
        WrongTransport,
        StartupFailed,
        QueueFull,
        UdpPeerNotSet
    };
}
