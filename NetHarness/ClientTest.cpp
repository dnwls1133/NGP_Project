#include "ClientTest.h"
#include "TestTransport.h"
#include "MessageTest.h"
#include "TestControl.h"

#include <conio.h>
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

namespace
{
    using Clock = std::chrono::steady_clock;
    using protocol::Message;
    using protocol::MessageType;

    struct ClientContext
    {
        SOCKET tcpSocket = INVALID_SOCKET;
        SOCKET udpSocket = INVALID_SOCKET;

        sockaddr_in hostTcp{};
        sockaddr_in hostUdp{};

        transport::TcpState tcp;
        transport::UdpState udp;

        std::deque<Message> inbox;
        std::deque<transport::ReceivedDatagram> datagrams;

        message_test::TestContext session{};

        std::uint32_t sequence = 1;
        std::uint32_t repeats = 0;
        protocol::PlayerSlot playerId = 1;

        bool approved = false;
        bool allowReady = false;
        bool readyRequested = false;
        bool failed = false;
        bool suitePassed = true;

        Clock::time_point lastReceive = Clock::now();
        Clock::time_point lastHeartbeat = Clock::now();
    };

    bool ReadHostAddress(ClientContext& client, unsigned short tcpPort, unsigned short udpPort)
    {
        std::string ip;
        std::cout << "Host IP를 입력해 주십시오: ";

        if (!(std::cin >> ip))
            return false;

        client.hostTcp.sin_family = AF_INET;
        client.hostTcp.sin_port = htons(tcpPort);

        if (inet_pton(AF_INET, ip.c_str(), &client.hostTcp.sin_addr) != 1)
        {
            std::cerr << "Invalid IPv4 address\n";
            return false;
        }

        client.hostUdp = client.hostTcp;
        client.hostUdp.sin_port = htons(udpPort);

        return true;
    }

    bool CreateClientSockets(ClientContext& client)
    {
        client.tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (client.tcpSocket == INVALID_SOCKET)
        {
            std::cerr << "TCP socket failed: " << WSAGetLastError() << "\n";
            return false;
        }

        client.udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (client.udpSocket == INVALID_SOCKET)
        {
            std::cerr << "UDP socket failed: " << WSAGetLastError() << "\n";
            return false;
        }

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = 0;

        if (bind(client.udpSocket, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR)
        {
            std::cerr << "UDP bind failed: " << WSAGetLastError() << "\n";
            return false;
        }

        if (!transport::SetNonBlocking(client.tcpSocket) || !transport::SetNonBlocking(client.udpSocket))
        {
            std::cerr << "Nonblocking setup failed: " << WSAGetLastError() << "\n";
            return false;
        }

        return true;
    }

    bool ConnectToHost(ClientContext& client)
    {
        std::cout << "[CLIENT] Host 연결 중...\n";

        const int result = connect(client.tcpSocket, reinterpret_cast<const sockaddr*>(&client.hostTcp), sizeof(client.hostTcp));

        if (result == 0)
            return true;

        const int error = WSAGetLastError();

        if (error != WSAEWOULDBLOCK)
        {
            std::cerr << "connect failed: " << error << "\n";
            return false;
        }

        fd_set writeSet;
        fd_set errorSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&errorSet);
        FD_SET(client.tcpSocket, &writeSet);
        FD_SET(client.tcpSocket, &errorSet);

        timeval timeout{};
        timeout.tv_sec = 5;

        const int selected = select(0, nullptr, &writeSet, &errorSet, &timeout);

        if (selected == SOCKET_ERROR)
        {
            std::cerr << "Connect select failed: " << WSAGetLastError() << "\n";
            return false;
        }

        if (selected == 0)
        {
            std::cerr << "TCP connect timeout\n";
            return false;
        }

        int socketError = 0;
        int optionSize = sizeof(socketError);

        if (getsockopt(client.tcpSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &optionSize) == SOCKET_ERROR)
        {
            std::cerr << "getsockopt failed: " << WSAGetLastError() << "\n";
            return false;
        }

        if (socketError != 0)
        {
            std::cerr << "TCP connect failed: " << socketError << "\n";
            return false;
        }

        return true;
    }

    void CloseClientSockets(ClientContext& client)
    {
        transport::CloseSocket(client.tcpSocket);
        transport::CloseSocket(client.udpSocket);
    }

    bool Fail(ClientContext& client, const char* reason)
    {
        std::cerr << "[FAIL] " << reason << "\n";
        client.failed = true;
        return false;
    }

    bool CheckIo(ClientContext& client, const transport::MessageResult& result)
    {
        if (result.status == transport::IoStatus::Success || result.status == transport::IoStatus::WouldBlock)
            return true;

        std::cerr << "[NETWORK] socketError=" << result.socketError << " codecError=" << static_cast<int>(result.codecError) << "\n";

        return Fail(client, result.status == transport::IoStatus::Closed ? "Host disconnected" : "Transport operation failed");
    }

    template<typename Payload>
    Message MakeMessage(ClientContext& client, MessageType type, Payload payload, std::uint32_t totalBytes)
    {
        Message message{};
        message.header.type = type;
        message.header.totalBytes = totalBytes;
        message.header.sessionId = client.session.sessionId;
        message.header.sequence = client.sequence++;
        message.payload = std::move(payload);
        return message;
    }

    bool QueueTcp(ClientContext& client, const Message& message)
    {
        return CheckIo(client, transport::QueueTcpMessage(client.tcp, message));
    }

    bool QueueControl(ClientContext& client, test_control::Command command, std::uint32_t index, bool passed = true)
    {
        return QueueTcp(client, test_control::Make(client.session, command, index, client.repeats, passed));
    }

    bool PumpNetwork(ClientContext& client)
    {
        if (_kbhit())
        {
            const int key = _getch();

            if (key == 'q' || key == 'Q')
                return Fail(client, "Cancelled by client");

            if (client.allowReady && (key == 'r' || key == 'R'))
                client.readyRequested = true;
        }

        const auto now = Clock::now();

        if (client.approved)
        {
            if (now - client.lastReceive > std::chrono::seconds(5))
                return Fail(client, "Host heartbeat timeout");

            if (now - client.lastHeartbeat >= std::chrono::seconds(1))
            {
                if (!QueueTcp(client, MakeMessage(client, MessageType::HEARTBEAT, protocol::HeartbeatPayload{}, 32)))
                    return false;

                client.lastHeartbeat = now;
            }
        }

        transport::SocketReady ready{};

        if (!transport::WaitForSockets(client.tcpSocket, client.udpSocket, !client.tcp.sendQueue.empty(), !client.udp.sendQueue.empty(), 20, ready))
        {
            std::cerr << "select failed: " << WSAGetLastError() << "\n";
            return Fail(client, "select failed");
        }

        if (ready.tcpReadable)
        {
            if (!CheckIo(client, transport::ReceiveTcpMessages(client.tcpSocket, client.tcp)))
                return false;
        }

        if (ready.tcpWritable)
        {
            if (!CheckIo(client, transport::FlushTcpSend(client.tcpSocket, client.tcp)))
                return false;
        }

        if (ready.udpReadable)
        {
            const auto result = transport::ReceiveUdpMessage(client.udpSocket, client.udp);

            if (result.status == transport::IoStatus::Error)
            {
                if (result.codecError != protocol::CodecError::NONE ||
                    result.socketError == WSAEMSGSIZE ||
                    result.socketError == WSAEPROTOTYPE)
                {
                    std::cerr << "[UDP] Invalid datagram discarded\n";
                }
                else if (!CheckIo(client, result))
                    return false;
            }
        }

        if (ready.udpWritable)
        {
            if (!CheckIo(client, transport::FlushUdpSend(client.udpSocket, client.udp)))
                return false;
        }

        while (!client.tcp.received.empty())
        {
            Message message = std::move(client.tcp.received.front());
            client.tcp.received.pop_front();

            if (message.header.roundId != 1)
                return Fail(client, "Invalid round");

            if (!client.approved)
            {
                if (message.header.type == MessageType::ACCEPT)
                {
                    const auto* accepted = std::get_if<protocol::AcceptPayload>(&message.payload);

                    if (accepted == nullptr || accepted->assignedPlayerId != 1 || accepted->maxPlayers != 2 || accepted->udpToken == 0 || message.header.sessionId == 0)
                        return Fail(client, "Invalid ACCEPT");

                    // heartbeat 발급된 세션으로 검사할 수 있게 바로 저장
                    client.playerId = accepted->assignedPlayerId;
                    client.session.sessionId = message.header.sessionId;
                    client.session.udpToken = accepted->udpToken;
                    client.approved = true;
                    client.lastHeartbeat = Clock::now();
                }
                else if (message.header.type != MessageType::REJECT)
                    return Fail(client, "Expected ACCEPT or REJECT");
            }
            else if (message.header.sessionId != client.session.sessionId)
                return Fail(client, "Invalid TCP session");

            client.lastReceive = Clock::now();

            // 반복 시험 및 시험 제어용 HEARTBEAT
            if (message.header.type == MessageType::HEARTBEAT && message.header.sequence < 0x40000000u)
                continue;

            if (client.inbox.size() >= transport::MAX_QUEUE_MESSAGES)
                return Fail(client, "Client TCP inbox overflow");

            client.inbox.push_back(std::move(message));
        }

        while (!client.udp.received.empty())
        {
            auto datagram = std::move(client.udp.received.front());
            client.udp.received.pop_front();

            if (!client.approved ||
                datagram.sender.sin_addr.s_addr !=
                client.hostUdp.sin_addr.s_addr ||
                datagram.sender.sin_port != client.hostUdp.sin_port ||
                datagram.message.header.sessionId !=
                client.session.sessionId ||
                datagram.message.header.roundId != 1)
            {
                continue;
            }

            client.lastReceive = Clock::now();

            if (client.datagrams.size() >= transport::MAX_QUEUE_MESSAGES)
                return Fail(client, "Client UDP inbox overflow");

            client.datagrams.push_back(std::move(datagram));
        }

        return true;
    }

    bool WaitUntil(ClientContext& client, const std::function<bool()>& complete, int timeoutMs = 0)
    {
        const auto started = Clock::now();

        while (!complete())
        {
            if (client.failed)
                return false;

            if (timeoutMs > 0 && Clock::now() - started >= std::chrono::milliseconds(timeoutMs))
                return Fail(client, "Stage timeout");

            if (!PumpNetwork(client))
                return false;
        }

        return !client.failed;
    }

    bool FlushPending(ClientContext& client)
    {
        return WaitUntil(
            client,
            [&]
            {
                return client.tcp.sendQueue.empty() &&
                    client.udp.sendQueue.empty();
            },
            5000);
    }

    bool WaitTcp(ClientContext& client, MessageType type, Message& out, int timeoutMs)
    {
        return WaitUntil(
            client,
            [&]
            {
                if (client.inbox.empty())
                    return false;

                out = std::move(client.inbox.front());
                client.inbox.pop_front();

                if (out.header.type != type)
                    Fail(client, "Unexpected TCP message");

                return true;
            },
            timeoutMs);
    }

    bool WaitControl(ClientContext& client, test_control::Command command, std::uint32_t index, test_control::Control& out, int timeoutMs = 5000)
    {
        return WaitUntil(
            client,
            [&]
            {
                while (!client.inbox.empty())
                {
                    Message message = std::move(client.inbox.front());
                    client.inbox.pop_front();

                    if (!test_control::Read(message, out))
                    {
                        std::cerr << "[TEST] Unexpected data between cases\n";
                        client.suitePassed = false;
                        continue;
                    }

                    if (out.command != command || out.caseIndex != index || (client.repeats != 0 && out.repeats != client.repeats))
                    {
                        Fail(client, "Unexpected test control");
                        return true;
                    }

                    // 첫 PREPARE에서 호스트가 정한 N 공유
                    if (client.repeats == 0)
                        client.repeats = out.repeats;

                    return true;
                }

                return false;
            },
            timeoutMs);
    }

    bool RequestJoin(ClientContext& client, bool expectReject, bool& rejected)
    {
        rejected = false;

        if (!QueueTcp(client, MakeMessage(client, MessageType::JOIN, protocol::JoinPayload{}, 32)))
            return false;

        std::cout << "[CLIENT] JOIN 전송 요청\n";

        Message reply;

        if (!WaitUntil(
            client,
            [&]
            {
                if (client.inbox.empty())
                    return false;

                reply = std::move(client.inbox.front());
                client.inbox.pop_front();
                return true;
            },
            5000))
        {
            return false;
        }

        if (reply.header.type == MessageType::REJECT)
        {
            const auto* payload = std::get_if<protocol::RejectPayload>(&reply.payload);

            if (payload == nullptr)
                return Fail(client, "Invalid REJECT");

            rejected = true;

            std::cout << "[CLIENT] REJECT 수신. reason=" << static_cast<int>(payload->reason) << "\n";

            if (!expectReject)
                return Fail(client, "Join rejected");

            std::cout << "[PASS] Expected REJECT received\n";
            return true;
        }

        if (reply.header.type != MessageType::ACCEPT ||
            !client.approved)
        {
            return Fail(client, "Expected ACCEPT");
        }

        if (expectReject)
            return Fail(client, "Expected REJECT but received ACCEPT");

        std::cout << "[CLIENT] ACCEPT 수신. PlayerId=" << static_cast<int>(client.playerId) << ", SessionId=" << client.session.sessionId << "\n";

        return true;
    }

    bool RegisterUdp(ClientContext& client)
    {
        protocol::UdpRegisterPayload registration{};
        registration.playerId = client.playerId;
        registration.udpToken = client.session.udpToken;

        const auto message = MakeMessage(client, MessageType::UDP_REGISTER, registration, 44);

        if (!CheckIo(client, transport::QueueUdpMessage(client.udp, client.hostUdp, message)))
            return false;

        std::cout << "[CLIENT] UDP 등록 요청\n";

        return WaitUntil(
            client,
            [&]
            {
                while (!client.datagrams.empty())
                {
                    auto datagram = std::move(client.datagrams.front());
                    client.datagrams.pop_front();

                    const auto* ack = std::get_if<protocol::UdpAckPayload>(&datagram.message.payload);

                    if (datagram.message.header.type != MessageType::UDP_ACK || ack == nullptr || ack->playerId != client.playerId || ack->registered != 1)
                    {
                        Fail(client, "Invalid UDP_ACK");
                        return true;
                    }

                    std::cout << "[CLIENT] UDP 등록 완료\n";
                    return true;
                }

                return false;
            },
            5000);
    }

    bool WaitRoomState(ClientContext& client, bool expectedReady)
    {
        Message message;

        if (!WaitTcp(client, MessageType::ROOM_STATE, message, 5000))
            return false;

        const auto* room = std::get_if<protocol::RoomStatePayload>(&message.payload);

        if (room == nullptr ||
            room->slots[0].playerId != 0 || room->slots[0].connected != 1 || room->slots[0].ready != 1 ||
            room->slots[1].playerId != client.playerId || room->slots[1].connected != 1 || room->slots[1].udpRegistered != 1 || room->slots[1].ready !=
            static_cast<std::uint8_t>(expectedReady))
        {
            return Fail(client, "Unexpected ROOM_STATE");
        }

        return true;
    }

    bool WaitAndSetReady(ClientContext& client)
    {
        if (!WaitRoomState(client, false))
            return false;

        std::cout << "R 키를 누르면 레디\n"
            << "Q 키: 취소\n";

        client.allowReady = true;

        if (!WaitUntil(client, [&] { return client.readyRequested; }))
            return false;

        client.allowReady = false;

        protocol::ReadyPayload ready{};
        ready.playerId = client.playerId;
        ready.isReady = 1;

        if (!QueueTcp(client, MakeMessage(client, MessageType::READY, ready, 36)))
        {
            return false;
        }

        if (!WaitRoomState(client, true))
            return false;

        std::cout << "Host의 시작을 대기하는 중...\n";
        return true;
    }

    bool WaitForStart(ClientContext& client)
    {
        Message load;

        // 호스트가 S 키를 누를 때까지 heartbeat를 처리하며 대기.
        if (!WaitTcp(client, MessageType::LOAD, load, 0))
            return false;

        const auto* loading = std::get_if<protocol::LoadPayload>(&load.payload);

        if (loading == nullptr || loading->stageId != 1)
            return Fail(client, "Invalid LOAD");

        std::cout << "[CLIENT] 시험용 로딩 완료\n";

        protocol::LoadedPayload loaded{};
        loaded.playerId = client.playerId;

        if (!QueueTcp(client, MakeMessage(client, MessageType::LOADED, loaded, 36)))
            return false;

        Message start;

        if (!WaitTcp(client, MessageType::START, start, 15000))
            return false;

        const auto* payload = std::get_if<protocol::StartPayload>(&start.payload);

        if (payload == nullptr ||
            payload->startHostTick == 0 ||
            payload->simRateHz != 60 ||
            payload->stateRateHz != 20 ||
            payload->inputRateHz != 30)
        {
            return Fail(client, "Invalid START");
        }

        std::cout << "[CLIENT] START 수신. 메시지별 시험 시작\n";
        return true;
    }

    bool RunSendCase(ClientContext& client, message_test::CaseState& test, std::uint32_t index)
    {
        while (test.stats.queued < test.repeats)
        {
            if (!CheckIo(client, message_test::QueueNextTestMessage(test, message_test::PeerRole::Client, client.tcp, client.udp, client.hostUdp)))
                return false;
        }

        if (!FlushPending(client))
            return false;

        if (!QueueControl(client, test_control::Command::DataSent, index))
            return false;

        test_control::Control result{};

        // 수신 측은 누락이 있으면 5초 후 결과를 보낸다.
        if (!WaitControl(client, test_control::Command::Result, index, result, 7000))
            return false;

        const auto* definition = message_test::FindCase(test.type);

        std::cout << "[" << definition->name << "]"
            << " receiver=Host "
            << (result.passed ? "PASS" : "FAIL") << "\n";

        client.suitePassed = client.suitePassed && result.passed;
        return true;
    }

    bool RunReceiveCase(
        ClientContext& client,
        message_test::CaseState& test,
        std::uint32_t index)
    {
        bool peerDataSent = false;

        if (!WaitUntil(
            client,
            [&]
            {
                while (!client.inbox.empty())
                {
                    Message message = std::move(client.inbox.front());
                    client.inbox.pop_front();

                    test_control::Control control{};

                    if (test_control::Read(message, control))
                    {
                        if (control.command != test_control::Command::DataSent || control.caseIndex != index || control.repeats != client.repeats || peerDataSent)
                        {
                            Fail(client, "Unexpected DataSent control");
                            return true;
                        }

                        peerDataSent = true;
                        continue;
                    }

                    message_test::RecordReceived(test, message, false);
                }

                while (!client.datagrams.empty())
                {
                    auto datagram = std::move(client.datagrams.front());
                    client.datagrams.pop_front();

                    message_test::RecordReceived(test, datagram.message, true);
                }

                return (peerDataSent && message_test::ReceiveComplete(test)) || message_test::TimedOut(test);
            },
            6000))
        {
            return false;
        }

        message_test::PrintResult(test, message_test::PeerRole::Client);

        if (!peerDataSent)
            return Fail(client, "Missing DataSent control");

        const bool passed = message_test::Passed(test);
        client.suitePassed = client.suitePassed && passed;

        return QueueControl(client, test_control::Command::Result, index, passed);
    }

    void DiscardLateDatagrams(ClientContext& client)
    {
        if (client.datagrams.empty())
            return;

        std::cerr << "[TEST] Late UDP datagrams: "
            << client.datagrams.size() << "\n";

        client.suitePassed = false;
        client.datagrams.clear();
    }

    bool RunMessageSuite(ClientContext& client)
    {
        using test_control::Command;

        for (std::uint32_t index = 0; index < message_test::CASES.size(); ++index)
        {
            test_control::Control prepare{};

            if (!WaitControl(client, Command::Prepare, index, prepare))
                return false;

            // 새 타입의 데이터는 PREPARED 전에는 보내지 않기로 했으므로,
            // 이 시점의 UDP 데이터는 이전 단계의 늦은 데이터다.
            DiscardLateDatagrams(client);

            const auto& definition = message_test::CASES[index];

            std::cout << "\n[TEST] " << definition.name << " x " << client.repeats << "\n";

            message_test::CaseState test;

            if (!message_test::BeginCase(test, definition.type, client.repeats, client.session))
                return Fail(client, "BeginCase failed");

            if (!QueueControl(client, Command::Prepared, index))
                return false;

            test_control::Control go{};

            if (!WaitControl(client, Command::Go, index, go))
                return false;

            test.deadline = Clock::now() + std::chrono::seconds(5);

            if (message_test::IsSender(test, message_test::PeerRole::Client))
            {
                if (!RunSendCase(client, test, index))
                    return false;
            }
            else
            {
                if (!RunReceiveCase(client, test, index))
                    return false;
            }

            test_control::Control advance{};

            if (!WaitControl(client, Command::Advance, index, advance))
                return false;

            DiscardLateDatagrams(client);

            if (!QueueControl(client, Command::Advanced, index))
                return false;
        }

        const auto finishIndex = static_cast<std::uint32_t>(message_test::CASES.size());

        test_control::Control finish{};

        if (!WaitControl(client, Command::Finish, finishIndex, finish))
            return false;

        DiscardLateDatagrams(client);

        client.suitePassed = client.suitePassed && finish.passed;

        if (!QueueControl(client, Command::Finished, finishIndex, client.suitePassed))
            return false;

        // FINISHED를 전송하면서 실제 세션 END를 기다린다.
        Message end;

        if (!WaitTcp(client, MessageType::END, end, 5000))
            return false;

        const auto* ending = std::get_if<protocol::EndPayload>(&end.payload);

        if (ending == nullptr || ending->reason != protocol::DisconnectReason::NORMAL)
            return Fail(client, "Unexpected END");

        std::cout << "\n[SUITE] " << (client.suitePassed ? "PASS" : "FAIL") << "\n";

        return client.suitePassed;
    }
}

bool RunClient(unsigned short tcpPort, unsigned short udpPort, bool expectReject)
{
    ClientContext client;

    if (!ReadHostAddress(client, tcpPort, udpPort))
        return false;

    bool passed = false;

    if (CreateClientSockets(client) && ConnectToHost(client))
    {
        std::cout << "[CLIENT] TCP 연결 완료\n";

        bool rejected = false;

        if (RequestJoin(client, expectReject, rejected))
        {
            if (rejected)
            {
                passed = true;
            }
            else if (RegisterUdp(client) && WaitAndSetReady(client) && WaitForStart(client))
            {
                passed = RunMessageSuite(client);
            }
        }
    }

    CloseClientSockets(client);
    return passed;
}
