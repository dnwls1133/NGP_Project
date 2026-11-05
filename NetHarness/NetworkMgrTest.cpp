#include "NetworkMgrTest.h"

#include "../Engine/define.h"
#include "../Engine/CNetworkMgr.h"

#include <ws2tcpip.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

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

        bool WaitEvent(CNetworkMgr& mgr, NetworkEventType expected, NetworkEvent& out,
            const char* step, std::chrono::milliseconds timeout = 5s)
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

        bool Receive(CNetworkMgr& mgr, TransportType transport, const Message& expected,
            const sockaddr_in& expectedSender, bool checkPort, const char* step,
            ReceivedMessage& out)
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

            std::cerr << "[FAIL] " << step
                << ", result=" << static_cast<int>(result) << '\n';
            return false;
        }

        template<typename Payload>
        Message MakeMessage(MessageType type, Payload payload, std::uint32_t totalBytes,
            std::uint32_t sessionId, std::uint32_t sequence)
        {
            Message message{};
            message.header.type = type;
            message.header.totalBytes = totalBytes;
            message.header.sessionId = sessionId;
            message.header.sequence = sequence;
            message.payload = std::move(payload);
            return message;
        }

        bool RunRound(CNetworkMgr& mgr, bool host, const std::string& hostIp,
            unsigned short tcpPort, unsigned short udpPort, std::uint32_t round)
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
                if (!Receive(mgr, TransportType::TCP, join, tcpPeer, true, "TCP JOIN", received))
                    return false;

                if (!CheckRequest(mgr.SendTcp(accept), "Queue ACCEPT"))
                    return false;

                // Client의 UDP 포트는 운영체제가 배정하므로 수신한 주소를 사용한다.
                if (!Receive(mgr, TransportType::UDP, udpRegister,
                    tcpPeer, false, "UDP_REGISTER", received))
                {
                    return false;
                }

                if (!CheckRequest(mgr.SendUdp(received.sender, udpAck), "Queue UDP_ACK"))
                    return false;

                // Client는 UDP_ACK를 검증한 뒤 연결을 닫는다.
                return WaitEvent(mgr, NetworkEventType::Disconnected, event, "Disconnected");
            }

            if (!CheckRequest(mgr.SendTcp(join), "Queue JOIN"))
                return false;

            if (!Receive(mgr, TransportType::TCP, accept, tcpPeer, true, "TCP ACCEPT", received))
                return false;

            sockaddr_in udpPeer = tcpPeer;
            udpPeer.sin_port = htons(udpPort);

            if (!CheckRequest(mgr.SendUdp(udpPeer, udpRegister), "Queue UDP_REGISTER"))
                return false;

            return Receive(mgr, TransportType::UDP, udpAck, udpPeer, true, "UDP_ACK", received);
        }
    }

    bool Run(bool host, const std::string& hostIp,
        unsigned short tcpPort, unsigned short udpPort)
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
