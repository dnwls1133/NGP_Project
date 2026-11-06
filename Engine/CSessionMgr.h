#pragma once

#include "NetworkTypes.h"
#include "../net/SendContract.h"

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

        net::SendResult RequestReady(bool ready);
        bool IsClientReady() const { return state == SessionState::UdpRegistered && clientReady; }

        net::SendResult RequestLoad();
        bool NotifyLocalLoaded();

        net::SendResult RequestStart(std::uint32_t startHostTick);

        bool HasClientLoaded() const { return clientLoaded; }
        const protocol::StartPayload& GetStartInfo() const { return startInfo; }

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
        void HandleReady(const ReceivedMessage& received);
        void HandleRoomState(const ReceivedMessage& received);
        void HandleLoad(const ReceivedMessage& received);
        void HandleLoadingMessage(const ReceivedMessage& received);

        bool clientReady = false;

        bool localLoaded = false;
        bool clientLoaded = false;
        protocol::StartPayload startInfo{};

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
