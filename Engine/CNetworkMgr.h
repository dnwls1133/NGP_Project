#pragma once
#include "NetworkTypes.h"
#include "../net/SendContract.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>
#include <functional>

namespace network
{

    struct SendRequest
    {
        TransportType transport = TransportType::TCP;
        sockaddr_in destination{};
        protocol::Message message{};
    };

    struct PendingDatagram
    {
        sockaddr_in destination{};
        std::vector<std::uint8_t> bytes;
    };

    class CNetworkMgr : public net::ISendSink
    {
        SINGLE(CNetworkMgr)

    private:
        friend struct SnapshotBatchTestAccess;

        struct PendingSnapshotBatch
        {
            sockaddr_in destination{};
            net::SnapshotBatch batch;
        };

        struct ActiveSnapshotBatch
        {
            PendingSnapshotBatch pending;
            std::size_t nextPart = 0;
            std::vector<std::uint8_t> encodedPart;
        };

        bool initialized = false;

        // [메인 ↔ 통신 스레드 공유 데이터]
        std::deque<SendRequest> sendRequestQueue;                                   // 송신 요청 큐
        sockaddr_in udpPeerAddress{};                                               // sendMutex로 보호
        std::optional<PendingSnapshotBatch> pendingSnapshotBatch;                   // sendMutex로 보호
        std::unordered_map<std::uint32_t, std::uint32_t> lastSnapshotTickBySession; // 세션별 마지막 수락 틱, 너무 오래된 틱 방지
        std::deque<ReceivedMessage> recvQueue;                                       // 수신 메시지 큐
        std::deque<NetworkEvent> networkEventQueue;                                  // 네트워크 이벤트 큐

        std::mutex sendMutex;                                           // sendRequestQueue 보호용
        std::mutex receiveMutex;                                        // recvQueue, networkEventQueue 보호용

        std::atomic<bool> stopRequested{ false };                       // 중지 요청 플래그
        std::atomic<NetworkState> netState{ NetworkState::Stopped };    // 현재 상태 : 접속 대기 / 연결 중 / 연결됨 / 종료
        std::atomic<bool> receiveOverflow{ false };                     // 수신 큐 포화 발생 여부
        bool receiveOverflowReported = false;                           // receiveMutex로 보호

        std::thread transportThread;                                    // 통신 스레드

        NetworkConfig config{}; // 실행 전에만 설정, 스레드 종료 전까지 변경하면 안됨

        // [통신 스레드에서만 사용하는 데이터]
        SOCKET listenSO_tcp = INVALID_SOCKET;    // TCP 리스너 소켓
        SOCKET tcpSO = INVALID_SOCKET;           // TCP 소켓
        SOCKET udpSO = INVALID_SOCKET;           // UDP 소켓

        sockaddr_in peerTcpAddress{};
        std::deque<std::vector<std::uint8_t>> tcpSendQueue; // TCP 인코딩된 송신 큐
        std::size_t tcpSendOffset = 0;                      // TCP 송신 오프셋
        std::vector<std::uint8_t> tcpPartialFrame;          // TCP 미완성 수신 버퍼

        std::deque<PendingDatagram> udpSendQueue;           // UDP 인코딩된 송신 큐

        static constexpr std::size_t MAX_SEND_REQUESTS = net::MAX_SEND_QUEUE_ENTRIES;

    public:
        void Shutdown();

        NetworkResult StartHost(unsigned short tcpPort, unsigned short udpPort);
        NetworkResult StartClient(const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);

        NetworkResult SendTcp(const protocol::Message& message);
        NetworkResult SendUdp(const sockaddr_in& destination, const protocol::Message& message);
        NetworkResult SendUdp(const protocol::Message& message);
        net::SendResult EnqueueSend(net::Channel channel, const protocol::Message& message) override;
        net::SendResult EnqueueSnapshotBatch(const net::SnapshotBatch& batch) override;

        // Host의 메인 스레드에서 세션과 토큰을 검증한 뒤 호출
        NetworkResult SetUdpPeer(const sockaddr_in& address);

        bool TryReceive(ReceivedMessage& out);
        bool TryPopEvent(NetworkEvent& out);

    private:
        static int SendSnapshotPart(SOCKET socket, const sockaddr_in& destination, const std::vector<std::uint8_t>& bytes);

        using SnapshotSendFunction = std::function<int(SOCKET, const sockaddr_in&, const std::vector<std::uint8_t>&)>;

        std::function<int(SOCKET, const char*, int)> tcpSendOverride;
        SnapshotSendFunction snapshotSendOverride;

        NetworkResult Start(const NetworkConfig& config);

        void NetworkThread();
        void ResetReceiveStateForStart();
        void ClearPendingSendsLocked();

        void FlushTcpSend();
        void ReceiveTcp();
        void FlushUdpSend();
        void FlushSnapshotBatch(ActiveSnapshotBatch& active, const SnapshotSendFunction& sendPart = SendSnapshotPart);
        void ReceiveUdp();

        void PushEvent(const NetworkEvent& event);
        bool PushReceived(ReceivedMessage message);
        void RequestStopOnReceiveOverflow();
        bool IsReceiveQueueFullLocked() const;
        NetworkResult EnqueueSendRequest(SendRequest request);
        void StopWithTcpError(int socketError, protocol::CodecError codecError = protocol::CodecError::NONE);

        std::optional<PendingSnapshotBatch> TakePendingSnapshotBatchLocked();
    };

}
