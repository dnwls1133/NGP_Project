#include "NetworkMgrTest.h"

#include "../Engine/define.h"
#include "../Engine/CNetworkMgr.h"
#include "../Engine/CSessionMgr.h"
#include "../net/SendContract.h"
#include "TcpTestSupport.h"

#include <ws2tcpip.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

namespace network
{
    struct SnapshotBatchTestAccess
    {
        template<typename Test>
        static bool WithConnectedHost(Test&& test)
        {
            CNetworkMgr mgr;
            return PrepareConnectedHost(mgr) && std::forward<Test>(test)(mgr);
        }

        static bool PrepareConnectedHost(CNetworkMgr& mgr)
        {
            mgr.config.role = NetworkRole::Host;
            mgr.ResetReceiveStateForStart();
            mgr.stopRequested.store(false);
            mgr.netState.store(NetworkState::Connected);

            sockaddr_in peer{};
            peer.sin_family = AF_INET;
            peer.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            peer.sin_port = htons(40001);

            return mgr.SetUdpPeer(peer) == NetworkResult::Success;
        }

        static void RecordReceiveOverflow(CNetworkMgr& mgr)
        {
            mgr.RequestStopOnReceiveOverflow();
        }

        static std::optional<net::SnapshotBatch> PeekPending(CNetworkMgr& mgr)
        {
            std::lock_guard lock(mgr.sendMutex);

            if (!mgr.pendingSnapshotBatch)
                return std::nullopt;

            return mgr.pendingSnapshotBatch->batch;
        }

        static std::vector<SendRequest> PeekSendRequests(CNetworkMgr& mgr)
        {
            std::lock_guard lock(mgr.sendMutex);
            return { mgr.sendRequestQueue.begin(), mgr.sendRequestQueue.end() };
        }

        static std::optional<net::SnapshotBatch> TakePending(CNetworkMgr& mgr)
        {
            std::lock_guard lock(mgr.sendMutex);
            auto pending = mgr.TakePendingSnapshotBatchLocked();

            if (!pending)
                return std::nullopt;

            return std::move(pending->batch);
        }

        static bool CheckWouldBlockRetry(CNetworkMgr& mgr, const net::SnapshotBatch& batch)
        {
            if (batch.parts.size() != 2 || mgr.EnqueueSnapshotBatch(batch) != net::SendResult::Queued)
                return false;

            std::optional<CNetworkMgr::PendingSnapshotBatch> pending;
            {
                std::lock_guard lock(mgr.sendMutex);
                pending = mgr.TakePendingSnapshotBatchLocked();
            }

            if (!pending)
                return false;

            CNetworkMgr::ActiveSnapshotBatch active{ std::move(*pending), 0, {} };
            std::vector<std::uint8_t> attempts;
            std::vector<std::uint8_t> sentParts;
            std::vector<std::uint8_t> blockedBytes;

            const auto sendPart = [&](SOCKET, const sockaddr_in&, const std::vector<std::uint8_t>& bytes) -> int {
                protocol::Message decoded{};
                if (protocol::CPacketCodec::Decode(bytes, decoded) != protocol::CodecError::NONE)
                    return WSAEINVAL;

                const auto* part = std::get_if<protocol::StateSnapshotPayload>(&decoded.payload);

                if (!part || part->partIndex >= batch.parts.size() || decoded != batch.parts[part->partIndex])
                    return WSAEINVAL;

                attempts.push_back(part->partIndex);

                if (part->partIndex == 1)
                {
                    if (blockedBytes.empty())
                    {
                        blockedBytes = bytes;
                        return WSAEWOULDBLOCK;
                    }

                    if (bytes != blockedBytes)
                        return WSAEINVAL;
                }

                sentParts.push_back(part->partIndex);
                return 0;
                };

            mgr.FlushSnapshotBatch(active, sendPart);

            if (active.nextPart != 1 || blockedBytes.empty() ||
                active.encodedPart != blockedBytes || mgr.stopRequested.load() ||
                attempts != std::vector<std::uint8_t>{ 0, 1 } ||
                sentParts != std::vector<std::uint8_t>{ 0 })
            {
                return false;
            }

            mgr.FlushSnapshotBatch(active, sendPart);

            NetworkEvent event{};

            return active.nextPart == batch.parts.size() && active.encodedPart.empty()
                && attempts == std::vector<std::uint8_t>{ 0, 1, 1 }
            && sentParts == std::vector<std::uint8_t>{ 0, 1 }
            && !mgr.stopRequested.load() && !mgr.TryPopEvent(event);
        }

        static void InstallStopTestSender(CNetworkMgr& mgr, bool udp, std::atomic<bool>& paused)
        {
            if (udp)
            {
                mgr.snapshotSendOverride = [&paused, firstSent = false](SOCKET sock, const sockaddr_in& destination, const std::vector<std::uint8_t>& bytes) mutable -> int {
                    if (!firstSent)
                    {
                        const int error = CNetworkMgr::SendSnapshotPart(sock, destination, bytes);
                        firstSent = error == 0;
                        return error;
                    }

                    paused.store(true);
                    return WSAEWOULDBLOCK;
                    };
                return;
            }

            mgr.tcpSendOverride = [&mgr, &paused](SOCKET sock, const char* data, int size) -> int {
                if (mgr.tcpSendOffset == 0)
                    return send(sock, data, (std::min)(size, 10), 0);

                paused.store(true);
                WSASetLastError(WSAEWOULDBLOCK);
                return SOCKET_ERROR;
                };
        }

        static void ClearStopTestSender(CNetworkMgr& mgr)
        {
            mgr.tcpSendOverride = {};
            mgr.snapshotSendOverride = {};
        }
    };
}

namespace network_mgr_test
{
    namespace
    {
        using namespace std::chrono_literals;
        using network::CNetworkMgr;
        using network::NetworkEvent;
        using network::NetworkEventType;
        using network::NetworkResult;
        using network::ReceivedMessage;
        using network::TransportType;
        using protocol::Message;
        using protocol::MessageType;

        enum class PollResult { Pending, Passed, Failed };

        constexpr std::uint32_t QUEUE_TEST_BATCH_SIZE = 64;
        constexpr std::uint32_t QUEUE_TEST_BATCH_COUNT = 32;
        constexpr std::uint32_t QUEUE_TEST_MESSAGE_COUNT = QUEUE_TEST_BATCH_SIZE * QUEUE_TEST_BATCH_COUNT;

        template<typename Poll>
        bool WaitFor(const char* step, std::chrono::milliseconds timeout, Poll poll)
        {
            const auto deadline = std::chrono::steady_clock::now() + timeout;

            while (std::chrono::steady_clock::now() < deadline)
            {
                const auto result = poll();

                if (result == PollResult::Passed)
                {
                    std::cout << "[PASS] " << step << '\n';
                    return true;
                }

                if (result == PollResult::Failed)
                {
                    std::cerr << "[FAIL] " << step << '\n';
                    return false;
                }

                std::this_thread::sleep_for(5ms);
            }

            std::cerr << "[FAIL] " << step << " timeout\n";
            return false;
        }

        PollResult UnexpectedEvent(const NetworkEvent& event)
        {
            std::cerr << "  event=" << static_cast<int>(event.type)
                << ", socketError=" << event.socketError
                << ", codecError=" << static_cast<int>(event.codecError) << '\n';

            return PollResult::Failed;
        }

        bool ExpectNoUdpAck(CNetworkMgr& mgr)
        {
            const auto deadline = std::chrono::steady_clock::now() + 1s;

            while (std::chrono::steady_clock::now() < deadline)
            {
                NetworkEvent event{};
                if (mgr.TryPopEvent(event))
                {
                    UnexpectedEvent(event);
                    return false;
                }

                ReceivedMessage received{};
                if (mgr.TryReceive(received) && received.message.header.type == MessageType::UDP_ACK)
                {
                    std::cerr << "[FAIL] Host replied to wrong-token UDP_REGISTER\n";
                    return false;
                }

                std::this_thread::sleep_for(5ms);
            }

            std::cout << "[PASS] No UDP_ACK for wrong token\n";
            return true;
        }

        bool WaitEvent(CNetworkMgr& mgr, NetworkEventType expected, NetworkEvent& out, const char* step, std::chrono::milliseconds timeout = 5s)
        {
            return WaitFor(step, timeout, [&]
                {
                    if (!mgr.TryPopEvent(out))
                        return PollResult::Pending;

                    if (out.type != expected)
                        return UnexpectedEvent(out);

                    return PollResult::Passed;
                });
        }

        bool WaitForSessionState(network::CSessionMgr& session, network::SessionState expected, const char* step, std::chrono::milliseconds timeout)
        {
            using network::SessionState;

            return WaitFor(step, timeout, [&session, expected]
                {
                    session.Update();
                    const auto state = session.GetState();

                    if (state == expected)
                        return PollResult::Passed;

                    if (state == SessionState::Failed || state == SessionState::Rejected || state == SessionState::Disconnected)
                    {
                        const auto& event = session.GetLastNetworkEvent();
                        std::cerr << "  sessionState=" << static_cast<int>(state) << ", socketError=" << event.socketError << ", codecError=" << static_cast<int>(event.codecError) << '\n';

                        return PollResult::Failed;
                    }

                    return PollResult::Pending;
                });
        }

        bool Receive(CNetworkMgr& mgr, TransportType transport, const Message& expected, const sockaddr_in& expectedSender, bool checkPort, const char* step, ReceivedMessage& out)
        {
            return WaitFor(step, 5s, [&]
                {
                    NetworkEvent event{};

                    if (mgr.TryPopEvent(event))
                        return UnexpectedEvent(event);

                    if (!mgr.TryReceive(out))
                        return PollResult::Pending;

                    const bool addressMatches =
                        out.sender.sin_family == AF_INET &&
                        out.sender.sin_addr.s_addr == expectedSender.sin_addr.s_addr &&
                        out.sender.sin_port != 0 &&
                        (!checkPort || out.sender.sin_port == expectedSender.sin_port);

                    if (out.transport != transport || !addressMatches || out.message != expected)
                    {
                        std::cerr << "  transport, sender or message mismatch"
                            << ", expected type=" << static_cast<int>(expected.header.type)
                            << ", actual type=" << static_cast<int>(out.message.header.type)
                            << '\n';
                        return PollResult::Failed;
                    }

                    return PollResult::Passed;
                });
        }

        bool CheckRequest(NetworkResult result, const char* step)
        {
            if (result == NetworkResult::Success)
                return true;

            std::cerr << "[FAIL] " << step << ", result=" << static_cast<int>(result) << '\n';
            return false;
        }

        bool CheckQueued(net::SendResult result, const char* step)
        {
            if (result == net::SendResult::Queued)
                return true;

            std::cerr << "[FAIL] " << step << ", sendResult=" << static_cast<int>(result) << '\n';
            return false;
        }

        template<typename Payload>
        Message MakeMessage(MessageType type, Payload payload, std::uint32_t totalBytes, std::uint32_t sessionId, std::uint32_t sequence)
        {
            Message message{};
            message.header.type = type;
            message.header.totalBytes = totalBytes;
            message.header.sessionId = sessionId;
            message.header.sequence = sequence;
            message.payload = std::move(payload);
            return message;
        }

        Message MakeQueueHeartbeat(std::uint32_t sequence)
        {
            return MakeMessage(MessageType::HEARTBEAT, protocol::HeartbeatPayload{}, static_cast<std::uint32_t>(protocol::HEADER_BYTES), 1, sequence);
        }

        bool RunQueueSender(CNetworkMgr& mgr, const sockaddr_in& peer)
        {
            for (std::uint32_t first = 1; first <= QUEUE_TEST_MESSAGE_COUNT; first += QUEUE_TEST_BATCH_SIZE)
            {
                const std::uint32_t last = first + QUEUE_TEST_BATCH_SIZE - 1;

                for (std::uint32_t sequence = first; sequence <= last; ++sequence)
                {
                    const auto result = mgr.EnqueueSend(net::Channel::Tcp, MakeQueueHeartbeat(sequence));

                    if (result != net::SendResult::Queued)
                    {
                        std::cerr << "[FAIL] EnqueueSend sequence=" << sequence << ", result=" << static_cast<int>(result) << '\n';
                        return false;
                    }
                }

                ReceivedMessage reply{};
                if (!Receive(mgr, TransportType::TCP, MakeQueueHeartbeat(last), peer, true, "Queue batch acknowledged", reply))
                    return false;
            }

            std::cout << "[PASS] queued=" << QUEUE_TEST_MESSAGE_COUNT << '\n';
            return true;
        }

        bool RunQueueReceiver(CNetworkMgr& mgr, const sockaddr_in& peer)
        {
            std::uint32_t expectedSequence = 1;

            for (std::uint32_t first = 1; first <= QUEUE_TEST_MESSAGE_COUNT; first += QUEUE_TEST_BATCH_SIZE)
            {
                const std::uint32_t last = first + QUEUE_TEST_BATCH_SIZE - 1;

                if (!WaitFor("Queue batch received in order", 5s, [&] {

                    NetworkEvent event{};
                    if (mgr.TryPopEvent(event))
                        return UnexpectedEvent(event);

                    ReceivedMessage received{};

                    while (expectedSequence <= last && mgr.TryReceive(received))
                    {
                        const auto expected = MakeQueueHeartbeat(expectedSequence);
                        const bool senderMatches =
                            received.sender.sin_family == peer.sin_family &&
                            received.sender.sin_addr.s_addr == peer.sin_addr.s_addr &&
                            received.sender.sin_port == peer.sin_port;

                        if (received.transport != TransportType::TCP || !senderMatches || received.message != expected)
                        {
                            std::cerr << "[FAIL] Message mismatch, expected sequence=" << expectedSequence << ", actual sequence=" << received.message.header.sequence << '\n';
                            return PollResult::Failed;
                        }

                        ++expectedSequence;
                    }

                    if (expectedSequence > last)
                        return PollResult::Passed;

                    return PollResult::Pending;
                    }))
                {
                    return false;
                }

                if (!CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, MakeQueueHeartbeat(last)), "Queue batch ACK"))
                    return false;
            }

            NetworkEvent event{};
            if (!WaitEvent(mgr, NetworkEventType::Disconnected, event, "Client disconnected"))
                return false;

            ReceivedMessage extra{};
            if (mgr.TryReceive(extra))
            {
                std::cerr << "[FAIL] Extra message, sequence=" << extra.message.header.sequence << '\n';
                return false;
            }

            std::cout << "[PASS] received=" << expectedSequence - 1 << '\n';
            return true;
        }

        net::SnapshotBatch MakeSnapshotBatch(std::uint32_t tick, std::uint8_t partCount = 2, std::uint32_t sessionId = 1000)
        {
            constexpr std::uint32_t SESSION_ID = 1000;
            constexpr auto TOTAL_BYTES = static_cast<std::uint32_t>(protocol::HEADER_BYTES + protocol::SNAPSHOT_HEADER_BYTES + protocol::OBJECT_STATE_RECORD_BYTES);

            net::SnapshotBatch batch;

            for (std::uint8_t index = 0; index < partCount; ++index)
            {
                protocol::ObjectStateRecord object{};
                object.objectId = static_cast<std::uint32_t>(index) + 1;
                object.pos.x = static_cast<std::int32_t>(tick * 100 + index);
                object.hp = 100;
                object.maxHp = 100;

                protocol::StateSnapshotPayload payload{};
                payload.snapshotTick = tick;
                payload.partIndex = index;
                payload.partCount = partCount;
                payload.objects.push_back(object);

                auto message = MakeMessage(MessageType::STATE_SNAPSHOT, std::move(payload), TOTAL_BYTES, SESSION_ID, tick * partCount + index);
                message.header.hostTick = tick;

                batch.parts.push_back(std::move(message));
            }

            return batch;
        }

        bool SendSnapshotBatchFixture(CNetworkMgr& mgr, std::uint32_t sessionId)
        {
            const auto first = MakeMessage(MessageType::HEARTBEAT, protocol::HeartbeatPayload{}, 32, sessionId, 101);
            const auto second = MakeMessage(MessageType::HEARTBEAT, protocol::HeartbeatPayload{}, 32, sessionId, 102);

            const auto batch = MakeSnapshotBatch(10, 2, sessionId);

            return CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, first), "Queue TCP E1")
                && CheckQueued(mgr.EnqueueSnapshotBatch(batch), "Queue snapshot batch")
                && CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, second), "Queue TCP E2");
        }

        bool ReceiveSnapshotBatchFixture(CNetworkMgr& mgr, const sockaddr_in& tcpPeer, const sockaddr_in& udpPeer, std::uint32_t sessionId)
        {
            const auto batch = MakeSnapshotBatch(10, 2, sessionId);
            std::vector<bool> seen(batch.parts.size(), false);
            std::size_t udpCount = 0;
            std::uint32_t tcpCount = 0;

            return WaitFor("TCP 2 messages and UDP 2 parts verified", 5s, [&] {
                NetworkEvent event{};
                if (mgr.TryPopEvent(event))
                    return UnexpectedEvent(event);

                ReceivedMessage received{};
                while (mgr.TryReceive(received))
                {
                    const auto& peer = received.transport == TransportType::TCP ? tcpPeer : udpPeer;
                    const auto& sender = received.sender;
                    if (sender.sin_family != peer.sin_family ||
                        sender.sin_addr.s_addr != peer.sin_addr.s_addr ||
                        sender.sin_port != peer.sin_port)
                    {
                        std::cerr << "[FAIL] Unexpected sender\n";
                        return PollResult::Failed;
                    }

                    if (received.transport == TransportType::TCP)
                    {
                        const auto expected = MakeMessage(MessageType::HEARTBEAT, protocol::HeartbeatPayload{}, 32, sessionId, 101 + tcpCount);

                        if (tcpCount >= 2 || received.message != expected)
                        {
                            std::cerr << "[FAIL] TCP message mismatch, expected sequence=" << 101 + tcpCount << ", actual=" << received.message.header.sequence << '\n';
                            return PollResult::Failed;
                        }

                        ++tcpCount;
                        continue;
                    }

                    const auto* part = std::get_if<protocol::StateSnapshotPayload>(&received.message.payload);
                    if (!part || part->partIndex >= batch.parts.size() ||
                        seen[part->partIndex] || received.message != batch.parts[part->partIndex])
                    {
                        std::cerr << "[FAIL] Invalid, duplicate or mismatched UDP snapshot part\n";
                        return PollResult::Failed;
                    }

                    seen[part->partIndex] = true;
                    ++udpCount;
                }

                return tcpCount == 2 && udpCount == batch.parts.size() ? PollResult::Passed : PollResult::Pending;
                });
        }

        bool RunActiveShutdownCase(CNetworkMgr& mgr, bool udp, unsigned short tcpPort, unsigned short udpPort) {
            using Access = network::SnapshotBatchTestAccess;

            std::atomic<bool> paused{ false };
            Access::InstallStopTestSender(mgr, udp, paused);

            const auto exercise = [&]() {
                NetworkEvent event{};

                if (!CheckRequest(mgr.StartHost(tcpPort, udpPort), "StartHost") || !WaitEvent(mgr, NetworkEventType::Listening, event, "Listening"))
                    return false;

                tcp_test::TestSocket tcpPeer;
                tcpPeer.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (tcpPeer.value == INVALID_SOCKET)
                    return tcp_test::ReportError("socket(TCP)", WSAGetLastError());

                sockaddr_in host{};
                host.sin_family = AF_INET;
                host.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                host.sin_port = htons(tcpPort);

                const auto connected = transport::ConnectTcp(tcpPeer.value, host);
                if (connected.status != transport::IoStatus::Success)
                    return tcp_test::ReportError("connect", connected.errorCode);

                if (!WaitEvent(mgr, NetworkEventType::Connected, event, "Connected"))
                    return false;

                // UDP 시험 첫 조각을 받아줄 로컬 소켓 준비
                tcp_test::TestSocket udpPeer;
                if (udp)
                {
                    udpPeer.value = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    if (udpPeer.value == INVALID_SOCKET)
                        return tcp_test::ReportError("socket(UDP)", WSAGetLastError());

                    sockaddr_in destination{};
                    destination.sin_family = AF_INET;
                    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

                    if (bind(udpPeer.value, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) == SOCKET_ERROR)
                        return tcp_test::ReportError("bind(UDP)", WSAGetLastError());

                    int addressSize = sizeof(destination);
                    if (getsockname(udpPeer.value, reinterpret_cast<sockaddr*>(&destination), &addressSize) == SOCKET_ERROR)
                        return tcp_test::ReportError("getsockname(UDP)", WSAGetLastError());

                    if (!CheckRequest(mgr.SetUdpPeer(destination), "Set UDP peer") || !CheckQueued(mgr.EnqueueSnapshotBatch(MakeSnapshotBatch(10, 16)), "Queue batch"))
                        return false;
                }
                else if (!CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, MakeQueueHeartbeat(1)), "Queue TCP heartbeat"))
                    return false;

                if (!WaitFor(udp ? "UDP parts remain" : "TCP bytes remain", 5s, [&] {
                    if (mgr.TryPopEvent(event))
                        return UnexpectedEvent(event);

                    return paused.load() ? PollResult::Passed : PollResult::Pending;
                    }))
                {
                    return false;
                }

                // 상대 소켓 유지하고 매니저 쪽에서 종료
                std::cout << "[TEST] Shutdown with pending data" << std::endl;
                const auto started = std::chrono::steady_clock::now();

                mgr.Shutdown();

                const auto elapsed = std::chrono::steady_clock::now() - started;
                std::cout << "[PASS] Shutdown returned in " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms\n";
                return true;
                };

            const bool passed = exercise();
            mgr.Shutdown();
            Access::ClearStopTestSender(mgr);

            if (!passed)
                return false;

            if (!CheckRequest(mgr.StartHost(tcpPort, udpPort), "Restart on same ports"))
                return false;

            NetworkEvent event{};
            const bool restarted = WaitEvent(mgr, NetworkEventType::Listening, event, "Listening again on the same ports");
            mgr.Shutdown();

            return restarted;
        }

        bool RunRound(CNetworkMgr& mgr, bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort, std::uint32_t round, TestMode mode)
        {
            const auto started = host
                ? mgr.StartHost(tcpPort, udpPort)
                : mgr.StartClient(hostIp, tcpPort, udpPort);

            if (!CheckRequest(started, "Start"))
                return false;

            NetworkEvent event{};

            if (host && !WaitEvent(mgr, NetworkEventType::Listening, event, "Listening"))
                return false;

            if (!WaitEvent(mgr, NetworkEventType::Connected, event, "Connected", 60s))
                return false;

            const sockaddr_in tcpPeer = event.peer;
            const std::uint32_t sessionId = 1000 + round;
            const std::uint32_t sequence = round * 10;
            const protocol::UdpToken token = 0x0102030405060708ULL + round;

            const auto ready = MakeMessage(MessageType::HEARTBEAT, protocol::HeartbeatPayload{}, 32, sessionId, 100);
            const auto done = MakeMessage(MessageType::HEARTBEAT, protocol::HeartbeatPayload{}, 32, sessionId, 103);

            protocol::AcceptPayload acceptPayload{};
            acceptPayload.udpToken = token;

            protocol::UdpRegisterPayload registerPayload{};
            registerPayload.udpToken = token;

            const auto join = MakeMessage(MessageType::JOIN, protocol::JoinPayload{}, 32, 0, sequence + 1);
            const auto accept = MakeMessage(MessageType::ACCEPT, acceptPayload, 44, sessionId, sequence + 2);
            const auto udpRegister = MakeMessage(MessageType::UDP_REGISTER, registerPayload, 44, sessionId, sequence + 3);
            const auto udpAck = MakeMessage(MessageType::UDP_ACK, protocol::UdpAckPayload{}, 36, sessionId, sequence + 4);

            ReceivedMessage received{};

            if (host)
            {
                if (mgr.EnqueueSend(net::Channel::Udp, udpAck) != net::SendResult::InvalidRequest)
                {
                    std::cerr << "[FAIL] Expected UDP peer to be unset before registration\n";
                    return false;
                }
                std::cout << "[PASS] UDP peer unset before registration\n";

                if (!Receive(mgr, TransportType::TCP, join, tcpPeer, true, "TCP JOIN", received))
                    return false;

                if (!CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, accept), "Queue ACCEPT"))
                    return false;

                // Client의 UDP 포트는 운영체제가 배정하므로 수신한 주소를 사용한다.
                if (!Receive(mgr, TransportType::UDP, udpRegister,
                    tcpPeer, false, "UDP_REGISTER", received))
                {
                    return false;
                }

                if (!CheckRequest(mgr.SetUdpPeer(received.sender), "Set verified UDP peer"))
                    return false;

                if (!CheckQueued(mgr.EnqueueSend(net::Channel::Udp, udpAck), "Queue UDP_ACK"))
                    return false;

                if (mode == TestMode::SnapshotBatch)
                {
                    std::cout << "[HOST] UDP_ACK queued\n";
                    // 다음 단계에서 TCP·UDP 시험 메시지 송신을 연결한다.

                    return Receive(mgr, TransportType::TCP, ready, tcpPeer, true, "Client ready", received)
                        && SendSnapshotBatchFixture(mgr, sessionId)
                        && Receive(mgr, TransportType::TCP, done, tcpPeer, true, "Client verified batch", received);
                }

                // Client는 UDP_ACK를 검증한 뒤 연결을 닫는다.
                return WaitEvent(mgr, NetworkEventType::Disconnected, event, "Disconnected");
            }

            if (!CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, join), "Queue JOIN"))
                return false;

            if (!Receive(mgr, TransportType::TCP, accept, tcpPeer, true, "TCP ACCEPT", received))
                return false;

            sockaddr_in udpPeer = tcpPeer;
            udpPeer.sin_port = htons(udpPort);

            if (!CheckQueued(mgr.EnqueueSend(net::Channel::Udp, udpRegister), "Queue UDP_REGISTER"))
                return false;

            if (!Receive(mgr, TransportType::UDP, udpAck, udpPeer, true, "UDP_ACK", received))
                return false;

            if (mode == TestMode::SnapshotBatch)
            {
                std::cout << "[CLIENT] UDP_ACK verified\n";

                // 다음 단계에서 TCP·UDP 시험 메시지 수신을 연결한다.
                return CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, ready), "Queue ready")
                    && ReceiveSnapshotBatchFixture(mgr, tcpPeer, udpPeer, sessionId)
                    && CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, done), "Queue completion")
                    && WaitEvent(mgr, NetworkEventType::Disconnected, event, "Host disconnected");
            }

            return true;
        }
    }

    bool RunReceiveOverflow(unsigned short tcpPort, unsigned short udpPort)
    {
        auto& mgr = *CNetworkMgr::GetInst();
        if (!CheckRequest(mgr.StartHost(tcpPort, udpPort), "StartHost"))
            return false;

        const auto test = [&]()
            {
                NetworkEvent event{};
                if (!WaitEvent(mgr, NetworkEventType::Listening, event, "Listening"))
                    return false;

                tcp_test::TestSocket peer;
                peer.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (peer.value == INVALID_SOCKET)
                    return tcp_test::ReportError("socket", WSAGetLastError());

                sockaddr_in address{};
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                address.sin_port = htons(tcpPort);

                const auto connected = transport::ConnectTcp(peer.value, address);
                if (connected.status != transport::IoStatus::Success)
                    return tcp_test::ReportError("connect", connected.errorCode);

                // Connected가 한 칸을 차지한다. 마지막 메시지가 상한을 넘긴다.
                constexpr auto limit = net::MAX_RECV_QUEUE_ENTRIES;
                const auto heartbeat = [](std::uint32_t sequence)
                    {
                        return MakeMessage(MessageType::HEARTBEAT, protocol::HeartbeatPayload{},
                            static_cast<std::uint32_t>(protocol::HEADER_BYTES), 1, sequence);
                    };

                std::vector<char> bytes;
                std::vector<std::uint8_t> frame;
                for (std::uint32_t sequence = 1; sequence <= limit; ++sequence)
                {
                    const auto error = protocol::CPacketCodec::Encode(heartbeat(sequence), frame);
                    if (error != protocol::CodecError::NONE)
                    {
                        std::cerr << "[FAIL] Encode HEARTBEAT, codecError=" << static_cast<int>(error) << '\n';
                        return false;
                    }

                    const auto* begin = reinterpret_cast<const char*>(frame.data());
                    bytes.insert(bytes.end(), begin, begin + frame.size());
                }

                if (!tcp_test::TransferExact(peer.value, bytes.data(), bytes.size(), true))
                    return false;

                // 매니저가 소켓을 닫을 때까지 수신 큐에서 꺼내지 않는다.
                if (!WaitFor("Peer closed after overflow", 5s, [&]
                    {
                        char byte{};
                        const auto result = transport::ReceiveTcp(peer.value, &byte, 1);
                        if (result.status == transport::IoStatus::WouldBlock)
                            return PollResult::Pending;

                        if (result.status == transport::IoStatus::Closed ||
                            (result.status == transport::IoStatus::Error && result.errorCode == WSAECONNRESET))
                        {
                            return PollResult::Passed;
                        }

                        return PollResult::Failed;
                    }))
                {
                    return false;
                }

                if (!WaitEvent(mgr, NetworkEventType::Connected, event, "Connected event preserved"))
                    return false;

                ReceivedMessage received{};
                for (std::uint32_t sequence = 1; sequence < limit; ++sequence)
                {
                    if (!mgr.TryReceive(received) || received.transport != TransportType::TCP ||
                        received.message != heartbeat(sequence))
                    {
                        std::cerr << "[FAIL] Missing or incorrect HEARTBEAT at sequence " << sequence << '\n';
                        return false;
                    }
                }

                if (mgr.TryReceive(received))
                {
                    std::cerr << "[FAIL] Accepted more messages than the combined queue limit allows\n";
                    return false;
                }

                std::cout << "[PASS] " << limit - 1 << " messages preserved in order\n";

                if (!mgr.TryPopEvent(event) || event.type != NetworkEventType::Error ||
                    event.socketError != WSAENOBUFS || mgr.TryPopEvent(event))
                {
                    std::cerr << "[FAIL] Expected exactly one WSAENOBUFS event\n";
                    return false;
                }

                std::cout << "[PASS] WSAENOBUFS reported once\n";
                return true;
            };

        const bool passed = test();
        mgr.Shutdown();

        if (passed)
            std::cout << "[SUITE] PASS\n";

        return passed;
    }

    bool RunSnapshotBatchTest()
    {
        using Access = network::SnapshotBatchTestAccess;

        const auto batchA = MakeSnapshotBatch(10);
        const auto batchB = MakeSnapshotBatch(11);
        const auto batchC = MakeSnapshotBatch(12);

        const auto pendingMatches = [](CNetworkMgr& mgr, const net::SnapshotBatch& expected)
            {
                const auto pending = Access::PeekPending(mgr);
                return pending && *pending == expected;
            };

        const bool b01 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                return mgr.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued
                    && pendingMatches(mgr, batchA);
            });

        const bool b02 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                return mgr.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued
                    && mgr.EnqueueSnapshotBatch(batchB) == net::SendResult::ReplacedPending
                    && pendingMatches(mgr, batchB);
            });

        const bool b03 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                if (mgr.EnqueueSnapshotBatch(batchA) != net::SendResult::Queued)
                    return false;

                const auto inFlight = Access::TakePending(mgr);
                if (!inFlight || *inFlight != batchA || Access::PeekPending(mgr))
                    return false;

                return mgr.EnqueueSnapshotBatch(batchB) == net::SendResult::Queued
                    && mgr.EnqueueSnapshotBatch(batchC) == net::SendResult::ReplacedPending
                    && *inFlight == batchA
                    && pendingMatches(mgr, batchC);
            });

        const auto rejectPreservingPending = [&](const net::SnapshotBatch& rejected,
            net::SendResult expectedResult)
            {
                return Access::WithConnectedHost([&](CNetworkMgr& mgr)
                    {
                        return mgr.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued
                            && mgr.EnqueueSnapshotBatch(rejected) == expectedResult
                            && pendingMatches(mgr, batchA)
                            && mgr.EnqueueSnapshotBatch(batchB) == net::SendResult::ReplacedPending
                            && pendingMatches(mgr, batchB);
                    });
            };

        const auto candidate = MakeSnapshotBatch(20);

        auto mismatchedSession = candidate;
        ++mismatchedSession.parts[1].header.sessionId;
        const bool b04 = rejectPreservingPending(mismatchedSession, net::SendResult::InvalidRequest);

        auto duplicatePartIndex = candidate;
        std::get<protocol::StateSnapshotPayload>(duplicatePartIndex.parts[1].payload).partIndex = 0;
        const bool duplicateIndexRejected = rejectPreservingPending(
            duplicatePartIndex, net::SendResult::InvalidRequest);

        auto mismatchedPartCount = candidate;
        std::get<protocol::StateSnapshotPayload>(mismatchedPartCount.parts[1].payload).partCount = 3;
        const bool partCountRejected = rejectPreservingPending(
            mismatchedPartCount, net::SendResult::InvalidRequest);
        const bool b05 = duplicateIndexRejected && partCountRejected;

        auto oversizedPart = candidate;
        auto& objects = std::get<protocol::StateSnapshotPayload>(oversizedPart.parts[0].payload).objects;
        objects.resize(protocol::MAX_SNAPSHOT_OBJECTS_PER_PACKET + 1);
        for (std::size_t i = 0; i < objects.size(); ++i)
            objects[i].objectId = static_cast<protocol::ObjectId>(i + 100);

        const bool oversizedPartRejected = rejectPreservingPending(oversizedPart, net::SendResult::TooLarge);

        auto tooManyParts = candidate;
        tooManyParts.parts.resize(net::MAX_SNAPSHOT_PARTS_PER_BATCH + 1);
        const bool partLimitRejected = rejectPreservingPending(tooManyParts, net::SendResult::TooLarge);
        const bool b06 = oversizedPartRejected && partLimitRejected;

        const bool b07 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                if (mgr.EnqueueSnapshotBatch(batchA) != net::SendResult::Queued)
                    return false;

                const auto inFlight = Access::TakePending(mgr);
                if (!inFlight || *inFlight != batchA)
                    return false;

                return mgr.EnqueueSnapshotBatch(batchB) == net::SendResult::Queued
                    && mgr.EnqueueSnapshotBatch(batchB) == net::SendResult::StaleBatch
                    && pendingMatches(mgr, batchB)
                    && mgr.EnqueueSnapshotBatch(batchA) == net::SendResult::StaleBatch
                    && pendingMatches(mgr, batchB)
                    && *inFlight == batchA;
            });

        const bool b08 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                const auto first = MakeQueueHeartbeat(1);
                const auto second = MakeQueueHeartbeat(2);

                if (mgr.EnqueueSend(net::Channel::Tcp, first) != net::SendResult::Queued
                    || mgr.EnqueueSnapshotBatch(batchA) != net::SendResult::Queued
                    || mgr.EnqueueSnapshotBatch(batchB) != net::SendResult::ReplacedPending
                    || mgr.EnqueueSend(net::Channel::Tcp, second) != net::SendResult::Queued
                    || mgr.EnqueueSnapshotBatch(batchC) != net::SendResult::ReplacedPending)
                {
                    return false;
                }

                const auto requests = Access::PeekSendRequests(mgr);
                return requests.size() == 2
                    && requests[0].transport == TransportType::TCP && requests[0].message == first
                    && requests[1].transport == TransportType::TCP && requests[1].message == second
                    && pendingMatches(mgr, batchC);
            });

        const bool b09 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                if (mgr.EnqueueSnapshotBatch(batchA) != net::SendResult::Queued)
                    return false;

                const auto singleSlots = net::MAX_SEND_QUEUE_ENTRIES - batchA.parts.size();
                for (std::size_t i = 0; i < singleSlots; ++i)
                {
                    if (mgr.EnqueueSend(net::Channel::Tcp, MakeQueueHeartbeat(static_cast<std::uint32_t>(i + 1)))
                        != net::SendResult::Queued)
                    {
                        return false;
                    }
                }

                return mgr.EnqueueSend(net::Channel::Tcp, MakeQueueHeartbeat(static_cast<std::uint32_t>(singleSlots + 1)))
                    == net::SendResult::QueueFull
                    && mgr.EnqueueSnapshotBatch(MakeSnapshotBatch(20, 3)) == net::SendResult::QueueFull
                    && pendingMatches(mgr, batchA)
                    && mgr.EnqueueSnapshotBatch(batchB) == net::SendResult::ReplacedPending
                    && pendingMatches(mgr, batchB)
                    && Access::PeekSendRequests(mgr).size() == singleSlots;
            });

        const bool b10 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                auto original = batchA;
                if (mgr.EnqueueSnapshotBatch(original) != net::SendResult::Queued)
                    return false;

                auto& payload = std::get<protocol::StateSnapshotPayload>(original.parts[0].payload);
                ++payload.objects[0].pos.x;

                return pendingMatches(mgr, batchA);
            });

        const bool b11 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                if (mgr.EnqueueSnapshotBatch(batchA) != net::SendResult::Queued)
                    return false;

                mgr.Shutdown();

                protocol::PlayerInputPayload payload{};
                payload.inputSequence = 1;
                const auto input = MakeMessage(MessageType::PLAYER_INPUT, payload, 40,
                    batchA.parts.front().header.sessionId, 1);

                return mgr.EnqueueSnapshotBatch(batchB) == net::SendResult::NotRunning
                    && mgr.EnqueueSend(net::Channel::Udp, input) == net::SendResult::NotRunning
                    && !Access::PeekPending(mgr)
                    && Access::PeekSendRequests(mgr).empty();
            });

        const bool b12 = Access::WithConnectedHost([&](CNetworkMgr& mgr)
            {
                if (mgr.EnqueueSnapshotBatch(batchC) != net::SendResult::Queued
                    || mgr.EnqueueSend(net::Channel::Tcp, MakeQueueHeartbeat(1)) != net::SendResult::Queued)
                {
                    return false;
                }

                Access::RecordReceiveOverflow(mgr);
                mgr.Shutdown();

                if (!Access::PrepareConnectedHost(mgr))
                    return false;

                NetworkEvent event{};
                // 같은 ID에 더 낮은 틱을 등록해 이전 수락 기록의 제거를 확인한다.
                return !mgr.TryPopEvent(event)
                    && Access::PeekSendRequests(mgr).empty()
                    && mgr.EnqueueSnapshotBatch(batchA) == net::SendResult::Queued
                    && pendingMatches(mgr, batchA);
            });

        std::cout << (b01 ? "[PASS]" : "[FAIL]") << " B01 Initial batch queued\n";
        std::cout << (b02 ? "[PASS]" : "[FAIL]") << " B02 Pending batch replaced\n";
        std::cout << (b03 ? "[PASS]" : "[FAIL]") << " B03 In-flight batch preserved\n";

        std::cout << (b04 ? "[PASS]" : "[FAIL]") << " B04 Mismatched session rejected\n";
        std::cout << (b05 ? "[PASS]" : "[FAIL]") << " B05 Invalid part index or count rejected\n";
        std::cout << (b06 ? "[PASS]" : "[FAIL]") << " B06 Batch size limits enforced\n";
        std::cout << (b07 ? "[PASS]" : "[FAIL]") << " B07 Same and older ticks rejected\n";

        std::cout << (b08 ? "[PASS]" : "[FAIL]") << " B08 TCP requests preserved during batch replacement\n";
        std::cout << (b09 ? "[PASS]" : "[FAIL]") << " B09 Combined send queue limit enforced\n";
        std::cout << (b10 ? "[PASS]" : "[FAIL]") << " B10 Queued batch owns its data\n";

        std::cout << (b11 ? "[PASS]" : "[FAIL]") << " B11 Batch and input rejected after shutdown\n";
        std::cout << (b12 ? "[PASS]" : "[FAIL]") << " B12 Restart clears pending data, ticks and overflow\n";

        const bool retryPassed = Access::WithConnectedHost([&](CNetworkMgr& mgr) {
            return Access::CheckWouldBlockRetry(mgr, batchA);
            });
        std::cout << (retryPassed ? "[PASS]" : "[FAIL]") << " UDP WouldBlock retries the same snapshot part\n";

        const bool passed = b01 && b02 && b03 && b04 && b05 && b06 && b07 && b08 && b09 && b10 && b11 && b12 && retryPassed;
        std::cout << (passed ? "[SUITE] PASS\n" : "[SUITE] FAIL\n");

        return passed;
    }

    bool RunQueueTest(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort)
    {
        auto& mgr = *CNetworkMgr::GetInst();

        const auto started = host
            ? mgr.StartHost(tcpPort, udpPort)
            : mgr.StartClient(hostIp, tcpPort, udpPort);

        if (!CheckRequest(started, "Start queue test"))
            return false;

        NetworkEvent event{};
        bool passed = true;

        if (host)
            passed = WaitEvent(mgr, NetworkEventType::Listening, event, "Listening");

        if (passed)
            passed = WaitEvent(mgr, NetworkEventType::Connected, event, "Connected", 60s);

        if (passed)
        {
            const sockaddr_in peer = event.peer;
            passed = host ? RunQueueReceiver(mgr, peer) : RunQueueSender(mgr, peer);
        }

        mgr.Shutdown();

        if (passed)
            std::cout << "[SUITE] PASS\n";

        return passed;
    }

    bool RunSessionApproval(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort)
    {
        using network::SessionState;

        network::CSessionMgr session(*CNetworkMgr::GetInst());
        const auto started = host
            ? session.StartHost(tcpPort, udpPort)
            : session.StartClient(hostIp, tcpPort, udpPort);

        if (!CheckRequest(started, "Start session"))
            return false;

        bool passed = false;

        if (host)
        {
            std::cout << "Waiting for session-client...\n";
            passed = WaitForSessionState(session, SessionState::UdpRegistered, "Host registered UDP peer", std::chrono::seconds(60));

            if (passed)
                passed = WaitForSessionState(session, SessionState::Disconnected, "Client disconnected after UDP registration", std::chrono::seconds(5));
        }
        else
            passed = WaitForSessionState(session, SessionState::UdpRegistered, "Client received and verified UDP_ACK", std::chrono::seconds(5));

        session.Stop();
        std::cout << (passed ? "[SUITE] PASS\n" : "[SUITE] FAIL\n");

        return passed;
    }

    bool RunSessionBadTokenClient(const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort)
    {
        auto& mgr = *CNetworkMgr::GetInst();

        if (!CheckRequest(mgr.StartClient(hostIp, tcpPort, udpPort), "Start bad-token client"))
            return false;

        const auto runTest = [&]() -> bool
            {
                NetworkEvent connected{};
                if (!WaitEvent(mgr, NetworkEventType::Connected, connected, "Connected"))
                    return false;

                const protocol::Message join{};
                if (!CheckQueued(mgr.EnqueueSend(net::Channel::Tcp, join), "Queue JOIN"))
                    return false;

                std::uint32_t sessionId = 0;
                protocol::PlayerSlot playerId = protocol::INVALID_PLAYER_SLOT;
                protocol::UdpToken validToken = 0;

                const auto pollAccept = [&]() -> PollResult
                    {
                        NetworkEvent event{};
                        if (mgr.TryPopEvent(event))
                            return UnexpectedEvent(event);

                        ReceivedMessage received{};
                        if (!mgr.TryReceive(received))
                            return PollResult::Pending;

                        const auto& message = received.message;
                        const auto* accept = std::get_if<protocol::AcceptPayload>(&message.payload);

                        if (received.transport != TransportType::TCP ||
                            message.header.type != MessageType::ACCEPT ||
                            message.header.sessionId == 0 ||
                            !accept || accept->assignedPlayerId != 1 || accept->udpToken == 0)
                        {
                            return PollResult::Failed;
                        }

                        sessionId = message.header.sessionId;
                        playerId = accept->assignedPlayerId;
                        validToken = accept->udpToken;
                        return PollResult::Passed;
                    };

                if (!WaitFor("Receive TCP ACCEPT", std::chrono::seconds(5), pollAccept))
                    return false;

                protocol::UdpRegisterPayload payload{};
                payload.playerId = playerId;
                payload.udpToken = validToken == 1 ? 2 : 1;

                auto request = MakeMessage(MessageType::UDP_REGISTER, payload, 44, sessionId, 1);

                if (!CheckQueued(mgr.EnqueueSend(net::Channel::Udp, request), "Queue wrong-token UDP_REGISTER"))
                    return false;

                if (!ExpectNoUdpAck(mgr))
                    return false;

                payload.udpToken = validToken;
                request.payload = payload;
                request.header.sequence = 2;

                if (!CheckQueued(mgr.EnqueueSend(net::Channel::Udp, request), "Queue valid UDP_REGISTER"))
                    return false;

                const auto pollAck = [&]() -> PollResult
                    {
                        NetworkEvent event{};
                        if (mgr.TryPopEvent(event))
                            return UnexpectedEvent(event);

                        ReceivedMessage received{};
                        if (!mgr.TryReceive(received))
                            return PollResult::Pending;

                        const auto& message = received.message;
                        const auto* ack = std::get_if<protocol::UdpAckPayload>(&message.payload);

                        if (received.transport != TransportType::UDP ||
                            message.header.type != MessageType::UDP_ACK ||
                            message.header.sessionId != sessionId ||
                            received.sender.sin_family != AF_INET ||
                            received.sender.sin_addr.s_addr != connected.peer.sin_addr.s_addr ||
                            received.sender.sin_port != htons(udpPort) ||
                            !ack || ack->playerId != playerId || ack->registered != 1)
                        {
                            return PollResult::Failed;
                        }

                        return PollResult::Passed;
                    };

                return WaitFor("Receive UDP_ACK for valid token", std::chrono::seconds(5), pollAck);
            };

        const bool passed = runTest();
        mgr.Shutdown();

        std::cout << (passed ? "[SUITE] PASS\n" : "[SUITE] FAIL\n");
        return passed;
    }


    bool RunListeningShutdown(unsigned short tcpPort, unsigned short udpPort)
    {
        auto& mgr = *CNetworkMgr::GetInst();

        const auto startListening = [&](const char* step)
            {
                if (!CheckRequest(mgr.StartHost(tcpPort, udpPort), "StartHost"))
                    return false;

                NetworkEvent event{};
                return WaitEvent(mgr, NetworkEventType::Listening, event, step);
            };

        const auto shutdown = [&](const char* step)
            {
                std::cout << "[TEST] " << step << std::endl;
                const auto started = std::chrono::steady_clock::now();
                mgr.Shutdown();
                const auto elapsed = std::chrono::steady_clock::now() - started;
                std::cout << "[PASS] " << step << " returned in "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                    << " ms\n";
            };

        if (!startListening("Listening before shutdown"))
        {
            mgr.Shutdown();
            return false;
        }

        const auto duplicate = mgr.StartHost(tcpPort, udpPort);
        if (duplicate != NetworkResult::AlreadyRunning)
        {
            std::cerr << "[FAIL] Expected AlreadyRunning, result="
                << static_cast<int>(duplicate) << '\n';
            mgr.Shutdown();
            return false;
        }

        std::cout << "[PASS] Duplicate StartHost rejected\n";
        shutdown("Shutdown while listening");
        shutdown("Repeated Shutdown");

        const bool restarted = startListening("Listening again on the same ports");
        if (!restarted)
        {
            mgr.Shutdown();
            return false;
        }

        shutdown("Shutdown after restart");
        std::cout << "[SUITE] PASS\n";
        return true;
    }

    bool RunActiveShutdown(unsigned short tcpPort, unsigned short udpPort)
    {
        WSADATA wsa{};
        const int error = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (error != 0)
            return tcp_test::ReportError("WSAStartup", error);

        auto& mgr = *CNetworkMgr::GetInst();

        std::cout << "=== TCP partial-send shutdown ===\n";
        const bool tcpPassed = RunActiveShutdownCase(mgr, false, tcpPort, udpPort);

        std::cout << "=== UDP batch-send shutdown ===\n";
        const bool udpPassed = RunActiveShutdownCase(mgr, true, tcpPort, udpPort);

        WSACleanup();

        const bool passed = tcpPassed && udpPassed;
        std::cout << (passed ? "[SUITE] PASS\n" : "[SUITE] FAIL\n");
        return passed;
    }

    bool Run(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort, TestMode mode)
    {
        auto& mgr = *CNetworkMgr::GetInst();

        for (std::uint32_t round = 1; round <= 2; ++round)
        {
            if (!host && round == 2)
            {
                std::cout << "Host에 두 번째 Listening이 표시되면 Enter를 누르세요.\n";

                std::string input;
                if (!std::getline(std::cin, input))
                    return false;
            }

            std::cout << "\n=== CNetworkMgr 시험 " << round << " ===\n";

            const bool passed = RunRound(mgr, host, hostIp, tcpPort, udpPort, round, mode);
            mgr.Shutdown();

            if (!passed)
                return false;
        }

        std::cout << "[SUITE] PASS\n";
        return true;
    }
}
