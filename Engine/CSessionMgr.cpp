#include "pch.h"
#include "CSessionMgr.h"
#include "CNetworkMgr.h"

#include <bcrypt.h>

#pragma comment(lib, "Bcrypt.lib")

namespace network
{
    CSessionMgr::CSessionMgr(CNetworkMgr& network) : network(network) {}

    NetworkResult CSessionMgr::StartHost(unsigned short tcpPort, unsigned short udpPort)
    {
        const auto result = network.StartHost(tcpPort, udpPort);
        if (result == NetworkResult::Success)
            BeginSession(NetworkRole::Host);

        return result;
    }

    NetworkResult CSessionMgr::StartClient(const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort)
    {
        const auto result = network.StartClient(hostIp, tcpPort, udpPort);

        if (result == NetworkResult::Success)
        {
            BeginSession(NetworkRole::Client);
            hostUdpPort = udpPort;
        }

        return result;
    }

    void CSessionMgr::BeginSession(NetworkRole newRole)
    {
        role = newRole;
        lastNetworkEvent = {};
        state = SessionState::Starting;
        ResetHandshake();
    }

    void CSessionMgr::Update()
    {
        NetworkEvent event{};
        while (state != SessionState::Idle && state != SessionState::Failed && state != SessionState::Disconnected && network.TryPopEvent(event))
        {
            HandleNetworkEvent(event);
        }

        switch (state)
        {
        case SessionState::AwaitingJoin:
        case SessionState::AwaitingAccept:
        case SessionState::AwaitingUdpRegister:
        case SessionState::AwaitingUdpAck:
            break;
        default:
            return;
        }

        ReceivedMessage received{};
        if (!network.TryReceive(received))
            return;

        switch (state)
        {
        case SessionState::AwaitingJoin:
            HandleJoin(received);
            break;
        case SessionState::AwaitingAccept:
            HandleJoinReply(received);
            break;
        case SessionState::AwaitingUdpRegister:
            HandleUdpRegister(received);
            break;
        case SessionState::AwaitingUdpAck:
            HandleUdpAck(received);
            break;
        default:
            break;
        }
    }

    void CSessionMgr::HandleNetworkEvent(const NetworkEvent& event)
    {
        lastNetworkEvent = event;

        switch (event.type)
        {
        case NetworkEventType::Listening:
            if (role == NetworkRole::Host && state == SessionState::Starting)
                state = SessionState::Listening;
            break;

        case NetworkEventType::Connected:
        {
            if (state != SessionState::Starting && state != SessionState::Listening)
                break;

            if (role == NetworkRole::Host)
            {
                state = SessionState::AwaitingJoin;
                break;
            }

            // 기본 헤더와 본문은 세션 발급 전의 JOIN
            const protocol::Message join{};
            const auto result = network.EnqueueSend(net::Channel::Tcp, join);
            state = result == net::SendResult::Queued ? SessionState::AwaitingAccept : SessionState::Failed;
            break;
        }

        case NetworkEventType::Disconnected:
            state = SessionState::Disconnected;
            break;

        case NetworkEventType::Error:
            state = SessionState::Failed;
            break;
        }
    }

    void CSessionMgr::HandleJoin(const ReceivedMessage& received)
    {
        const auto& message = received.message;

        if (received.transport != TransportType::TCP ||
            message.header.type != protocol::MessageType::JOIN ||
            message.header.sessionId != 0 ||
            !std::holds_alternative<protocol::JoinPayload>(message.payload))
        {
            SendReject(protocol::RejectReason::INTERNAL_ERROR);
            return;
        }

        std::uint32_t newSessionId = 0;
        protocol::UdpToken newToken = 0;

        if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&newSessionId), static_cast<ULONG>(sizeof(newSessionId)), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0 ||
            BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&newToken), static_cast<ULONG>(sizeof(newToken)), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0 ||
            newSessionId == 0 || newToken == 0)
        {
            SendReject(protocol::RejectReason::INTERNAL_ERROR);
            return;
        }

        protocol::AcceptPayload payload{};
        payload.assignedPlayerId = 1;
        payload.udpToken = newToken;

        protocol::Message reply{};
        reply.header.type = protocol::MessageType::ACCEPT;
        reply.header.sessionId = newSessionId;
        reply.payload = payload;

        if (network.EnqueueSend(net::Channel::Tcp, reply) != net::SendResult::Queued)
        {
            state = SessionState::Failed;
            return;
        }

        sessionId = newSessionId;
        udpToken = newToken;
        clientSlot = payload.assignedPlayerId;
        state = SessionState::AwaitingUdpRegister;
    }

    void CSessionMgr::HandleJoinReply(const ReceivedMessage& received)
    {
        const auto& message = received.message;

        if (received.transport != TransportType::TCP)
        {
            state = SessionState::Failed;
            return;
        }

        if (message.header.type == protocol::MessageType::REJECT)
        {
            const auto* payload = std::get_if<protocol::RejectPayload>(&message.payload);
            if (payload)
            {
                rejectReason = payload->reason;
                state = SessionState::Rejected;
                return;
            }
        }

        if (message.header.type == protocol::MessageType::ACCEPT)
        {
            const auto* payload = std::get_if<protocol::AcceptPayload>(&message.payload);
            if (payload && message.header.sessionId != 0 && payload->assignedPlayerId == 1 && payload->udpToken != 0)
            {
                sessionId = message.header.sessionId;
                udpToken = payload->udpToken;
                clientSlot = payload->assignedPlayerId;
                SendUdpRegister();
                return;
            }
        }

        state = SessionState::Failed;
    }

    void CSessionMgr::SendReject(protocol::RejectReason reason)
    {
        protocol::RejectPayload payload{};
        payload.reason = reason;

        protocol::Message reply{};
        reply.header.type = protocol::MessageType::REJECT;
        reply.payload = payload;

        if (network.EnqueueSend(net::Channel::Tcp, reply) != net::SendResult::Queued)
        {
            state = SessionState::Failed;
            return;
        }

        rejectReason = reason;
        state = SessionState::Rejected;
    }

    void CSessionMgr::ResetHandshake()
    {
        sessionId = 0;
        udpToken = 0;
        clientSlot = protocol::INVALID_PLAYER_SLOT;
        rejectReason = protocol::RejectReason::NONE;
    }

    void CSessionMgr::SendUdpRegister()
    {
        protocol::UdpRegisterPayload payload{};
        payload.playerId = clientSlot;
        payload.udpToken = udpToken;

        protocol::Message request{};
        request.header.type = protocol::MessageType::UDP_REGISTER;
        request.header.sessionId = sessionId;
        request.payload = payload;

        if (network.EnqueueSend(net::Channel::Udp, request) != net::SendResult::Queued)
        {
            state = SessionState::Failed;
            return;
        }

        state = SessionState::AwaitingUdpAck;
    }

    void CSessionMgr::HandleUdpRegister(const ReceivedMessage& received)
    {
        const auto& message = received.message;
        if (received.transport != TransportType::UDP ||
            message.header.type != protocol::MessageType::UDP_REGISTER ||
            message.header.sessionId != sessionId)
        {
            return;
        }

        const auto* payload = std::get_if<protocol::UdpRegisterPayload>(&message.payload);
        if (!payload || payload->playerId != clientSlot || payload->udpToken != udpToken)
            return;

        if (received.sender.sin_family != AF_INET ||
            received.sender.sin_port == 0 ||
            received.sender.sin_addr.s_addr != lastNetworkEvent.peer.sin_addr.s_addr)
        {
            return;
        }

        if (network.SetUdpPeer(received.sender) != NetworkResult::Success)
        {
            state = SessionState::Failed;
            return;
        }

        protocol::UdpAckPayload ackPayload{};
        ackPayload.playerId = clientSlot;
        ackPayload.registered = 1;

        protocol::Message reply{};
        reply.header.type = protocol::MessageType::UDP_ACK;
        reply.header.sessionId = sessionId;
        reply.payload = ackPayload;

        if (network.EnqueueSend(net::Channel::Udp, reply) != net::SendResult::Queued)
        {
            state = SessionState::Failed;
            return;
        }

        state = SessionState::UdpRegistered;
    }

    void CSessionMgr::HandleUdpAck(const ReceivedMessage& received)
    {
        const auto& message = received.message;
        if (received.transport != TransportType::UDP ||
            message.header.type != protocol::MessageType::UDP_ACK ||
            message.header.sessionId != sessionId)
        {
            return;
        }

        if (received.sender.sin_family != AF_INET ||
            received.sender.sin_addr.s_addr != lastNetworkEvent.peer.sin_addr.s_addr ||
            received.sender.sin_port != htons(hostUdpPort))
        {
            return;
        }

        const auto* payload = std::get_if<protocol::UdpAckPayload>(&message.payload);

        if (!payload || payload->playerId != clientSlot || payload->registered != 1)
            return;

        state = SessionState::UdpRegistered;
    }

    void CSessionMgr::Stop()
    {
        if (state == SessionState::Idle)
            return;

        network.Shutdown();
        lastNetworkEvent = {};
        state = SessionState::Idle;
        ResetHandshake();
    }
}
