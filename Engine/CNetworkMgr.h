#pragma once
#include "NetworkTypes.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>


//송신:
//protocol::Message
//    → Encode
//    → 인코딩된 바이트를 송신 큐에 등록
//    → TCP 또는 UDP 송신
//
//TCP 수신:
//바이트 누적
//    → 32바이트 확보
//    → DecodeHeader로 헤더 검증·전체 길이 확인
//    → 전체 길이만큼 누적
//    → Decode
//    → 수신 큐에 등록
//
//UDP 수신:
//데이터그램 하나 수신
//    → Decode
//    → 송신자 주소와 함께 수신 큐에 등록

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

    class CNetworkMgr
    {
        SINGLE(CNetworkMgr)

    private:
        bool initialized = false;

        // [메인 ↔ 통신 스레드 공유 데이터]
        std::deque<SendRequest> sendRequestQueue;       // 송신 요청 큐
        std::deque<ReceivedMessage> recvQueue;          // 수신 메시지 큐
        std::deque<NetworkEvent> networkEventQueue;     // 네트워크 이벤트 큐

        std::mutex queueMutex;                                          // 각 공유 큐를 보호할 mutex
        std::atomic<bool> stopRequested{ false };                       // 중지 요청 플래그
        std::atomic<NetworkState> netState{ NetworkState::Stopped };    // 현재 상태 : 접속 대기 / 연결 중 / 연결됨 / 종료
        HANDLE transportThread = nullptr;                               // 통신 스레드

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

        static constexpr std::size_t MAX_SEND_REQUESTS = 1024;

    public:
        void Shutdown();

        NetworkResult StartHost(unsigned short tcpPort, unsigned short udpPort);
        NetworkResult StartClient(const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);

        NetworkResult SendTcp(const protocol::Message& message);
        NetworkResult SendUdp(const sockaddr_in& destination, const protocol::Message& message);

        bool TryReceive(ReceivedMessage& out);
        bool TryPopEvent(NetworkEvent& out);

    private:
        NetworkResult Start(const NetworkConfig& config);

        static DWORD WINAPI ThreadEntry(LPVOID parameter);

        void NetworkThread();

        void FlushTcpSend();
        void ReceiveTcp();
        void FlushUdpSend();
        void ReceiveUdp();

        void PushEvent(const NetworkEvent& event);
    };

}
