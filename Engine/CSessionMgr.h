#pragma once

#include "NetworkTypes.h"
#include "../net/SendContract.h"

#include <deque>
#include <span>
#include <unordered_set>

namespace network
{
    class CNetworkMgr;

    enum class SessionState
    {
        Idle,
        Starting,
        Listening,
        AwaitingJoin,
        AwaitingAccept,
        Disconnected,
        AwaitingUdpAck,
        UdpRegistered,
        AwaitingUdpRegister, // Host가 ACCEPT를 큐에 넣고 UDP 등록 대기
        Loading,
        Started,
        Rejected,
        Failed
    };

    // 메인 스레드에서 네트워크 이벤트를 소비하고 세션 상태를 관리한다.
    class CSessionMgr
    {
    public:
        explicit CSessionMgr(CNetworkMgr& network);

        CSessionMgr(const CSessionMgr&) = delete;
        CSessionMgr& operator=(const CSessionMgr&) = delete;

        NetworkResult StartHost(unsigned short tcpPort, unsigned short udpPort);
        NetworkResult StartClient(const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);

        void Update();
        void Stop();

        SessionState GetState() const { return state; }
        const NetworkEvent& GetLastNetworkEvent() const { return lastNetworkEvent; }
        protocol::RejectReason GetRejectReason() const { return rejectReason; }

        bool SetRole(network::NetworkRole newRole);
        network::NetworkRole GetRole() const { return role; }

        net::SendResult RequestReady(bool ready);
        bool IsClientReady() const { return state == SessionState::UdpRegistered && clientReady; }

        net::SendResult RequestLoad();
        bool NotifyLocalLoaded();

        net::SendResult RequestStart(std::uint32_t startHostTick);

        bool HasClientLoaded() const { return clientLoaded; }
        const protocol::StartPayload& GetStartInfo() const { return startInfo; }

        bool IsHost() const { return role == NetworkRole::Host; }

        const std::string& GetHostIp() const { return hostIp; }
        unsigned short GetTcpPort() const { return tcpPort; }
        unsigned short GetUdpPort() const { return hostUdpPort; }

        static bool EncodeRoomCode(const std::string& ip, std::string& outCode);
        static bool DecodeRoomCode(const std::string& code, std::string& outIp);

        std::string GetRoomCode() const;
        NetworkResult JoinRoom(const std::string& code, unsigned short tcpPort, unsigned short udpPort);

        std::uint32_t GetSessionId() const { return sessionId; }
        protocol::PlayerSlot GetLocalPlayerId() const;

        net::SendResult EnqueueSend(net::Channel channel, const protocol::Message& message);
        net::SendResult EnqueueSnapshotBatch(const net::SnapshotBatch& batch);

        net::SendResult SendPlayerInput(std::uint8_t moveBits, std::uint8_t fireHeld);
        net::SendResult SendSpawn(const protocol::SpawnPayload& spawn, std::uint32_t hostTick);
        net::SendResult SendSnapshot(std::span<const protocol::ObjectStateRecord> states, std::uint32_t hostTick);

        bool TryReceiveGame(network::ReceivedMessage& out);
        bool TryReceivePlayerInput(protocol::PlayerInputPayload& outInput, std::uint64_t& outReceivedTimeMs);
        bool TryReceiveSpawn(protocol::SpawnPayload& out, protocol::PacketHeader& outHeader);
        bool TryReceiveDespawn(protocol::DespawnPayload& out, protocol::PacketHeader& outHeader);
        bool TryReceiveCombatEvent(protocol::CombatEventPayload& out, protocol::PacketHeader& outHeader);
        bool TryReceiveSnapshot(protocol::StateSnapshotPayload& out, protocol::PacketHeader& outHeader);
        bool TryReceiveResult(protocol::ResultPayload& out, protocol::PacketHeader& outHeader);

    private:
        void ResetHandshake();
        void BeginSession(NetworkRole newRole);

        void HandleNetworkEvent(const NetworkEvent& event);
        void HandleJoin(const ReceivedMessage& received);
        void HandleJoinReply(const ReceivedMessage& received);
        void HandleUdpRegister(const ReceivedMessage& received);
        void HandleUdpAck(const ReceivedMessage& received);
        void HandleReady(const ReceivedMessage& received);
        void HandleRoomState(const ReceivedMessage& received);
        void HandleLoad(const ReceivedMessage& received);
        void HandleLoadingMessage(const ReceivedMessage& received);
        void HandleGameMessage(ReceivedMessage received);
        void HandleReceivedMessage(ReceivedMessage received);

        bool IsGameMessageAllowed(NetworkRole senderRole, TransportType transport, const protocol::Message& message) const;

        void SendReject(protocol::RejectReason reason);
        void SendUdpRegister();

        bool CanProcessReceivedMessage() const;

        void EnterStarted();
        void UpdateHeartbeat();

        template<typename Payload>
        bool TryReceiveGamePayload(Payload& out, protocol::PacketHeader* outHeader, std::uint64_t* outReceivedTimeMs);

        std::deque<ReceivedMessage> gameReceiveQueue;
        std::unordered_set<protocol::EventId> acceptedEventIds;

        sockaddr_in peerTcpAddress{};
        sockaddr_in peerUdpAddress{};

        bool clientReady = false;

        bool localLoaded = false;
        bool clientLoaded = false;

        protocol::StartPayload startInfo{};

        CNetworkMgr& network;
        NetworkRole role = NetworkRole::Client;
        SessionState state = SessionState::Idle;
        NetworkEvent lastNetworkEvent{};

        std::string hostIp;
        unsigned short tcpPort = 0;
        unsigned short hostUdpPort = 0;

        std::uint32_t sessionId = 0;
        protocol::UdpToken udpToken = 0;
        protocol::PlayerSlot clientSlot = protocol::INVALID_PLAYER_SLOT;
        protocol::RejectReason rejectReason = protocol::RejectReason::NONE;

        std::uint32_t playerInputSequence = 0;

        std::uint64_t lastValidReceiveMs = 0;
        std::uint64_t lastHeartbeatQueuedMs = 0;
    };
}
