#include "MessageTest.h"

#include <iostream>
#include <utility>

namespace message_test
{
    using protocol::MessageType;

    // 반복 시험에서는 양방향 메시지도 한 방향을 정해 시험한다.
    // HEARTBEAT의 반대 방향 등은 이후 별도 사례로 확장할 수 있다.
    const std::array<CaseDefinition, 19> CASES = { {
        { MessageType::JOIN,           "JOIN",           PeerRole::Client, 32 },
        { MessageType::ACCEPT,         "ACCEPT",         PeerRole::Host,   44 },
        { MessageType::REJECT,         "REJECT",         PeerRole::Host,   36 },
        { MessageType::READY,          "READY",          PeerRole::Client, 36 },
        { MessageType::ROOM_STATE,     "ROOM_STATE",     PeerRole::Host,   40 },
        { MessageType::LOAD,           "LOAD",           PeerRole::Host,   36 },
        { MessageType::LOADED,         "LOADED",         PeerRole::Client, 36 },
        { MessageType::START,          "START",          PeerRole::Host,   44 },
        { MessageType::UDP_REGISTER,   "UDP_REGISTER",   PeerRole::Client, 44 },
        { MessageType::UDP_ACK,        "UDP_ACK",        PeerRole::Host,   36 },
        { MessageType::PLAYER_INPUT,   "PLAYER_INPUT",   PeerRole::Client, 40 },
        { MessageType::STATE_SNAPSHOT, "STATE_SNAPSHOT", PeerRole::Host,   72 },
        { MessageType::SPAWN,          "SPAWN",          PeerRole::Host,   68 },
        { MessageType::DESPAWN,        "DESPAWN",        PeerRole::Host,   44 },
        { MessageType::COMBAT_EVENT,   "COMBAT_EVENT",   PeerRole::Host,   60 },
        { MessageType::RESULT,         "RESULT",         PeerRole::Host,   44 },
        { MessageType::HEARTBEAT,      "HEARTBEAT",      PeerRole::Client, 32 },
        { MessageType::LEAVE,          "LEAVE",          PeerRole::Client, 36 },
        { MessageType::END,            "END",            PeerRole::Host,   36 }
    } };

    namespace
    {
        // 일반 접속 단계의 순번과 구분하는 시험용 순번 영역.
        // 프로토콜의 flags나 메시지 번호를 추가하는 것은 아니다.
        constexpr std::uint32_t TEST_SEQUENCE_BASE = 0x40000000;

        std::uint32_t FirstSequence(MessageType type)
        {
            return TEST_SEQUENCE_BASE
                + (static_cast<std::uint32_t>(type) - 1)
                * MAX_REPEATS;
        }
    }

    const CaseDefinition* FindCase(MessageType type)
    {
        for (const auto& test : CASES)
        {
            if (test.type == type)
                return &test;
        }

        return nullptr;
    }

    bool BuildTestMessage( MessageType type, std::uint32_t index, const TestContext& context, protocol::Message& out)
    {
        const auto* definition = FindCase(type);

        if (definition == nullptr ||
            index >= MAX_REPEATS ||
            context.sessionId == 0 ||
            context.udpToken == 0)
        {
            return false;
        }

        protocol::Message message{};
        const std::uint32_t number = index + 1;

        message.header.type = type;
        message.header.totalBytes = definition->totalBytes;
        message.header.sessionId = type == MessageType::JOIN ? 0 : context.sessionId;
        message.header.roundId = 1;
        message.header.sequence = FirstSequence(type) + index;

        // 시험 메시지 종류 간 eventId가 겹치지 않도록 한다.
        const std::uint32_t eventId = (static_cast<std::uint32_t>(type) - 1) * MAX_REPEATS + number;

        using namespace protocol;

        switch (type)
        {
        case MessageType::JOIN:
            message.payload = JoinPayload{};
            break;

        case MessageType::ACCEPT:
        {
            AcceptPayload payload{};
            payload.assignedPlayerId = 1;
            payload.udpToken = context.udpToken;
            message.payload = payload;
            break;
        }

        case MessageType::REJECT:
        {
            RejectPayload payload{};
            payload.reason = RejectReason::ROOM_FULL;
            message.payload = payload;
            break;
        }

        case MessageType::READY:
        {
            ReadyPayload payload{};
            payload.playerId = 1;
            payload.isReady = static_cast<std::uint8_t>(number % 2);
            message.payload = payload;
            break;
        }

        case MessageType::ROOM_STATE:
        {
            RoomStatePayload payload{};
            payload.slots[0] = RoomSlotState{ 0, 1, 1, 1 };
            payload.slots[1] = RoomSlotState{ 1, 1, 1, 1 };
            message.payload = payload;
            break;
        }

        case MessageType::LOAD:
            message.payload = LoadPayload{};
            break;

        case MessageType::LOADED:
            message.payload = LoadedPayload{};
            break;

        case MessageType::START:
        {
            StartPayload payload{};
            payload.startHostTick = 1000 + number;
            message.payload = payload;
            break;
        }

        case MessageType::UDP_REGISTER:
        {
            UdpRegisterPayload payload{};
            payload.playerId = 1;
            payload.udpToken = context.udpToken;
            message.payload = payload;
            break;
        }

        case MessageType::UDP_ACK:
            message.payload = UdpAckPayload{};
            break;

        case MessageType::PLAYER_INPUT:
        {
            PlayerInputPayload payload{};
            payload.playerId = 1;
            payload.moveBits = static_cast<std::uint8_t>(
                number % (MoveBit::ALL_VALID + 1));
            payload.fireHeld =
                static_cast<std::uint8_t>(number % 2);
            payload.inputSequence = message.header.sequence;
            message.payload = payload;
            break;
        }

        case MessageType::STATE_SNAPSHOT:
        {
            StateSnapshotPayload payload{};
            payload.snapshotTick = 1000 + number;

            ObjectStateRecord object{};
            object.objectId = number;
            object.kind = NetObjectKind::PLAYER;
            object.subType = 1;
            object.pos.x = 10000 + static_cast<std::int32_t>(number);
            object.pos.y = 20000;
            object.vel.x = -100;
            object.vel.y = 200;
            object.hp = 3;
            object.maxHp = 3;

            payload.objects.push_back(object);

            message.header.hostTick = payload.snapshotTick;
            message.payload = std::move(payload);
            break;
        }

        case MessageType::SPAWN:
        {
            SpawnPayload payload{};
            payload.eventId = eventId;
            payload.objectId = number;
            payload.kind = NetObjectKind::MONSTER;
            payload.subType = 0; // NORMAL1
            payload.pos.x = 10000 + static_cast<std::int32_t>(number);
            payload.pos.y = 20000;
            payload.vel.y = 100;
            payload.hp = 100;

            message.header.hostTick = 1000 + number;
            message.payload = payload;
            break;
        }

        case MessageType::DESPAWN:
        {
            DespawnPayload payload{};
            payload.eventId = eventId;
            payload.objectId = number;
            payload.reason = DespawnReason::DESTROYED;

            message.header.hostTick = 1000 + number;
            message.payload = payload;
            break;
        }

        case MessageType::COMBAT_EVENT:
        {
            CombatEventPayload payload{};
            payload.eventId = eventId;
            payload.kind = CombatEventKind::DAMAGE_APPLIED;
            payload.instigatorPlayerId = 1;
            payload.sourceObjectId = 10000 + number;
            payload.targetObjectId = number;
            payload.confirmedHp = 2;
            payload.teamScore = number * 100;

            message.header.hostTick = 1000 + number;
            message.payload = payload;
            break;
        }

        case MessageType::RESULT:
        {
            ResultPayload payload{};
            payload.eventId = eventId;
            payload.outcome = MatchOutcome::VICTORY;
            payload.finalTeamScore = number * 100;

            message.header.hostTick = 1000 + number;
            message.payload = payload;
            break;
        }

        case MessageType::HEARTBEAT:
            message.payload = HeartbeatPayload{};
            break;

        case MessageType::LEAVE:
        {
            LeavePayload payload{};
            payload.playerId = 1;
            payload.reason = DisconnectReason::USER_LEFT;
            message.payload = payload;
            break;
        }

        case MessageType::END:
            message.payload = EndPayload{};
            break;

        default:
            return false;
        }

        out = std::move(message);
        return true;
    }

    bool BeginCase(
        CaseState& state,
        MessageType type,
        std::uint32_t repeats,
        const TestContext& context,
        int timeoutMs)
    {
        if (FindCase(type) == nullptr ||
            repeats == 0 ||
            repeats > MAX_REPEATS ||
            context.sessionId == 0 ||
            context.udpToken == 0 ||
            timeoutMs <= 0)
        {
            return false;
        }

        state = CaseState{};
        state.type = type;
        state.repeats = repeats;
        state.context = context;
        state.seen.assign(repeats, false);
        state.deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(timeoutMs);

        return true;
    }

    bool IsSender(const CaseState& state, PeerRole role)
    {
        const auto* definition = FindCase(state.type);
        return definition != nullptr && definition->sender == role;
    }

    transport::MessageResult QueueNextTestMessage(
        CaseState& state,
        PeerRole role,
        transport::TcpState& tcp,
        transport::UdpState& udp,
        const sockaddr_in& peerUdpAddress)
    {
        if (state.repeats == 0 || !IsSender(state, role))
        {
            return {
                transport::IoStatus::Error,
                WSAEINVAL,
                protocol::CodecError::NONE
            };
        }

        if (state.stats.queued >= state.repeats)
            return {};

        protocol::Message message;

        if (!BuildTestMessage(
            state.type,
            static_cast<std::uint32_t>(state.stats.queued),
            state.context,
            message))
        {
            return {
                transport::IoStatus::Error,
                WSAEINVAL,
                protocol::CodecError::NONE
            };
        }

        const auto result =
            protocol::IsUdpMessageType(state.type)
            ? transport::QueueUdpMessage(
                udp, peerUdpAddress, message)
            : transport::QueueTcpMessage(tcp, message);

        if (result.status == transport::IoStatus::Success)
            ++state.stats.queued;

        return result;
    }

    bool RecordReceived(
        CaseState& state,
        const protocol::Message& message,
        bool arrivedViaUdp)
    {
        ++state.stats.received;

        if (state.repeats == 0 ||
            message.header.type != state.type ||
            arrivedViaUdp != protocol::IsUdpMessageType(state.type))
        {
            ++state.stats.unexpected;
            return false;
        }

        const std::uint32_t first = FirstSequence(state.type);

        if (message.header.sequence < first ||
            message.header.sequence - first >= state.repeats)
        {
            ++state.stats.unexpected;
            return false;
        }

        const std::uint32_t index = message.header.sequence - first;

        if (state.seen[index])
        {
            ++state.stats.duplicates;
            return false;
        }

        state.seen[index] = true;
        ++state.stats.unique;

        protocol::Message expected;

        if (!BuildTestMessage(
            state.type,
            index,
            state.context,
            expected) ||
            !(message == expected))
        {
            ++state.stats.mismatched;
            return false;
        }

        ++state.stats.matched;
        return true;
    }

    bool ReceiveComplete(const CaseState& state)
    {
        return state.repeats > 0 &&
            state.stats.unique == state.repeats;
    }

    bool TimedOut(const CaseState& state)
    {
        return state.repeats > 0 &&
            std::chrono::steady_clock::now() >= state.deadline;
    }

    bool Passed(const CaseState& state)
    {
        return ReceiveComplete(state) &&
            state.stats.matched == state.repeats &&
            state.stats.mismatched == 0 &&
            state.stats.duplicates == 0 &&
            state.stats.unexpected == 0;
    }

    void PrintResult(const CaseState& state, PeerRole role)
    {
        const auto* definition = FindCase(state.type);

        if (state.repeats == 0 || definition == nullptr)
        {
            std::cout << "[TEST] Not initialized\n";
            return;
        }

        const bool udp = protocol::IsUdpMessageType(state.type);

        std::cout << "[" << definition->name << "] "
            << (udp ? "UDP" : "TCP");

        if (IsSender(state, role))
        {
            // 등록은 실제 전송 완료나 상대 검증 성공을 뜻하지 않는다.
            std::cout << " queued=" << state.stats.queued
                << "/" << state.repeats << "\n";
            return;
        }

        const std::size_t missing =
            state.repeats - state.stats.unique;

        const char* result =
            ReceiveComplete(state) ? (Passed(state) ? "PASS" : "FAIL")
            : TimedOut(state) ? "TIMEOUT"
            : "WAIT";

        std::cout
            << " received=" << state.stats.received
            << " matched=" << state.stats.matched
            << " mismatch=" << state.stats.mismatched
            << " duplicate=" << state.stats.duplicates
            << " unexpected=" << state.stats.unexpected
            << " missing=" << missing
            << " " << result << "\n";
    }
}
