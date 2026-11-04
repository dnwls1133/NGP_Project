#include "HostTest.h"
#include "TestTransport.h"
#include "MessageTest.h"
#include "TestControl.h"

#include <conio.h>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <random>
#include <utility>

namespace
{
    using Clock = std::chrono::steady_clock;
    using protocol::Message;
    using protocol::MessageType;

    struct HostContext
    {
        // 기존 소켓 생성·정리 함수가 사용하는 이름 유지
        SOCKET listenSocket = INVALID_SOCKET;
        SOCKET clientSocket = INVALID_SOCKET;
        SOCKET udpSocket = INVALID_SOCKET;

        sockaddr_in clientTcpAddress{};
        sockaddr_in clientUdpAddress{};

        transport::TcpState tcp;
        transport::UdpState udp;

        // 세션 검사 후 처리할 메시지
        std::deque<Message> inbox;
        std::deque<transport::ReceivedDatagram> datagrams;

        message_test::TestContext session{};
        std::uint32_t repeats = 10;
        std::uint32_t sequence = 1;

        bool approved = false;
        bool udpRegistered = false;
        bool allowStart = false;
        bool startRequested = false;
        bool failed = false;
        bool suitePassed = true;

        Clock::time_point lastReceive = Clock::now();
        Clock::time_point lastHeartbeat = Clock::now();
    };

    bool BindSocket(SOCKET sock, unsigned short port)
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);

        if (bind(sock, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
        {
            std::cerr << "bind failed: " << WSAGetLastError() << "\n";
            return false;
        }

        return true;
    }

    bool CreateHostSockets(HostContext& host, unsigned short tcpPort, unsigned short udpPort)
    {
        host.listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (host.listenSocket == INVALID_SOCKET)
        {
            std::cerr << "TCP socket failed: " << WSAGetLastError() << "\n";
            return false;
        }

        if (!BindSocket(host.listenSocket, tcpPort))
            return false;

        if (listen(host.listenSocket, 1) == SOCKET_ERROR)
        {
            std::cerr << "listen failed: " << WSAGetLastError() << "\n";
            return false;
        }

        host.udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (host.udpSocket == INVALID_SOCKET)
        {
            std::cerr << "UDP socket failed: " << WSAGetLastError() << "\n";
            return false;
        }

        if (!BindSocket(host.udpSocket, udpPort))
            return false;

        if (!transport::SetNonBlocking(host.listenSocket) || !transport::SetNonBlocking(host.udpSocket))
        {
            std::cerr << "Nonblocking setup failed: " << WSAGetLastError() << "\n";
            return false;
        }

        std::cout << "[HOST] TCP port: " << tcpPort << "\n" << "[HOST] UDP port: " << udpPort << "\n";

        return true;
    }

    bool AcceptClient(HostContext& host)
    {
        int addressSize = sizeof(host.clientTcpAddress);

        SOCKET client = accept(host.listenSocket, reinterpret_cast<sockaddr*>(&host.clientTcpAddress), &addressSize);

        if (client == INVALID_SOCKET)
        {
            const int error = WSAGetLastError();

            if (error == WSAEWOULDBLOCK)
                return true;

            std::cerr << "accept failed: " << error << "\n";
            return false;
        }

        if (!transport::SetNonBlocking(client))
        {
            const int error = WSAGetLastError();
            transport::CloseSocket(client);

            std::cerr << "Client nonblocking setup failed: " << error << "\n";
            return false;
        }

        host.clientSocket = client;
        host.lastReceive = Clock::now();

        transport::CloseSocket(host.listenSocket);

        std::cout << "[HOST] TCP 연결됨. JOIN 대기 중...\n";
        return true;
    }

    bool Fail(HostContext& host, const char* reason)
    {
        std::cerr << "[FAIL] " << reason << "\n";
        host.failed = true;
        return false;
    }

    bool CheckIo(HostContext& host, const transport::MessageResult& result)
    {
        if (result.status == transport::IoStatus::Success || result.status == transport::IoStatus::WouldBlock)
        {
            return true;
        }

        std::cerr << "[NETWORK] socketError=" << result.socketError << " codecError=" << static_cast<int>(result.codecError) << "\n";

        return Fail(host, result.status == transport::IoStatus::Closed ? "Client disconnected" : "Transport operation failed");
    }

    template<typename Payload>
    Message MakeMessage(HostContext& host, MessageType type, Payload payload, std::uint32_t totalBytes)
    {
        Message message{};
        message.header.type = type;
        message.header.totalBytes = totalBytes;
        message.header.sessionId = host.session.sessionId;
        message.header.sequence = host.sequence++;
        message.payload = std::move(payload);
        return message;
    }

    bool QueueTcp(HostContext& host, const Message& message)
    {
        return CheckIo(host, transport::QueueTcpMessage(host.tcp, message));
    }

    bool QueueControl(HostContext& host, test_control::Command command, std::uint32_t index, bool passed = true)
    {
        return QueueTcp(host, test_control::Make(host.session, command, index, host.repeats, passed));
    }

    bool SameAddress(const sockaddr_in& left, const sockaddr_in& right)
    {
        return left.sin_addr.s_addr == right.sin_addr.s_addr &&
            left.sin_port == right.sin_port;
    }

    bool PumpNetwork(HostContext& host)
    {
        if (_kbhit())
        {
            const int key = _getch();

            if (key == 'q' || key == 'Q')
                return Fail(host, "Cancelled by host");

            if (host.allowStart && (key == 's' || key == 'S'))
                host.startRequested = true;
        }

        const auto now = Clock::now();

        if (host.approved)
        {
            if (now - host.lastReceive > std::chrono::seconds(5))
                return Fail(host, "Client heartbeat timeout");

            if (now - host.lastHeartbeat >= std::chrono::seconds(1))
            {
                if (!QueueTcp(host, MakeMessage(host, MessageType::HEARTBEAT, protocol::HeartbeatPayload{}, 32)))
                {
                    return false;
                }

                host.lastHeartbeat = now;
            }
        }

        const bool connected = host.clientSocket != INVALID_SOCKET;

        transport::SocketReady ready{};

        if (!transport::WaitForSockets(connected ? host.clientSocket : host.listenSocket, host.udpSocket, connected && !host.tcp.sendQueue.empty(), !host.udp.sendQueue.empty(), 20, ready))
        {
            std::cerr << "select failed: " << WSAGetLastError() << "\n";
            return Fail(host, "select failed");
        }

        if (ready.tcpReadable)
        {
            if (!connected)
            {
                if (!AcceptClient(host))
                    return false;
            }
            else
            {
                if (!CheckIo(host, transport::ReceiveTcpMessages(host.clientSocket, host.tcp)))
                {
                    return false;
                }
            }
        }

        if (ready.tcpWritable && connected)
        {
            if (!CheckIo(host, transport::FlushTcpSend(host.clientSocket, host.tcp)))
            {
                return false;
            }
        }

        if (ready.udpReadable)
        {
            const auto result = transport::ReceiveUdpMessage(host.udpSocket, host.udp);

            if (result.status == transport::IoStatus::Error)
            {
                // 잘못된 데이터그램 하나는 버리고 계속 진행한다.
                if (result.codecError != protocol::CodecError::NONE || result.socketError == WSAEMSGSIZE || result.socketError == WSAEPROTOTYPE)
                {
                    std::cerr << "[UDP] Invalid datagram discarded\n";
                }
                else if (!CheckIo(host, result))
                {
                    return false;
                }
            }
        }

        if (ready.udpWritable)
        {
            if (!CheckIo(host, transport::FlushUdpSend(host.udpSocket, host.udp)))
                return false;
        }

        while (!host.tcp.received.empty())
        {
            Message message = std::move(host.tcp.received.front());
            host.tcp.received.pop_front();

            const bool initialJoin =
                message.header.type == MessageType::JOIN &&
                message.header.sessionId == 0;

            if (message.header.roundId != 1 ||
                (!initialJoin &&
                    message.header.sessionId != host.session.sessionId))
            {
                return Fail(host, "Invalid TCP session or round");
            }

            host.lastReceive = Clock::now();

            // 일반 heartbeat만 소비한다.
            // 0x40000000~: 반복 시험 데이터
            // 0x80000000~: 시험 동기화 제어
            if (message.header.type == MessageType::HEARTBEAT &&
                message.header.sequence < 0x40000000u)
            {
                continue;
            }

            if (host.inbox.size() >= transport::MAX_QUEUE_MESSAGES)
                return Fail(host, "Host TCP inbox overflow");

            host.inbox.push_back(std::move(message));
        }

        while (!host.udp.received.empty())
        {
            auto datagram = std::move(host.udp.received.front());
            host.udp.received.pop_front();

            if (!host.approved ||
                datagram.message.header.sessionId != host.session.sessionId ||
                datagram.message.header.roundId != 1 ||
                datagram.sender.sin_addr.s_addr !=
                host.clientTcpAddress.sin_addr.s_addr)
            {
                continue;
            }

            if (host.udpRegistered && !SameAddress(datagram.sender, host.clientUdpAddress))
                continue;

            if (host.udpRegistered)
                host.lastReceive = Clock::now();

            if (host.datagrams.size() >= transport::MAX_QUEUE_MESSAGES)
                return Fail(host, "Host UDP inbox overflow");

            host.datagrams.push_back(std::move(datagram));
        }

        return true;
    }

    bool WaitUntil(HostContext& host, const std::function<bool()>& complete, int timeoutMs = 0)
    {
        const auto started = Clock::now();

        while (!complete())
        {
            if (host.failed)
                return false;

            if (timeoutMs > 0 && Clock::now() - started >= std::chrono::milliseconds(timeoutMs))
            {
                return Fail(host, "Stage timeout");
            }

            if (!PumpNetwork(host))
                return false;
        }

        return !host.failed;
    }

    bool FlushPending(HostContext& host)
    {
        return WaitUntil(
            host,
            [&]
            {
                return host.tcp.sendQueue.empty() &&
                    host.udp.sendQueue.empty();
            },
            5000);
    }

    bool WaitTcp(HostContext& host, MessageType type, Message& out, int timeoutMs)
    {
        return WaitUntil(
            host,
            [&]
            {
                if (host.inbox.empty())
                    return false;

                out = std::move(host.inbox.front());
                host.inbox.pop_front();

                if (out.header.type != type)
                    Fail(host, "Unexpected TCP message");

                return true;
            },
            timeoutMs);
    }

    bool WaitControl(HostContext& host, test_control::Command command, std::uint32_t index, test_control::Control& out, int timeoutMs = 5000)
    {
        return WaitUntil(
            host,
            [&]
            {
                while (!host.inbox.empty())
                {
                    Message message = std::move(host.inbox.front());
                    host.inbox.pop_front();

                    // 전 단계의 늦은 시험 데이터 제어 메시지로 해석하지 않음
                    if (!test_control::Read(message, out))
                    {
                        std::cerr << "[TEST] Unexpected data between cases\n";
                        host.suitePassed = false;
                        continue;
                    }

                    if (out.command != command || out.caseIndex != index || out.repeats != host.repeats)
                        Fail(host, "Unexpected test control");

                    return true;
                }

                return false;
            },
            5000);
    }

    bool SendRoomState(HostContext& host, bool clientReady)
    {
        protocol::RoomStatePayload room{};
        room.slots[0] = protocol::RoomSlotState{ 0, 1, 1, 1 };
        room.slots[1] = protocol::RoomSlotState{
            1, 1,
            static_cast<std::uint8_t>(clientReady),
            static_cast<std::uint8_t>(host.udpRegistered)
        };

        return QueueTcp(host, MakeMessage(host, MessageType::ROOM_STATE, room, 40));
    }

    bool RegisterClientUdp(HostContext& host)
    {
        std::cout << "[HOST] UDP 등록 대기 중...\n";

        return WaitUntil(
            host,
            [&]
            {
                while (!host.datagrams.empty())
                {
                    auto datagram = std::move(host.datagrams.front());
                    host.datagrams.pop_front();

                    const auto* registration = std::get_if<protocol::UdpRegisterPayload>(&datagram.message.payload);

                    if (datagram.message.header.type != MessageType::UDP_REGISTER ||
                        registration == nullptr || registration->playerId != 1 || registration->udpToken != host.session.udpToken)
                    {
                        std::cerr << "[UDP] Registration rejected\n";
                        continue;
                    }

                    host.clientUdpAddress = datagram.sender;
                    host.udpRegistered = true;
                    host.lastReceive = Clock::now();

                    const auto ack = MakeMessage(
                        host, MessageType::UDP_ACK,
                        protocol::UdpAckPayload{}, 36);

                    if (!CheckIo(host, transport::QueueUdpMessage(host.udp, host.clientUdpAddress, ack)))
                        return true;

                    std::cout << "[HOST] UDP 등록 완료\n";
                    return true;
                }

                return false;
            },
            5000);
    }

    bool WaitClientReady(HostContext& host)
    {
        if (!SendRoomState(host, false))
            return false;

        std::cout << "Client 레디 대기 중...\n";

        while (true)
        {
            Message message;

            // 키 입력을 기다리는 동안에도 heartbeat 처리
            if (!WaitTcp(host, MessageType::READY, message, 0))
                return false;

            const auto* ready =
                std::get_if<protocol::ReadyPayload>(&message.payload);

            if (ready == nullptr || ready->playerId != 1)
                return Fail(host, "Invalid READY");

            const bool clientReady = ready->isReady != 0;

            if (!SendRoomState(host, clientReady))
                return false;

            if (clientReady)
                return true;
        }
    }

    void CloseHostSockets(HostContext& host)
    {
        transport::CloseSocket(host.clientSocket);
        transport::CloseSocket(host.listenSocket);
        transport::CloseSocket(host.udpSocket);
    }

    bool RunReceiveCase(HostContext& host, message_test::CaseState& test, std::uint32_t index)
    {
        bool peerDataSent = false;

        const bool completed = WaitUntil(
            host,
            [&]
            {
                while (!host.inbox.empty())
                {
                    Message message = std::move(host.inbox.front());
                    host.inbox.pop_front();

                    test_control::Control control{};

                    if (test_control::Read(message, control))
                    {
                        if (control.command != test_control::Command::DataSent ||
                            control.caseIndex != index ||
                            control.repeats != host.repeats ||
                            peerDataSent)
                        {
                            Fail(host, "Unexpected DataSent control");
                            return true;
                        }

                        peerDataSent = true;
                        continue;
                    }

                    message_test::RecordReceived(test, message, false);
                }

                while (!host.datagrams.empty())
                {
                    auto datagram = std::move(host.datagrams.front());
                    host.datagrams.pop_front();

                    message_test::RecordReceived(
                        test, datagram.message, true);
                }

                return
                    (peerDataSent && message_test::ReceiveComplete(test)) ||
                    message_test::TimedOut(test);
            },
            6000);

        if (!completed)
            return false;

        message_test::PrintResult(test, message_test::PeerRole::Host);

        // 완료 표식 자체가 없으면 상대와 시험 단계가 어긋난 상태
        if (!peerDataSent)
            return Fail(host, "Missing DataSent control");

        const bool passed = message_test::Passed(test);
        host.suitePassed = host.suitePassed && passed;

        return QueueControl(host, test_control::Command::Result, index, passed);
    }

    bool RunSendCase(HostContext& host, message_test::CaseState& test, std::uint32_t index)
    {
        // 최대 N=1000, 송신 큐 상한=1024.
        // N개 등록 후 실제 큐가 모두 처리될 때까지 대기
        while (test.stats.queued < test.repeats)
        {
            if (!CheckIo(host, message_test::QueueNextTestMessage(test, message_test::PeerRole::Host, host.tcp, host.udp, host.clientUdpAddress)))
                return false;
        }

        if (!FlushPending(host))
            return false;

        if (!QueueControl(host, test_control::Command::DataSent, index))
            return false;

        test_control::Control result{};

        if (!WaitControl(host, test_control::Command::Result, index, result, 7000))
            return false;

        const auto* definition = message_test::FindCase(test.type);

        std::cout << "[" << definition->name << "]" << " receiver=Client " << (result.passed ? "PASS" : "FAIL") << "\n";

        host.suitePassed = host.suitePassed && result.passed;
        return true;
    }

    bool RunMessageSuite(HostContext& host)
    {
        using test_control::Command;

        for (std::uint32_t index = 0; index < message_test::CASES.size(); ++index)
        {
            const auto& definition = message_test::CASES[index];

            std::cout << "\n[TEST] " << definition.name << " x " << host.repeats << "\n";

            if (!QueueControl(host, Command::Prepare, index))
                return false;

            test_control::Control prepared{};

            if (!WaitControl(host, Command::Prepared, index, prepared))
                return false;

            if (!prepared.passed)
                return Fail(host, "Client could not prepare test");

            message_test::CaseState test;

            if (!message_test::BeginCase(test, definition.type, host.repeats, host.session))
                return Fail(host, "BeginCase failed");

            if (!QueueControl(host, Command::Go, index))
                return false;

            const bool sender = message_test::IsSender(test, message_test::PeerRole::Host);

            if (sender)
            {
                if (!RunSendCase(host, test, index))
                    return false;
            }
            else
            {
                if (!RunReceiveCase(host, test, index))
                    return false;
            }

            if (!QueueControl(host, Command::Advance, index))
                return false;

            test_control::Control advanced{};

            if (!WaitControl(host, Command::Advanced, index, advanced))
                return false;

            // 이미 종료한 타입의 늦은 UDP 데이터는 다음 타입에 섞지 않기
            if (!host.datagrams.empty())
            {
                std::cerr << "[TEST] Late UDP datagrams: " << host.datagrams.size() << "\n";

                host.suitePassed = false;
                host.datagrams.clear();
            }
        }

        const auto finishIndex = static_cast<std::uint32_t>(message_test::CASES.size());

        if (!QueueControl(host, Command::Finish, finishIndex, host.suitePassed))
            return false;

        test_control::Control finished{};

        if (!WaitControl(host, Command::Finished, finishIndex, finished))
            return false;

        host.suitePassed = host.suitePassed && finished.passed;

        std::cout << "\n[SUITE] " << (host.suitePassed ? "PASS" : "FAIL") << "\n";

        // 반복 시험 모드 종료 후, 실제 종료 메시지 전송
        if (!QueueTcp(host, MakeMessage(host, MessageType::END, protocol::EndPayload{}, 36)))
        {
            return false;
        }

        if (!FlushPending(host))
            return false;

        return host.suitePassed;
    }

    bool RunHostSteps(HostContext& host, bool rejectJoin)
    {
        std::cout << "Client 대기 중...\n"
            << "Q 키: 취소\n";

        if (!WaitUntil(host, [&] { return host.clientSocket != INVALID_SOCKET; }))
            return false;

        Message join;

        if (!WaitTcp(host, MessageType::JOIN, join, 5000))
            return false;

        if (join.header.sessionId != 0 || !std::holds_alternative<protocol::JoinPayload>(join.payload))
        {
            return Fail(host, "Invalid JOIN");
        }

        if (rejectJoin)
        {
            protocol::RejectPayload rejected{};
            rejected.reason = protocol::RejectReason::ROOM_FULL;

            if (!QueueTcp(host, MakeMessage(host, MessageType::REJECT, rejected, 36)))
                return false;

            if (!FlushPending(host))
                return false;

            std::cout << "[HOST] 시험용 REJECT 전송 완료\n";
            return true;
        }

        protocol::AcceptPayload accepted{};
        accepted.assignedPlayerId = 1;
        accepted.udpToken = host.session.udpToken;

        if (!QueueTcp(host, MakeMessage(host, MessageType::ACCEPT, accepted, 44)))
            return false;

        host.approved = true;
        host.lastReceive = Clock::now();
        host.lastHeartbeat = Clock::now();

        std::cout << "[HOST] ACCEPT 등록. PlayerId=1, SessionId=" << host.session.sessionId << "\n";

        if (!RegisterClientUdp(host))
            return false;

        if (!WaitClientReady(host))
            return false;

        std::cout << "S 키를 누르면 시작\n";
        host.allowStart = true;

        if (!WaitUntil(host, [&] { return host.startRequested; }))
        {
            return false;
        }

        host.allowStart = false;

        if (!QueueTcp(host, MakeMessage(host, MessageType::LOAD, protocol::LoadPayload{}, 36)))
            return false;

        std::cout << "[HOST] 호스트 시험용 로딩 완료. Client 로딩 대기...\n";

        Message loaded;

        if (!WaitTcp(host, MessageType::LOADED, loaded, 15000))
            return false;

        const auto* loading = std::get_if<protocol::LoadedPayload>(&loaded.payload);

        if (loading == nullptr || loading->playerId != 1)
            return Fail(host, "Invalid LOADED");

        protocol::StartPayload start{};
        start.startHostTick = 1000;

        if (!QueueTcp(host, MakeMessage(host, MessageType::START, start, 44)))
        {
            return false;
        }

        if (!FlushPending(host))
            return false;

        std::cout << "[HOST] START 전송. 메시지별 시험 시작\n";
        return RunMessageSuite(host);
    }
}

bool RunHost(unsigned short tcpPort, unsigned short udpPort, std::uint32_t repeats, bool rejectJoin)
{
    if (repeats == 0 || repeats > message_test::MAX_REPEATS)
    {
        std::cerr << "N must be between 1 and " << message_test::MAX_REPEATS << "\n";
        return false;
    }

    HostContext host;
    host.repeats = repeats;

    // 시험 세션마다 새 식별자와 토큰 생성
    std::random_device random;

    do
    {
        host.session.sessionId = static_cast<std::uint32_t>(random());
    } while (host.session.sessionId == 0);

    do
    {
        host.session.udpToken = (static_cast<std::uint64_t>(random()) << 32) | static_cast<std::uint32_t>(random());
    } while (host.session.udpToken == 0);

    bool passed = false;

    if (CreateHostSockets(host, tcpPort, udpPort))
        passed = RunHostSteps(host, rejectJoin);

    CloseHostSockets(host);
    return passed;
}
