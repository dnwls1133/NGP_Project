#include "pch.h"
#include "CSessionMgr.h"
#include "CNetworkMgr.h"

#include <bcrypt.h>
#include <iostream>
#include <Ws2tcpip.h>
#include <charconv>
#include <string_view>

#pragma comment(lib, "Bcrypt.lib")

namespace network
{
    CSessionMgr::CSessionMgr(CNetworkMgr& network) : network(network) {}

    NetworkResult CSessionMgr::StartHost(unsigned short tcpPort, unsigned short udpPort)
    {
        std::string selectedIp;
        if (!network.FindLocalIpv4(selectedIp))
        {
            std::cerr << "[HOST] No usable local IPv4 address\n";
            return NetworkResult::AddressUnavailable;
        }

#if _DEBUG
        std::cout << selectedIp << std::endl;
#endif

        const auto result = network.StartHost(tcpPort, udpPort);
        if (result == NetworkResult::Success)
        {
            BeginSession(NetworkRole::Host);
            hostIp = std::move(selectedIp);
            this->tcpPort = tcpPort;
            hostUdpPort = udpPort;
        }

        return result;
    }

    NetworkResult CSessionMgr::StartClient(const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort)
    {
        std::cout << "[CLIENT] Join Request. IP=" << hostIp << ", TCP=40000, UDP=40001\n";

        const auto result = network.StartClient(hostIp, tcpPort, udpPort);

        if (result == NetworkResult::Success)
        {
            BeginSession(NetworkRole::Client);
            this->hostIp = hostIp;
            this->tcpPort = tcpPort;
            hostUdpPort = udpPort;
        }

        return result;
    }

    bool CSessionMgr::SetRole(network::NetworkRole newRole)
    {
        if (state != SessionState::Idle)
            return false;

        if (newRole != NetworkRole::Host && newRole != NetworkRole::Client)
            return false;

        role = newRole;
        return true;
    }

    net::SendResult CSessionMgr::RequestReady(bool ready)
    {
        if (role != NetworkRole::Client || state != SessionState::UdpRegistered)
            return net::SendResult::InvalidRequest;

        protocol::ReadyPayload payload{};
        payload.playerId = clientSlot;
        payload.isReady = ready ? 1 : 0;

        protocol::Message request{};
        request.header.type = protocol::MessageType::READY;
        request.header.sessionId = sessionId;
        request.payload = payload;

        return network.EnqueueSend(net::Channel::Tcp, request);
    }

    net::SendResult CSessionMgr::RequestLoad()
    {
        if (role != NetworkRole::Host || state != SessionState::UdpRegistered || !clientReady)
            return net::SendResult::InvalidRequest;

        protocol::Message message{};
        message.header.type = protocol::MessageType::LOAD;
        message.header.sessionId = sessionId;
        message.payload = protocol::LoadPayload{};

        const auto result = network.EnqueueSend(net::Channel::Tcp, message);
        if (result == net::SendResult::Queued)
            state = SessionState::Loading;

        return result;
    }

    bool CSessionMgr::NotifyLocalLoaded()
    {
        if (state != SessionState::Loading)
            return false;

        if (localLoaded)
            return true;

        if (role == NetworkRole::Client)
        {
            protocol::LoadedPayload payload{};
            payload.playerId = clientSlot;

            protocol::Message message{};
            message.header.type = protocol::MessageType::LOADED;
            message.header.sessionId = sessionId;
            message.payload = payload;

            if (network.EnqueueSend(net::Channel::Tcp, message) != net::SendResult::Queued)
                return false;
        }

        localLoaded = true;
        return true;
    }

    net::SendResult CSessionMgr::RequestStart(std::uint32_t startHostTick)
    {
        if (role != NetworkRole::Host || state != SessionState::Loading ||
            !localLoaded || !clientLoaded)
        {
            return net::SendResult::InvalidRequest;
        }

        protocol::StartPayload payload{};
        payload.startHostTick = startHostTick;

        protocol::Message message{};
        message.header.type = protocol::MessageType::START;
        message.header.sessionId = sessionId;
        message.payload = payload;

        const auto result = network.EnqueueSend(net::Channel::Tcp, message);
        if (result == net::SendResult::Queued)
        {
            startInfo = payload;
            state = SessionState::Started;
        }

        return result;
    }

    bool CSessionMgr::EncodeRoomCode(const std::string& ip, std::string& outCode)
    {
        in_addr address{};

        // 문자열 중간의 널 문자로 뒷부분이 무시되는 입력도 거부
        if (ip.find('\0') != std::string::npos || inet_pton(AF_INET, ip.c_str(), &address) != 1)
        {
            outCode.clear();
            return false;
        }

        const std::uint32_t value = ntohl(address.s_addr);
        constexpr std::string_view HEX = "0123456789ABCDEF";

        std::string result;
        result.reserve(9);

        for (int digit = 0; digit < 8; ++digit)
        {
            if (digit == 4)
                result.push_back('-');

            const int shift = (7 - digit) * 4;
            result.push_back(HEX[(value >> shift) & 0xFu]);
        }

        outCode = std::move(result);
        return true;
    }

    bool CSessionMgr::DecodeRoomCode(const std::string& code, std::string& outIp)
    {
        std::string digits;

        if (code.size() == 8)
        {
            digits = code;
        }
        else if (code.size() == 9 && code[4] == '-')
        {
            digits = code.substr(0, 4) + code.substr(5);
        }
        else
        {
            outIp.clear();
            return false;
        }

        // from_chars가 허용할 수 있는 부호 등의 입력도 명시적으로 거부
        if (digits.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
        {
            outIp.clear();
            return false;
        }

        std::uint32_t value = 0;
        const char* begin = digits.data();
        const char* end = begin + digits.size();
        const auto parsed = std::from_chars(begin, end, value, 16);

        if (parsed.ec != std::errc{} || parsed.ptr != end)
        {
            outIp.clear();
            return false;
        }

        std::string result = std::to_string((value >> 24) & 0xFFu);
        result += "." + std::to_string((value >> 16) & 0xFFu);
        result += "." + std::to_string((value >> 8) & 0xFFu);
        result += "." + std::to_string(value & 0xFFu);

        outIp = std::move(result);
        return true;
        return false;
    }

    std::string CSessionMgr::GetRoomCode() const
    {
        if (!IsHost() || state == SessionState::Idle)
            return {};

        std::string code;
        EncodeRoomCode(hostIp, code);
        return code;
    }

    NetworkResult CSessionMgr::JoinRoom(const std::string& code, unsigned short tcpPort, unsigned short udpPort)
    {
        if (state != SessionState::Idle)
            return NetworkResult::AlreadyRunning;

        std::string decodedIp;
        if (!DecodeRoomCode(code, decodedIp))
            return NetworkResult::InvalidArgument;

        return StartClient(decodedIp, tcpPort, udpPort);
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
        case SessionState::UdpRegistered:
        case SessionState::Loading:
            break;
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
        case SessionState::UdpRegistered:
            if (role == NetworkRole::Host)
                HandleReady(received);
            else if (received.message.header.type == protocol::MessageType::LOAD)
                HandleLoad(received);
            else
                HandleRoomState(received);
            break;
        case SessionState::Loading:
            HandleLoadingMessage(received);
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
        localLoaded = false;
        clientLoaded = false;
        clientReady = false;
        startInfo = {};
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

    void CSessionMgr::HandleReady(const ReceivedMessage& received)
    {
        const auto& message = received.message;
        if (received.transport != TransportType::TCP ||
            message.header.type != protocol::MessageType::READY ||
            message.header.sessionId != sessionId)
        {
            return;
        }

        const auto* ready = std::get_if<protocol::ReadyPayload>(&message.payload);
        if (!ready || ready->playerId != clientSlot || ready->isReady > 1)
            return;

        protocol::RoomStatePayload room{};
        room.slots[0] = protocol::RoomSlotState{ 0, 1, 1, 0 };
        room.slots[clientSlot] = protocol::RoomSlotState{ clientSlot, 1, ready->isReady, 1 };

        protocol::Message reply{};
        reply.header.type = protocol::MessageType::ROOM_STATE;
        reply.header.sessionId = sessionId;
        reply.payload = room;

        if (network.EnqueueSend(net::Channel::Tcp, reply) != net::SendResult::Queued)
        {
            state = SessionState::Failed;
            return;
        }

        clientReady = ready->isReady == 1;
    }

    void CSessionMgr::HandleRoomState(const ReceivedMessage& received)
    {
        const auto& message = received.message;
        if (received.transport != TransportType::TCP ||
            message.header.type != protocol::MessageType::ROOM_STATE ||
            message.header.sessionId != sessionId)
        {
            return;
        }

        const auto* room = std::get_if<protocol::RoomStatePayload>(&message.payload);
        if (!room || clientSlot >= room->slots.size())
            return;

        const auto& slot = room->slots[clientSlot];
        if (slot.playerId != clientSlot || slot.connected != 1 || slot.udpRegistered != 1)
            return;

        clientReady = slot.ready == 1;
    }

    void CSessionMgr::HandleLoad(const ReceivedMessage& received)
    {
        const auto& message = received.message;
        if (received.transport != TransportType::TCP ||
            message.header.type != protocol::MessageType::LOAD ||
            message.header.sessionId != sessionId ||
            !clientReady)
        {
            return;
        }

        const auto* payload = std::get_if<protocol::LoadPayload>(&message.payload);
        if (payload && payload->stageId == 1)
            state = SessionState::Loading;
    }

    void CSessionMgr::HandleLoadingMessage(const ReceivedMessage& received)
    {
        const auto& message = received.message;
        if (received.transport != TransportType::TCP || message.header.sessionId != sessionId)
            return;

        if (role == NetworkRole::Host)
        {
            if (message.header.type != protocol::MessageType::LOADED)
                return;

            const auto* payload = std::get_if<protocol::LoadedPayload>(&message.payload);
            if (payload && payload->playerId == clientSlot)
                clientLoaded = true;

            return;
        }

        if (message.header.type != protocol::MessageType::START || !localLoaded)
            return;

        const auto* payload = std::get_if<protocol::StartPayload>(&message.payload);
        if (payload)
        {
            startInfo = *payload;
            state = SessionState::Started;
        }
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
