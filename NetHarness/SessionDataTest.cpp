#include "SessionDataTest.h"

#include "../Engine/define.h"
#include "../Engine/CNetworkMgr.h"
#include "../Engine/CSessionMgr.h"
#include "../shared/protocol/PacketCodec.h"
#include "../tests/CodecTests/fixtures/ProtocolV1Fixtures.h"

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
#include <array>
#include <span>

namespace session_data_test
{
    using Clock = std::chrono::steady_clock;
    using network::SessionState;

    constexpr std::uint32_t START_TICK = 120;
    constexpr std::uint32_t INPUT_COUNT = 300;
    constexpr std::uint32_t SNAPSHOT_COUNT = 200;

    constexpr std::size_t MIN_INPUT_RECEIVED = 100;
    constexpr std::size_t MIN_SNAPSHOT_RECEIVED = 50;

    constexpr std::uint32_t REPLAY_SESSION_ID = 1000;
    constexpr std::size_t REPLAY_FRAME_COUNT = 4;

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

    struct ReplayAppliedState
    {
        std::vector<std::uint32_t> ticks;
        protocol::ObjectStateRecord object{};
    };

    struct DuplicateEventState
    {
        std::map<protocol::EventId, std::size_t> applicationCounts;
        std::set<protocol::ObjectId> missiles;

        std::size_t skipped = 0;
        std::int32_t targetHp = 100;
        std::uint8_t playerLevel = 1;
        std::uint32_t teamScore = 0;

        bool targetDead = false;
        bool itemAvailable = true;
        bool finished = false;
        protocol::MatchOutcome outcome = protocol::MatchOutcome::DEFEAT;
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

    bool BuildSnapshotReplayFrames(std::array<std::vector<std::uint8_t>, REPLAY_FRAME_COUNT>& frames)
    {
        for (std::size_t index = 0; index < frames.size(); ++index)
        {
            const auto number = static_cast<std::uint32_t>(index + 1);
            const auto batch = MakeSnapshot(REPLAY_SESSION_ID, number);
            const auto error = protocol::CPacketCodec::Encode(batch.parts.front(), frames[index]);

            if (error != protocol::CodecError::NONE)
            {
                std::cerr << "[FAIL] Snapshot replay encode failed, index=" << index << ", codecError=" << static_cast<int>(error) << '\n';
                return false;
            }
        }

        return true;
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

    bool DecodeSnapshotReplay(const std::array<std::vector<std::uint8_t>, REPLAY_FRAME_COUNT>& frames, const std::array<std::size_t, REPLAY_FRAME_COUNT>& order, std::array<protocol::Message, REPLAY_FRAME_COUNT>& decoded)
    {
        for (std::size_t step = 0; step < order.size(); ++step)
        {
            const auto frameIndex = order[step];

            if (frameIndex >= frames.size())
            {
                std::cerr << "[FAIL] Invalid replay frame index=" << frameIndex << '\n';
                return false;
            }

            auto& message = decoded[step];
            const auto error = protocol::CPacketCodec::Decode(frames[frameIndex], message);

            if (error != protocol::CodecError::NONE)
            {
                std::cerr << "[FAIL] Replay decode failed, step=" << step << ", frame=" << frameIndex << ", codecError=" << static_cast<int>(error) << '\n';
                return false;
            }

            std::uint32_t number = 0;
            if (!ValidateSnapshot(message, REPLAY_SESSION_ID, number) || number != frameIndex + 1)
            {
                std::cerr << "[FAIL] Replay snapshot mismatch, step=" << step << ", frame=" << frameIndex << ", decodedNumber=" << number << '\n';
                return false;
            }
        }

        return true;
    }

    void ApplyReplaySnapshot(const protocol::StateSnapshotPayload& snapshot, std::map<protocol::ObjectId, ReplayAppliedState>& applied)
    {
        for (const auto& object : snapshot.objects)
        {
            auto& state = applied[object.objectId];

            if (!state.ticks.empty() && snapshot.snapshotTick <= state.ticks.back())
                continue;

            state.ticks.push_back(snapshot.snapshotTick);
            state.object = object;
        }
    }

    bool CheckSnapshotReplay(const std::array<std::vector<std::uint8_t>, REPLAY_FRAME_COUNT>& frames, const std::array<std::size_t, REPLAY_FRAME_COUNT>& order, const std::vector<std::uint32_t>& expectedAppliedTicks)
    {
        std::cout << "[TEST] Replay order:";
        for (const auto index : order)
            std::cout << ' ' << index;
        std::cout << '\n';

        std::array<protocol::Message, REPLAY_FRAME_COUNT> decoded{};
        if (!DecodeSnapshotReplay(frames, order, decoded))
            return false;

        std::map<protocol::ObjectId, ReplayAppliedState> applied;

        std::cout << "  Received ticks:";

        for (const auto& message : decoded)
        {
            const auto& snapshot =
                std::get<protocol::StateSnapshotPayload>(message.payload);

            std::cout << ' ' << snapshot.snapshotTick;
            ApplyReplaySnapshot(snapshot, applied);
        }

        std::cout << '\n';

        const auto finalBatch = MakeSnapshot(REPLAY_SESSION_ID, REPLAY_FRAME_COUNT);
        const auto& finalSnapshot = std::get<protocol::StateSnapshotPayload>(finalBatch.parts.front().payload);

        for (const auto& object : finalSnapshot.objects)
        {
            const auto& state = applied.at(object.objectId);

            std::cout << "  Object " << object.objectId << " applied ticks:";
            for (const auto tick : state.ticks)
                std::cout << ' ' << tick;
            std::cout << '\n';

            if (state.ticks != expectedAppliedTicks || state.object != object)
            {
                std::cerr << "[FAIL] Applied ticks or object state mismatch, objectId=" << object.objectId << '\n';
                return false;
            }
        }

        std::cout << "[PASS] Applied ticks and final positions matched\n";
        return true;
    }

    bool CheckSplitSnapshotReplay(const std::array<std::size_t, 2>& order)
    {
        const auto batch = MakeSnapshot(REPLAY_SESSION_ID, 10);
        const auto& source = batch.parts.front();
        const auto& sourceSnapshot = std::get<protocol::StateSnapshotPayload>(source.payload);

        std::array<protocol::Message, 2> expected{};
        std::array<std::vector<std::uint8_t>, 2> frames{};

        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            auto& message = expected[index];
            message.header = source.header;
            message.header.totalBytes = static_cast<std::uint32_t>(
                protocol::HEADER_BYTES +
                protocol::SNAPSHOT_HEADER_BYTES +
                protocol::OBJECT_STATE_RECORD_BYTES);

            protocol::StateSnapshotPayload snapshot{};
            snapshot.snapshotTick = sourceSnapshot.snapshotTick;
            snapshot.partIndex = static_cast<std::uint8_t>(index);
            snapshot.partCount = 2;
            snapshot.objects.push_back(sourceSnapshot.objects[index]);
            message.payload = std::move(snapshot);

            const auto error = protocol::CPacketCodec::Encode(message, frames[index]);
            if (error != protocol::CodecError::NONE)
            {
                std::cerr << "[FAIL] Split snapshot encode failed, part=" << index << ", codecError=" << static_cast<int>(error) << '\n';
                return false;
            }
        }
        std::cout << "[TEST] Split snapshot order: " << order[0] << ' ' << order[1] << '\n';

        std::map<protocol::ObjectId, ReplayAppliedState> applied;

        for (const auto index : order)
        {
            protocol::Message decoded{};
            const auto error = protocol::CPacketCodec::Decode(frames[index], decoded);

            if (error != protocol::CodecError::NONE || decoded != expected[index])
            {
                std::cerr << "[FAIL] Split snapshot decode or field mismatch, part=" << index << '\n';
                return false;
            }

            const auto& snapshot = std::get<protocol::StateSnapshotPayload>(decoded.payload);

            ApplyReplaySnapshot(snapshot, applied);
        }

        if (applied.size() != sourceSnapshot.objects.size())
        {
            std::cerr << "[FAIL] Missing objects after split snapshot replay\n";
            return false;
        }

        for (const auto& object : sourceSnapshot.objects)
        {
            const auto found = applied.find(object.objectId);
            if (found == applied.end())
            {
                std::cerr << "[FAIL] Object not applied, objectId=" << object.objectId << '\n';
                return false;
            }

            const auto& state = found->second;
            if (state.ticks != std::vector<std::uint32_t>{ sourceSnapshot.snapshotTick } ||
                state.object != object)
            {
                std::cerr << "[FAIL] Split snapshot application mismatch, objectId=" << object.objectId << '\n';
                return false;
            }

            std::cout << "  Object " << object.objectId << " applied tick=" << state.ticks.back() << '\n';
        }

        std::cout << "[PASS] Both snapshot parts applied with matching object states\n";
        return true;
    }

    bool RunSnapshotReplayTest()
    {
        std::array<std::vector<std::uint8_t>, REPLAY_FRAME_COUNT> frames{};
        if (!BuildSnapshotReplayFrames(frames))
        {
            std::cout << "[SUITE] FAIL\n";
            return false;
        }

        std::array<std::size_t, REPLAY_FRAME_COUNT> order{ 0, 1, 2, 3 };
        std::size_t total = 0;
        std::size_t passed = 0;

        do
        {
            std::vector<std::uint32_t> expectedAppliedTicks;

            for (auto current = order.begin(); current != order.end(); ++current)
            {
                const auto newest = std::max_element(order.begin(), current + 1);
                if (newest == current)
                {
                    const auto number = static_cast<std::uint32_t>(*current + 1);
                    expectedAppliedTicks.push_back(START_TICK + number * 3);
                }
            }

            ++total;

            if (CheckSnapshotReplay(frames, order, expectedAppliedTicks))
                ++passed;
        } while (std::next_permutation(order.begin(), order.end()));

        const std::array<std::array<size_t, 2>, 2> splitOrders({
            {1, 0},
            {0, 1}
            });

        for (const auto& splitOrder : splitOrders)
        {
            ++total;

            if (CheckSplitSnapshotReplay(splitOrder))
                ++passed;
        }

        std::cout << "[SUMMARY] total=" << total << " passed=" << passed << " failed=" << total - passed << '\n';

        const bool success = total == 26 && passed == total;
        std::cout << (success ? "[SUITE] PASS\n" : "[SUITE] FAIL\n");
        return success;
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

    protocol::Message MakeDespawnAfterSpawn(const protocol::Message& spawn)
    {
        const auto& spawnPayload = std::get<protocol::SpawnPayload>(spawn.payload);

        protocol::DespawnPayload payload{};
        payload.eventId = spawnPayload.eventId + 1;
        payload.objectId = spawnPayload.objectId;
        payload.reason = protocol::DespawnReason::DESTROYED;

        protocol::Message message{};
        message.header = spawn.header;
        message.header.type = protocol::MessageType::DESPAWN;
        message.header.totalBytes = 44;
        message.header.sequence = spawn.header.sequence + 1;
        message.payload = payload;

        return message;
    }

    std::array<protocol::Message, 4> MakeGameEventMessages(std::uint32_t sessionId, std::uint32_t hostTick)
    {
        std::array<protocol::Message, 4> messages{};

        // 1. 시험용 미사일 생성
        auto& spawnMessage = messages[0];
        spawnMessage = protocol::fixtures::MakeValidSpawnMessage();
        spawnMessage.header.sessionId = sessionId;
        spawnMessage.header.hostTick = hostTick;

        const auto& spawn = std::get<protocol::SpawnPayload>(spawnMessage.payload);

        // 2. 해당 미사일이 시험용 대상에 피해를 준 이벤트
        protocol::CombatEventPayload combat{};
        combat.eventId = spawn.eventId + 1;
        combat.kind = protocol::CombatEventKind::DAMAGE_APPLIED;
        combat.instigatorPlayerId = spawn.ownerPlayerId;
        combat.sourceObjectId = spawn.objectId;
        combat.targetObjectId = 100;
        combat.confirmedHp = 0;
        combat.scoreDelta = 100;
        combat.teamScore = 100;

        auto& combatMessage = messages[1];
        combatMessage.header = spawnMessage.header;
        combatMessage.header.type = protocol::MessageType::COMBAT_EVENT;
        combatMessage.header.totalBytes = 60;
        combatMessage.header.sequence = spawnMessage.header.sequence + 1;
        combatMessage.payload = combat;

        // 3. 생성했던 미사일 삭제
        auto& despawnMessage = messages[2];
        despawnMessage = MakeDespawnAfterSpawn(spawnMessage);
        despawnMessage.header.sequence = combatMessage.header.sequence + 1;

        auto& despawn = std::get<protocol::DespawnPayload>(despawnMessage.payload);
        despawn.eventId = combat.eventId + 1;

        // 4. 최종 결과
        protocol::ResultPayload result{};
        result.eventId = despawn.eventId + 1;
        result.outcome = protocol::MatchOutcome::VICTORY;
        result.finalTeamScore = combat.teamScore;

        auto& resultMessage = messages[3];
        resultMessage.header = despawnMessage.header;
        resultMessage.header.type = protocol::MessageType::RESULT;
        resultMessage.header.totalBytes = 44;
        resultMessage.header.sequence = despawnMessage.header.sequence + 1;
        resultMessage.payload = result;

        return messages;
    }

    std::array<protocol::Message, 7> MakeDuplicateEventMessages(std::uint32_t sessionId, std::uint32_t hostTick)
    {
        const auto base = MakeGameEventMessages(sessionId, hostTick);
        std::array<protocol::Message, 7> messages{};

        // 1. 미사일 생성
        messages[0] = base[0];

        const auto& spawn = std::get<protocol::SpawnPayload>(messages[0].payload);

        // 2~5. 피해·사망·아이템 획득·점수 변경
        const std::array<protocol::CombatEventKind, 4> kinds{
            protocol::CombatEventKind::DAMAGE_APPLIED,
            protocol::CombatEventKind::ENTITY_DIED,
            protocol::CombatEventKind::ITEM_PICKED,
            protocol::CombatEventKind::SCORE_CHANGED
        };

        for (std::size_t index = 0; index < kinds.size(); ++index)
        {
            protocol::CombatEventPayload payload{};
            payload.eventId = spawn.eventId + static_cast<protocol::EventId>(index) + 1;
            payload.kind = kinds[index];
            payload.instigatorPlayerId = spawn.ownerPlayerId;

            auto& message = messages[index + 1];
            message.header = base[1].header;
            message.header.sequence = messages[0].header.sequence + static_cast<std::uint32_t>(index) + 1;
            message.payload = payload;
        }

        auto& damage = std::get<protocol::CombatEventPayload>(messages[1].payload);
        damage.sourceObjectId = spawn.objectId;
        damage.targetObjectId = 100;
        damage.confirmedHp = 80;

        auto& death = std::get<protocol::CombatEventPayload>(messages[2].payload);
        death.sourceObjectId = spawn.objectId;
        death.targetObjectId = 100;
        death.confirmedHp = 0;

        auto& item = std::get<protocol::CombatEventPayload>(messages[3].payload);
        item.sourceObjectId = 200;
        item.targetObjectId = 1;
        item.updatedPlayerLevel = 2;

        auto& score = std::get<protocol::CombatEventPayload>(messages[4].payload);
        score.targetObjectId = 100;
        score.scoreDelta = 100;
        score.teamScore = 100;

        // 6. 생성했던 미사일 삭제
        messages[5] = base[2];
        messages[5].header.sequence = messages[4].header.sequence + 1;

        auto& despawn = std::get<protocol::DespawnPayload>(messages[5].payload);
        despawn.eventId = score.eventId + 1;

        // 7. 게임 결과
        messages[6] = base[3];
        messages[6].header.sequence = messages[5].header.sequence + 1;

        auto& result = std::get<protocol::ResultPayload>(messages[6].payload);
        result.eventId = despawn.eventId + 1;
        result.finalTeamScore = score.teamScore;

        return messages;
    }

    bool QueueGameEventMessages(network::CSessionMgr& session, std::span<const protocol::Message> messages, std::size_t repeatCount)
    {
        if (messages.empty() || repeatCount == 0)
            return false;

        std::size_t queued = 0;

        for (const auto& message : messages)
        {
            for (std::size_t repeat = 0; repeat < repeatCount; ++repeat)
            {
                const auto result = session.EnqueueSend(net::Channel::Tcp, message);

                if (result != net::SendResult::Queued)
                {
                    std::cerr << "[FAIL] TCP event enqueue failed"
                        << ", type=" << static_cast<int>(message.header.type)
                        << ", sequence=" << message.header.sequence
                        << ", repeat=" << repeat + 1
                        << ", queued=" << queued
                        << ", result=" << static_cast<int>(result) << '\n';
                    return false;
                }

                ++queued;

                std::cout << "[HOST] TCP event queued"
                    << ", type=" << static_cast<int>(message.header.type)
                    << ", sequence=" << message.header.sequence
                    << ", repeat=" << repeat + 1 << '/' << repeatCount
                    << ", queued=" << queued << '\n';
            }
        }

        std::cout << "[PASS] TCP events queued=" << queued << '\n';
        return true;
    }

    protocol::EventId GetGameEventId(const protocol::Message& message)
    {
        if (const auto* payload = std::get_if<protocol::SpawnPayload>(&message.payload))
            return payload->eventId;

        if (const auto* payload = std::get_if<protocol::DespawnPayload>(&message.payload))
            return payload->eventId;

        if (const auto* payload = std::get_if<protocol::CombatEventPayload>(&message.payload))
            return payload->eventId;

        if (const auto* payload = std::get_if<protocol::ResultPayload>(&message.payload))
            return payload->eventId;

        return 0;
    }

    bool ApplyDuplicateTestEvent(const protocol::Message& message, DuplicateEventState& state)
    {
        const auto eventId = GetGameEventId(message);
        if (eventId == 0)
            return false;

        if (state.applicationCounts.contains(eventId))
        {
            ++state.skipped;
            std::cout << "[SKIP] Duplicate eventId=" << eventId << '\n';
            return true;
        }

        if (const auto* spawn = std::get_if<protocol::SpawnPayload>(&message.payload))
        {
            if (!state.missiles.insert(spawn->objectId).second)
                return false;
        }
        else if (const auto* despawn = std::get_if<protocol::DespawnPayload>(&message.payload))
        {
            if (state.missiles.erase(despawn->objectId) != 1)
                return false;
        }
        else if (const auto* combat = std::get_if<protocol::CombatEventPayload>(&message.payload))
        {
            switch (combat->kind)
            {
            case protocol::CombatEventKind::DAMAGE_APPLIED:
                state.targetHp = combat->confirmedHp;
                break;

            case protocol::CombatEventKind::ENTITY_DIED:
                state.targetHp = combat->confirmedHp;
                state.targetDead = true;
                break;

            case protocol::CombatEventKind::ITEM_PICKED:
                if (!state.itemAvailable)
                    return false;

                state.itemAvailable = false;
                state.playerLevel = combat->updatedPlayerLevel;
                break;

            case protocol::CombatEventKind::SCORE_CHANGED:
                state.teamScore = combat->teamScore;
                break;

            default:
                return false;
            }
        }
        else if (const auto* result = std::get_if<protocol::ResultPayload>(&message.payload))
        {
            state.finished = true;
            state.outcome = result->outcome;
            state.teamScore = result->finalTeamScore;
        }

        ++state.applicationCounts[eventId];
        std::cout << "[APPLY] eventId=" << eventId << ", count=" << state.applicationCounts[eventId] << '\n';
        return true;
    }

    bool RunGameEventPhase(network::CSessionMgr& session, bool duplicateTest = false)
    {
        std::vector<protocol::Message> expected;

        if (duplicateTest)
        {
            const auto messages = MakeDuplicateEventMessages(session.GetSessionId(), session.GetStartInfo().startHostTick);
            expected.assign(messages.begin(), messages.end());
        }
        else
        {
            const auto messages = MakeGameEventMessages(session.GetSessionId(), session.GetStartInfo().startHostTick);
            expected.assign(messages.begin(), messages.end());
        }

        const std::size_t repeatCount = duplicateTest ? 2 : 1;
        const std::size_t expectedCount = expected.size() * repeatCount;
        const bool host = session.IsHost();

        if (host && !QueueGameEventMessages(session, expected, repeatCount))
            return false;

        std::size_t receivedCount = 0;
        DuplicateEventState duplicateState{};
        const auto deadline = Clock::now() + std::chrono::seconds(5);

        while (Clock::now() < deadline)
        {
            session.Update();
            const auto state = session.GetState();

            if (host && state == SessionState::Disconnected)
            {
                std::cout << "[HOST] Client disconnected. Check client verification result.\n";
                return true;
            }

            if (state != SessionState::Started)
            {
                const auto& event = session.GetLastNetworkEvent();

                std::cerr << "[FAIL] Session ended during event test"
                    << ", state=" << static_cast<int>(state)
                    << ", socketError=" << event.socketError
                    << ", codecError=" << static_cast<int>(event.codecError) << '\n';
                return false;
            }

            if (!host)
            {
                network::ReceivedMessage received{};

                while (session.TryReceiveGame(received))
                {
                    if (receivedCount >= expectedCount)
                    {
                        std::cerr << "[FAIL] Unexpected extra game event\n";
                        return false;
                    }

                    const auto& wanted = expected[receivedCount / repeatCount];

                    if (received.transport != network::TransportType::TCP ||
                        received.message != wanted)
                    {
                        std::cerr << "[FAIL] Game event order or fields mismatch"
                            << ", index=" << receivedCount
                            << ", expectedType=" << static_cast<int>(wanted.header.type)
                            << ", actualType=" << static_cast<int>(received.message.header.type)
                            << ", expectedSequence=" << wanted.header.sequence
                            << ", actualSequence=" << received.message.header.sequence << '\n';
                        return false;
                    }

                    if (duplicateTest && !ApplyDuplicateTestEvent(received.message, duplicateState))
                    {
                        std::cerr << "[FAIL] Duplicate event application failed, eventId=" << GetGameEventId(received.message) << '\n';
                        return false;
                    }

                    ++receivedCount;

                    std::cout << "[CLIENT] TCP event matched"
                        << ", type=" << static_cast<int>(received.message.header.type)
                        << ", sequence=" << received.message.header.sequence
                        << ", repeat=" << (receivedCount - 1) % repeatCount + 1
                        << '/' << repeatCount
                        << ", received=" << receivedCount << '\n';
                }

                if (receivedCount == expectedCount)
                {
                    if (duplicateTest)
                    {
                        bool passed =
                            duplicateState.applicationCounts.size() == expected.size() &&
                            duplicateState.skipped == expected.size() &&
                            duplicateState.missiles.empty() &&
                            duplicateState.targetHp == 0 &&
                            duplicateState.targetDead &&
                            !duplicateState.itemAvailable &&
                            duplicateState.playerLevel == 2 &&
                            duplicateState.teamScore == 100 &&
                            duplicateState.finished &&
                            duplicateState.outcome == protocol::MatchOutcome::VICTORY;

                        for (const auto& message : expected)
                        {
                            const auto eventId = GetGameEventId(message);
                            const auto found = duplicateState.applicationCounts.find(eventId);

                            if (found == duplicateState.applicationCounts.end() || found->second != 1)
                                passed = false;
                        }

                        if (!passed)
                        {
                            std::cerr << "[FAIL] Duplicate event effects or application counts mismatch\n";
                            return false;
                        }

                        std::cout << "[PASS] received=" << receivedCount << ", applied=" << duplicateState.applicationCounts.size() << ", duplicateSkipped=" << duplicateState.skipped << '\n';

                        std::cout << "[PASS] Each event applied once and final game state matched\n";
                    }
                    else
                    {
                        std::cout << "[PASS] TCP game events received in order, matched=" << receivedCount << '/' << expectedCount << '\n';
                    }

                    return true;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (host)
            std::cerr << "[FAIL] Timed out waiting for client to close\n";
        else
            std::cerr << "[FAIL] Game event receive timeout, received=" << receivedCount << '/' << expectedCount << '\n';

        return false;
    }

    bool RunSpawnPhase(network::CSessionMgr& session, bool includeDespawn = false)
    {
        auto expected = protocol::fixtures::MakeValidSpawnMessage();
        expected.header.sessionId = session.GetSessionId();
        expected.header.hostTick = session.GetStartInfo().startHostTick;

        const auto despawn = MakeDespawnAfterSpawn(expected);

        const bool host = session.IsHost();

        if (host)
        {
            const auto queueTcp = [&session](const protocol::Message& message, const char* name) {
                const auto result = session.EnqueueSend(net::Channel::Tcp, message);

                if (result != net::SendResult::Queued)
                {
                    std::cerr << "[FAIL] " << name << " enqueue failed, result=" << static_cast<int>(result) << '\n';
                    return false;
                }

                std::cout << "[HOST] TCP " << name << " queued\n";
                return true;
                };

            if (!queueTcp(expected, "SPAWN"))
                return false;

            if (includeDespawn)
            {
                if (!queueTcp(despawn, "DESPAWN"))
                    return false;
            }
        }

        const auto deadline = Clock::now() + std::chrono::seconds(5);

        bool spawnReceived = false;

        while (Clock::now() < deadline)
        {
            session.Update();
            const auto state = session.GetState();

            if (host && state == SessionState::Disconnected)
            {
                std::cout << "[HOST] Client disconnected. Check client verification result.\n";
                return true;
            }

            if (state != SessionState::Started)
            {
                const auto& event = session.GetLastNetworkEvent();
                std::cerr << "[FAIL] Session ended during SPAWN test, state=" << static_cast<int>(state) << ", socketError=" << event.socketError << ", codecError=" << static_cast<int>(event.codecError) << '\n';
                return false;
            }

            if (!host)
            {
                network::ReceivedMessage received{};
                if (session.TryReceiveGame(received))
                {
                    const auto& expectedMessage = spawnReceived ? despawn : expected;
                    const char* expectedName = spawnReceived ? "DESPAWN" : "SPAWN";

                    if (received.transport != network::TransportType::TCP || received.message != expectedMessage)
                    {
                        std::cerr << "[FAIL] Expected TCP " << expectedName << ", received type=" << static_cast<int>(received.message.header.type) << ", sequence=" << received.message.header.sequence << '\n';
                        return false;
                    }

                    if (spawnReceived)
                    {
                        std::cout << "[PASS] TCP SPAWN -> DESPAWN received in order with matching fields\n";
                        return true;
                    }

                    std::cout << "[PASS] TCP SPAWN reached client session with matching fields\n";

                    if (!includeDespawn)
                        return true;

                    spawnReceived = true;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (host)
            std::cerr << "[FAIL] Timed out waiting for client to close\n";
        else
            std::cerr << "[FAIL] Timed out waiting for TCP " << (spawnReceived ? "DESPAWN" : "SPAWN") << '\n';

        return false;
    }

    bool Run(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort, Mode mode)
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

        const auto sendInterval = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / (host ? 20.0 : 30.0)));

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

                std::cerr << "[FAIL] Session failed" << ", socketError=" << event.socketError << ", codecError=" << static_cast<int>(event.codecError) << '\n';

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

                    std::cout << "[PASS] Session started, sessionId=" << trace.sessionId << '\n';

                    if (mode != Mode::Data)
                    {
                        bool eventPassed;

                        if (mode == Mode::Events || mode == Mode::DuplicateEvents)
                            eventPassed = RunGameEventPhase(session, mode == Mode::DuplicateEvents);
                        else
                            eventPassed = RunSpawnPhase(session, mode == Mode::Lifecycle);

                        session.Stop();

                        std::cout << (eventPassed ? "[LOCAL] PASS\n" : "[LOCAL] FAIL\n");
                        return eventPassed;
                    }
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

    bool CompareDirection(const Trace& sender, const Trace& receiver, std::uint32_t expectedCount, std::size_t minimumReceived, const char* label)
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
