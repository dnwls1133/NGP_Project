#include "NetworkMgrTest.h"

#include "../Engine/define.h"
#include "../Engine/CNetworkMgr.h"
#include "../net/SendContract.h"
#include "TcpTestSupport.h"

#include <ws2tcpip.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

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

        bool RunRound(CNetworkMgr& mgr, bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort, std::uint32_t round)
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

            protocol::AcceptPayload acceptPayload{};
            acceptPayload.udpToken = token;

            protocol::UdpRegisterPayload registerPayload{};
            registerPayload.udpToken = token;

            const auto join = MakeMessage(MessageType::JOIN,
                protocol::JoinPayload{}, 32, 0, sequence + 1);
            const auto accept = MakeMessage(MessageType::ACCEPT,
                acceptPayload, 44, sessionId, sequence + 2);
            const auto udpRegister = MakeMessage(MessageType::UDP_REGISTER,
                registerPayload, 44, sessionId, sequence + 3);
            const auto udpAck = MakeMessage(MessageType::UDP_ACK,
                protocol::UdpAckPayload{}, 36, sessionId, sequence + 4);

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

            return Receive(mgr, TransportType::UDP, udpAck, udpPeer, true, "UDP_ACK", received);
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

    bool Run(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort)
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

            const bool passed = RunRound(mgr, host, hostIp, tcpPort, udpPort, round);
            mgr.Shutdown();

            if (!passed)
                return false;
        }

        std::cout << "[SUITE] PASS\n";
        return true;
    }
}
