#include "SessionDataTest.h"

#include "../Engine/define.h"
#include "../Engine/CNetworkMgr.h"
#include "../Engine/CSessionMgr.h"
#include "../shared/protocol/PacketCodec.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace session_data_test
{
    using Clock = std::chrono::steady_clock;
    using network::SessionState;

    constexpr std::uint32_t START_TICK = 120;
    constexpr std::uint32_t INPUT_COUNT = 300;
    constexpr std::uint32_t SNAPSHOT_COUNT = 200;

    constexpr std::size_t MIN_INPUT_RECEIVED = 100;
    constexpr std::size_t MIN_SNAPSHOT_RECEIVED = 50;

    struct Record
    {
        bool sent = false;
        std::uint32_t number = 0;
        std::string bytes;
    };

    struct Trace
    {
        bool host = false;
        std::uint32_t sessionId = 0;
        bool passed = false;
        std::vector<Record> records;
    };

    struct Statistics
    {
        std::set<std::uint32_t> receivedNumbers;
        std::uint32_t latestNumber = 0;
        std::size_t duplicates = 0;
        std::size_t outOfOrder = 0;
        std::size_t queueFull = 0;
    };

    protocol::Message MakeInput(std::uint32_t sessionId, protocol::PlayerSlot playerId, std::uint32_t number)
    {
        const bool odd = number % 2 != 0;

        protocol::PlayerInputPayload payload{};
        payload.playerId = playerId;
        payload.inputSequence = number;
        payload.moveBits = odd ? protocol::MoveBit::UP : protocol::MoveBit::DOWN;
        payload.fireHeld = odd ? 1 : 0;

        protocol::Message message{};
        message.header.type = protocol::MessageType::PLAYER_INPUT;
        message.header.totalBytes = 40;
        message.header.sessionId = sessionId;
        message.header.sequence = number;
        message.payload = payload;

        return message;
    }

    net::SnapshotBatch MakeSnapshot(std::uint32_t sessionId, std::uint32_t number)
    {
        const std::uint32_t tick = START_TICK + number * 3;
        const std::int32_t y = static_cast<std::int32_t>(number % 7) * 100;

        protocol::StateSnapshotPayload payload{};
        payload.snapshotTick = tick;

        for (std::uint32_t index = 0; index < 2; ++index)
        {
            protocol::ObjectStateRecord object{};
            object.objectId = index + 1;
            object.kind = protocol::NetObjectKind::PLAYER;
            object.subType = 1;
            object.pos = protocol::FixedVec2{ index == 0 ? 300 : 3800, index == 0 ? y : 600 - y };
            object.vel = protocol::FixedVec2{ 0, index == 0 ? 100 : -100 };
            object.hp = static_cast<std::int32_t>(50 + number % 50);
            object.maxHp = 100;

            payload.objects.push_back(object);
        }

        protocol::Message message{};
        message.header.type = protocol::MessageType::STATE_SNAPSHOT;
        message.header.totalBytes = static_cast<std::uint32_t>(
            protocol::HEADER_BYTES +
            protocol::SNAPSHOT_HEADER_BYTES +
            protocol::OBJECT_STATE_RECORD_BYTES * payload.objects.size());

        message.header.sessionId = sessionId;
        message.header.sequence = number;
        message.header.hostTick = tick;
        message.payload = std::move(payload);

        net::SnapshotBatch batch{};
        batch.parts.push_back(std::move(message));
        return batch;
    }

    bool ValidateInput(const protocol::Message& message, std::uint32_t sessionId, std::uint32_t& number)
    {
        number = message.header.sequence;

        if (number == 0 || number > INPUT_COUNT)
            return false;

        protocol::PacketHeader expectedHeader{};
        expectedHeader.type = protocol::MessageType::PLAYER_INPUT;
        expectedHeader.totalBytes = 40;
        expectedHeader.sessionId = sessionId;
        expectedHeader.sequence = number;

        if (message.header != expectedHeader)
        {
            std::cerr << "[FAIL] Input header mismatch, sequence=" << number << '\n';
            return false;
        }

        const auto* input = std::get_if<protocol::PlayerInputPayload>(&message.payload);
        const bool odd = number % 2 != 0;

        if (!input ||
            input->playerId != 1 ||
            input->inputSequence != number ||
            input->moveBits != (odd ? protocol::MoveBit::UP : protocol::MoveBit::DOWN) ||
            input->fireHeld != (odd ? 1 : 0) ||
            input->reserved != 0)
        {
            std::cerr << "[FAIL] Input payload mismatch, sequence=" << number << '\n';
            return false;
        }

        return true;
    }

    bool ValidateSnapshot(const protocol::Message& message, std::uint32_t sessionId, std::uint32_t& number)
    {
        const auto tick = message.header.hostTick;

        if (tick <= START_TICK || (tick - START_TICK) % 3 != 0)
            return false;

        number = (tick - START_TICK) / 3;

        if (number == 0 || number > SNAPSHOT_COUNT)
            return false;

        protocol::PacketHeader expectedHeader{};
        expectedHeader.type = protocol::MessageType::STATE_SNAPSHOT;
        expectedHeader.totalBytes = static_cast<std::uint32_t>(
            protocol::HEADER_BYTES +
            protocol::SNAPSHOT_HEADER_BYTES +
            protocol::OBJECT_STATE_RECORD_BYTES * 2);

        expectedHeader.sessionId = sessionId;
        expectedHeader.sequence = number;
        expectedHeader.hostTick = tick;

        if (message.header != expectedHeader)
        {
            std::cerr << "[FAIL] Snapshot header mismatch, tick=" << tick << '\n';
            return false;
        }

        const auto* snapshot = std::get_if<protocol::StateSnapshotPayload>(&message.payload);

        if (!snapshot ||
            snapshot->snapshotTick != tick ||
            snapshot->partIndex != 0 ||
            snapshot->partCount != 1 ||
            snapshot->objects.size() != 2)
        {
            std::cerr << "[FAIL] Snapshot payload mismatch, tick=" << tick << '\n';
            return false;
        }

        const auto y = static_cast<std::int32_t>(number % 7) * 100;

        for (std::size_t index = 0; index < snapshot->objects.size(); ++index)
        {
            const auto& object = snapshot->objects[index];

            if (object.objectId != index + 1 ||
                object.kind != protocol::NetObjectKind::PLAYER ||
                object.subType != 1 ||
                object.alive != 1 ||
                object.fsmState != 0 ||
                object.pos.x != (index == 0 ? 300 : 3800) ||
                object.pos.y != (index == 0 ? y : 600 - y) ||
                object.vel.x != 0 ||
                object.vel.y != (index == 0 ? 100 : -100) ||
                object.hp != static_cast<std::int32_t>(50 + number % 50) ||
                object.maxHp != 100)
            {
                std::cerr
                    << "[FAIL] Object fields mismatch, tick=" << tick
                    << ", index=" << index << '\n';

                return false;
            }
        }

        return true;
    }

    bool AppendRecord(Trace& trace, bool sent, std::uint32_t number, const protocol::Message& message)
    {
        std::vector<std::uint8_t> bytes;

        if (protocol::CPacketCodec::Encode(message, bytes) != protocol::CodecError::NONE)
        {
            std::cerr << "[FAIL] Trace encoding failed\n";
            return false;
        }

        constexpr char HEX[] = "0123456789ABCDEF";

        std::string text;
        text.reserve(bytes.size() * 2);

        for (const auto byte : bytes)
        {
            text.push_back(HEX[byte >> 4]);
            text.push_back(HEX[byte & 0x0F]);
        }

        trace.records.push_back(Record{ sent, number, std::move(text) });
        return true;
    }

    bool SaveTrace(const std::string& path, const Trace& trace)
    {
        std::ofstream file(path, std::ios::trunc);

        if (!file)
        {
            std::cerr << "[FAIL] Cannot create trace: " << path << '\n';
            return false;
        }

        file
            << "SESSION_DATA_V1 "
            << (trace.host ? "HOST" : "CLIENT") << ' '
            << trace.sessionId << ' '
            << (trace.passed ? 1 : 0) << '\n';

        for (const auto& record : trace.records)
        {
            file
                << (record.sent ? "TX" : "RX") << ' '
                << record.number << ' '
                << record.bytes << '\n';
        }

        file.close();

        if (file.fail())
        {
            std::cerr << "[FAIL] Trace write failed: " << path << '\n';
            return false;
        }

        std::cout << "[PASS] Trace saved: " << path << '\n';
        return true;
    }

    bool AdvanceSession(network::CSessionMgr& session, bool& readyRequested)
    {
        switch (session.GetState())
        {
        case SessionState::UdpRegistered:
            if (session.IsHost())
            {
                if (session.IsClientReady())
                    return session.RequestLoad() == net::SendResult::Queued;
            }
            else if (!readyRequested)
            {
                if (session.RequestReady(true) != net::SendResult::Queued)
                    return false;

                readyRequested = true;
            }
            break;

        case SessionState::Loading:
            if (!session.NotifyLocalLoaded())
                return false;

            if (session.IsHost() && session.HasClientLoaded())
                return session.RequestStart(START_TICK) == net::SendResult::Queued;
            break;

        default:
            break;
        }

        return true;
    }

    bool ReceiveMessages(network::CSessionMgr& session, Trace& trace, Statistics& statistics)
    {
        network::ReceivedMessage received{};

        while (session.TryReceiveGame(received))
        {
            if (received.transport != network::TransportType::UDP)
            {
                std::cerr << "[FAIL] Unexpected transport\n";
                return false;
            }

            std::uint32_t number = 0;

            const bool valid = session.IsHost()
                ? ValidateInput(received.message, trace.sessionId, number)
                : ValidateSnapshot(received.message, trace.sessionId, number);

            if (!valid)
            {
                std::cerr
                    << "[FAIL] Received data validation failed"
                    << ", sequence=" << received.message.header.sequence
                    << ", tick=" << received.message.header.hostTick
                    << '\n';

                return false;
            }

            if (!AppendRecord(trace, false, number, received.message))
                return false;

            const bool inserted = statistics.receivedNumbers.insert(number).second;

            if (!inserted)
            {
                ++statistics.duplicates;
            }
            else
            {
                if (number < statistics.latestNumber)
                    ++statistics.outOfOrder;

                statistics.latestNumber =
                    (std::max)(statistics.latestNumber, number);
            }
        }

        return true;
    }

    bool QueueNextMessage(network::CSessionMgr& session, Trace& trace, Statistics& statistics, std::uint32_t& sentCount)
    {
        const auto number = sentCount + 1;

        protocol::Message message{};
        net::SendResult result;

        if (session.IsHost())
        {
            const auto batch = MakeSnapshot(trace.sessionId, number);
            message = batch.parts.front();
            result = session.EnqueueSnapshotBatch(batch);
        }
        else
        {
            message = MakeInput(trace.sessionId, session.GetLocalPlayerId(), number);
            result = session.EnqueueSend(net::Channel::Udp, message);
        }

        if (result == net::SendResult::QueueFull)
        {
            ++statistics.queueFull;
            return true;
        }

        if (result != net::SendResult::Queued &&
            result != net::SendResult::ReplacedPending)
        {
            std::cerr
                << "[FAIL] Enqueue failed, number=" << number
                << ", result=" << static_cast<int>(result) << '\n';

            return false;
        }

        if (!AppendRecord(trace, true, number, message))
            return false;

        ++sentCount;
        return true;
    }

    bool Run(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort)
    {
        network::CSessionMgr session(*network::CNetworkMgr::GetInst());

        const auto started = host
            ? session.StartHost(tcpPort, udpPort)
            : session.StartClient(hostIp, tcpPort, udpPort);

        if (started != network::NetworkResult::Success)
        {
            std::cerr << "[FAIL] Start session, result=" << static_cast<int>(started) << '\n';
            return false;
        }

        Trace trace{};
        trace.host = host;

        Statistics statistics{};

        bool passed = true;
        bool readyRequested = false;
        bool gameStarted = false;
        bool sendingFinished = false;

        std::uint32_t sentCount = 0;
        const std::uint32_t targetCount = host ? SNAPSHOT_COUNT : INPUT_COUNT;

        auto previousState = session.GetState();
        auto stateSince = Clock::now();
        auto gameSince = Clock::now();
        auto nextSend = Clock::now();
        auto finishedAt = Clock::now();

        const auto sendInterval =
            std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / (host ? 20.0 : 30.0)));

        std::cout << (host ? "[HOST] Waiting for Client...\n" : "[CLIENT] Connecting...\n");

        while (true)
        {
            session.Update();

            const auto now = Clock::now();
            const auto state = session.GetState();

            if (state != previousState)
            {
                std::cout << "[STATE] " << static_cast<int>(state) << '\n';
                previousState = state;
                stateSince = now;
            }

            if (state == SessionState::Failed || state == SessionState::Rejected)
            {
                const auto& event = session.GetLastNetworkEvent();

                std::cerr
                    << "[FAIL] Session failed"
                    << ", socketError=" << event.socketError
                    << ", codecError=" << static_cast<int>(event.codecError)
                    << '\n';

                passed = false;
                break;
            }

            if (state == SessionState::Disconnected)
            {
                if (!host || !gameStarted || !sendingFinished)
                {
                    std::cerr << "[FAIL] Peer disconnected before completion\n";
                    passed = false;
                }
                break;
            }

            if (!gameStarted)
            {
                if (state == SessionState::Started)
                {
                    if (session.GetStartInfo().startHostTick != START_TICK)
                    {
                        std::cerr << "[FAIL] START tick mismatch\n";
                        passed = false;
                        break;
                    }

                    gameStarted = true;
                    trace.sessionId = session.GetSessionId();
                    gameSince = now;
                    nextSend = now;

                    std::cout
                        << "[PASS] Session started, sessionId="
                        << trace.sessionId << '\n';
                }
                else
                {
                    const auto timeout = state == SessionState::Listening
                        ? std::chrono::seconds(60)
                        : std::chrono::seconds(15);

                    if (now - stateSince >= timeout ||
                        !AdvanceSession(session, readyRequested))
                    {
                        std::cerr << "[FAIL] Session preparation failed or timed out\n";
                        passed = false;
                        break;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
            }

            if (!ReceiveMessages(session, trace, statistics))
            {
                passed = false;
                break;
            }

            if (!sendingFinished && now >= nextSend)
            {
                if (!QueueNextMessage(session, trace, statistics, sentCount))
                {
                    passed = false;
                    break;
                }

                // 지연된 송신을 한꺼번에 몰아 보내지 않고 다음 송신 시점을 잡는다.
                nextSend = now + sendInterval;

                if (sentCount == targetCount)
                {
                    sendingFinished = true;
                    finishedAt = now;
                    std::cout << "[PASS] All send requests accepted: " << sentCount << '\n';
                }
            }

            if (!host && sendingFinished &&
                now - finishedAt >= std::chrono::seconds(2))
            {
                break;
            }

            if (now - gameSince >= std::chrono::seconds(25))
            {
                std::cerr << "[FAIL] Data test timeout\n";
                passed = false;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const auto minimumReceived = host
            ? MIN_INPUT_RECEIVED
            : MIN_SNAPSHOT_RECEIVED;

        if (!gameStarted ||
            sentCount != targetCount ||
            statistics.receivedNumbers.size() < minimumReceived)
        {
            std::cerr << "[FAIL] Insufficient completed sends or received messages\n";
            passed = false;
        }

        // 기록은 메모리에 보관해 두었다가 통신 종료 후 저장한다.
        session.Stop();

        trace.passed = passed;

        std::cout
            << "[SUMMARY] accepted=" << sentCount
            << ", uniqueReceived=" << statistics.receivedNumbers.size()
            << ", duplicate=" << statistics.duplicates
            << ", outOfOrder=" << statistics.outOfOrder
            << ", queueFull=" << statistics.queueFull
            << '\n';

        const std::string path = host
            ? "session-data-host.txt"
            : "session-data-client.txt";

        const bool saved = SaveTrace(path, trace);

        std::cout << (passed && saved ? "[LOCAL] PASS\n" : "[LOCAL] FAIL\n");

        return passed && saved;
    }

    bool LoadTrace(const std::string& path, bool expectedHost, Trace& out)
    {
        std::ifstream file(path);

        std::string version;
        std::string role;
        int passed = 0;
        Trace trace{};

        if (!(file >> version >> role >> trace.sessionId >> passed) ||
            version != "SESSION_DATA_V1" ||
            role != (expectedHost ? "HOST" : "CLIENT") ||
            trace.sessionId == 0 ||
            passed != 1)
        {
            std::cerr << "[FAIL] Invalid or failed trace: " << path << '\n';
            return false;
        }

        trace.host = expectedHost;
        trace.passed = true;

        std::string line;
        std::getline(file, line);

        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            std::istringstream row(line);

            std::string direction;
            std::string extra;
            Record record{};

            if (!(row >> direction >> record.number >> record.bytes) ||
                (row >> extra) ||
                (direction != "TX" && direction != "RX") ||
                record.bytes.empty() ||
                record.bytes.size() % 2 != 0 ||
                record.bytes.size() > protocol::MAX_UDP_BYTES * 2 ||
                record.bytes.find_first_not_of("0123456789ABCDEF") != std::string::npos)
            {
                std::cerr << "[FAIL] Invalid trace row: " << path << '\n';
                return false;
            }

            record.sent = direction == "TX";
            trace.records.push_back(std::move(record));

            if (trace.records.size() > 2'000)
                return false;
        }

        if (file.bad())
            return false;

        out = std::move(trace);
        return true;
    }

    bool CompareDirection(
        const Trace& sender, const Trace& receiver,
        std::uint32_t expectedCount, std::size_t minimumReceived,
        const char* label)
    {
        std::map<std::uint32_t, std::string> sent;
        std::set<std::uint32_t> received;

        for (const auto& record : sender.records)
        {
            if (!record.sent)
                continue;

            if (record.number == 0 || record.number > expectedCount ||
                !sent.emplace(record.number, record.bytes).second)
            {
                std::cerr << "[FAIL] Invalid sender records: " << label << '\n';
                return false;
            }
        }

        if (sent.size() != expectedCount)
        {
            std::cerr << "[FAIL] Sender record count mismatch: " << label << '\n';
            return false;
        }

        for (const auto& record : receiver.records)
        {
            if (record.sent)
                continue;

            const auto found = sent.find(record.number);

            if (found == sent.end() || found->second != record.bytes)
            {
                std::cerr
                    << "[FAIL] Sender/receiver mismatch: " << label
                    << ", number=" << record.number << '\n';

                return false;
            }

            received.insert(record.number);
        }

        if (received.size() < minimumReceived)
        {
            std::cerr << "[FAIL] Insufficient receiver records: " << label << '\n';
            return false;
        }

        std::cout
            << "[PASS] " << label
            << ", accepted=" << sent.size()
            << ", uniqueReceived=" << received.size()
            << ", notObserved=" << sent.size() - received.size()
            << '\n';

        return true;
    }

    bool Compare(const std::string& hostPath, const std::string& clientPath)
    {
        Trace host{};
        Trace client{};

        if (!LoadTrace(hostPath, true, host) ||
            !LoadTrace(clientPath, false, client))
        {
            return false;
        }

        if (host.sessionId != client.sessionId)
        {
            std::cerr << "[FAIL] Traces belong to different sessions\n";
            return false;
        }

        const bool inputPassed = CompareDirection(
            client, host, INPUT_COUNT, MIN_INPUT_RECEIVED,
            "Client input values preserved");

        const bool snapshotPassed = CompareDirection(
            host, client, SNAPSHOT_COUNT, MIN_SNAPSHOT_RECEIVED,
            "Host snapshot values preserved");

        const bool passed = inputPassed && snapshotPassed;

        std::cout << (passed ? "[SUITE] PASS\n" : "[SUITE] FAIL\n");
        return passed;
    }


}
