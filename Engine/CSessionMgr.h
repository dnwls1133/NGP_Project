#pragma once

#include "NetworkTypes.h"

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
        NetworkResult StartClient(const std::string& hostIp,
            unsigned short tcpPort, unsigned short udpPort);

        void Update();
        void Stop();

        SessionState GetState() const { return state; }
        const NetworkEvent& GetLastNetworkEvent() const { return lastNetworkEvent; }
        protocol::RejectReason GetRejectReason() const { return rejectReason; }

    private:
        void BeginSession(NetworkRole newRole);
        void HandleNetworkEvent(const NetworkEvent& event);
        void HandleJoin(const ReceivedMessage& received);
        void HandleJoinReply(const ReceivedMessage& received);
        void SendReject(protocol::RejectReason reason);
        void ResetHandshake();
        void SendUdpRegister();
        void HandleUdpRegister(const ReceivedMessage& received);
        void HandleUdpAck(const ReceivedMessage& received);

        CNetworkMgr& network;
        NetworkRole role = NetworkRole::Client;
        SessionState state = SessionState::Idle;
        NetworkEvent lastNetworkEvent{};

        unsigned short hostUdpPort = 0;

        std::uint32_t sessionId = 0;
        protocol::UdpToken udpToken = 0;
        protocol::PlayerSlot clientSlot = protocol::INVALID_PLAYER_SLOT;
        protocol::RejectReason rejectReason = protocol::RejectReason::NONE;
    };
}
