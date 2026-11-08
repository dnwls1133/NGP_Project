#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "../net/SendContract.h"
#include "../shared/protocol/PacketCodec.h"
#include "../shared/protocol/ProtocolTypes.h"

namespace host
{
    // ─────────────────────────────────────────────────────────────────────────
    // 1. 11/08 호스트 고정 틱·주기·한계 상수 (추진계획서 및 실행계획 2절·4절)
    // ─────────────────────────────────────────────────────────────────────────
    inline constexpr std::uint32_t SIM_TICK_HZ = 60;
    inline constexpr std::uint32_t INPUT_SEND_HZ = 30;
    inline constexpr std::uint32_t STATE_SEND_HZ = 20;

    // 60Hz 시뮬레이션에서 20Hz 상태 전송은 정확히 3틱(50ms)마다 수행한다.
    inline constexpr std::uint32_t STATE_PUBLISH_INTERVAL_TICKS = SIM_TICK_HZ / STATE_SEND_HZ; // 3

    // 한 프레임 최대 실행 틱 수 (초과 시간은 버리지 않고 잔여 누적기에 보존)
    inline constexpr std::uint32_t MAX_TICKS_PER_FRAME = 5;

    // 마지막 유효 입력 후 중립화까지 허용 시간 (300ms)
    inline constexpr std::uint64_t INPUT_NEUTRAL_TIMEOUT_MS = 300;

    // 고정 틱 시뮬레이션 dt (전역 fDT와 분리된 호스트 계산 전용 고정 시간 간격)
    inline constexpr float SIM_FIXED_DT = 1.0f / static_cast<float>(SIM_TICK_HZ);

    // 부동소수점 오차 없는 60Hz 시간 누적 단위:
    // - 1 마이크로초(us) = 60 sub-unit
    // - 1 밀리초(ms)    = 60,000 sub-unit
    // - 1 초(s)         = 60,000,000 sub-unit
    // - 1 틱(1/60초)    = 1,000,000 sub-unit (정확히 나누어떨어짐)
    inline constexpr std::uint64_t SUB_UNITS_PER_US = SIM_TICK_HZ;               // 60
    inline constexpr std::uint64_t SUB_UNITS_PER_MS = 1'000ULL * SUB_UNITS_PER_US; // 60,000
    inline constexpr std::uint64_t SUB_UNITS_PER_TICK = 1'000'000ULL;              // 16.666...ms

    // ─────────────────────────────────────────────────────────────────────────
    // 2. FakeClock: 실제 대기 없이 시험이 지정한 단조 시간을 전진시키는 가짜 시계
    // ─────────────────────────────────────────────────────────────────────────
    class FakeClock
    {
    public:
        void Reset(std::uint64_t initialTimeUs = 0) noexcept
        {
            m_nowUs = initialTimeUs;
        }

        void AdvanceUs(std::uint64_t deltaUs) noexcept
        {
            m_nowUs += deltaUs;
        }

        void AdvanceMs(std::uint64_t deltaMs) noexcept
        {
            m_nowUs += (deltaMs * 1'000ULL);
        }

        std::uint64_t NowUs() const noexcept
        {
            return m_nowUs;
        }

        std::uint64_t NowMs() const noexcept
        {
            return m_nowUs / 1'000ULL;
        }

    private:
        std::uint64_t m_nowUs = 0;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // 3. 입력·상태 선언 및 InputScript (키보드 대신 틱/시각별 플레이어 입력 주입)
    // ─────────────────────────────────────────────────────────────────────────
    struct ScheduledInputEntry
    {
        std::uint32_t targetHostTick = 1;
        std::uint64_t receivedTimeMs = 0;
        protocol::PlayerInputPayload payload{};
    };

    class InputScript
    {
    public:
        void Clear()
        {
            m_entries.clear();
        }

        void ScheduleInput(
            std::uint32_t targetHostTick,
            protocol::PlayerSlot playerId,
            std::uint8_t moveBits,
            std::uint8_t fireHeld,
            std::uint32_t inputSequence,
            std::uint64_t receivedTimeMs = 0)
        {
            ScheduledInputEntry entry{};
            entry.targetHostTick = targetHostTick;
            entry.receivedTimeMs = receivedTimeMs;
            entry.payload.playerId = playerId;
            entry.payload.moveBits = moveBits;
            entry.payload.fireHeld = fireHeld;
            entry.payload.reserved = 0;
            entry.payload.inputSequence = inputSequence;
            m_entries.push_back(entry);
        }

        std::vector<ScheduledInputEntry> ConsumeForTick(std::uint32_t hostTick)
        {
            std::vector<ScheduledInputEntry> matched;
            for (const ScheduledInputEntry& entry : m_entries)
            {
                if (entry.targetHostTick == hostTick)
                {
                    matched.push_back(entry);
                }
            }
            return matched;
        }

    private:
        std::vector<ScheduledInputEntry> m_entries;
    };

    // 호스트 권위 객체 값 상태 (렌더링·GDI·리소스 포인터 없이 순수 값으로만 구성)
    struct HostObjectValue
    {
        protocol::ObjectId objectId = 0;
        protocol::NetObjectKind kind = protocol::NetObjectKind::PLAYER;
        std::uint8_t subType = 1;
        protocol::PlayerSlot ownerPlayerId = protocol::INVALID_PLAYER_SLOT;
        std::uint8_t patternType = 0;
        std::uint8_t fsmState = 0;
        protocol::FixedVec2 pos{}; // 1/100 px
        protocol::FixedVec2 vel{}; // 1/100 px/s
        std::int32_t hp = 3;
        std::int32_t maxHp = 3;
        std::int32_t damage = 1;
        bool pendingDestroy = false; // 첫 삭제 요청 시 true -> 같은 틱 충돌/피해/조준에서 즉시 제외
    };

    // ─────────────────────────────────────────────────────────────────────────
    // 4. CaptureSink: 실제 소켓 대신 생성된 상태 배치·TCP 이벤트를 수집하는 싱크
    // ─────────────────────────────────────────────────────────────────────────
    class CaptureSink : public net::ISendSink
    {
    public:
        net::SendResult EnqueueSend(
            net::Channel channel,
            const protocol::Message& message) override
        {
            const net::SendResult res = m_fakeSink.EnqueueSend(channel, message);
            if (res == net::SendResult::Queued || res == net::SendResult::ReplacedPending)
            {
                m_capturedMessages.push_back(net::FakeSendSink::QueuedSendEntry{ channel, message });
            }
            return res;
        }

        net::SendResult EnqueueSnapshotBatch(const net::SnapshotBatch& batch) override
        {
            const net::SendResult res = m_fakeSink.EnqueueSnapshotBatch(batch);
            if (res == net::SendResult::Queued || res == net::SendResult::ReplacedPending)
            {
                m_capturedBatches.push_back(batch);
            }
            return res;
        }

        void Clear()
        {
            m_fakeSink.StartNewSession();
            m_capturedMessages.clear();
            m_capturedBatches.clear();
        }

        const std::vector<net::FakeSendSink::QueuedSendEntry>& GetCapturedMessages() const noexcept
        {
            return m_capturedMessages;
        }

        const std::vector<net::SnapshotBatch>& GetCapturedBatches() const noexcept
        {
            return m_capturedBatches;
        }

        net::FakeSendSink& GetInnerFakeSink() noexcept
        {
            return m_fakeSink;
        }

    private:
        net::FakeSendSink m_fakeSink;
        std::vector<net::FakeSendSink::QueuedSendEntry> m_capturedMessages;
        std::vector<net::SnapshotBatch> m_capturedBatches;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // 5. FrameStepReport: 한 프레임(StepFrame) 실행 결과 기록
    // ─────────────────────────────────────────────────────────────────────────
    struct FrameStepReport
    {
        std::uint32_t ticksExecuted = 0;         // 이번 프레임에 실제 실행된 고정 틱 수 (0..5)
        bool clampedByMaxTicks = false;          // 5틱 상한에 걸려 남은 시간이 이월되었는지 여부
        std::uint64_t residualSubUnits = 0;      // 다음 프레임으로 이월된 잔여 sub-unit (1,000,000 = 1틱)
        float fixedDtUsed = SIM_FIXED_DT;        // 시뮬레이션에 실제 사용된 고정 dt (항상 1/60초)
        float ignoredRenderFdt = 0.0f;           // 외부 표시용 fDT (시뮬레이션에 사용하지 않음)
        std::vector<std::uint32_t> executedHostTicks{};
    };

    // ─────────────────────────────────────────────────────────────────────────
    // 6. HostSimEngine: 고정 틱(60Hz)과 표시 분리 + 틱 종료 수명 관리 경계
    // ─────────────────────────────────────────────────────────────────────────
    class HostSimEngine
    {
    public:
        explicit HostSimEngine(std::uint32_t sessionId = 1000)
            : m_sessionId(sessionId)
        {
        }

        void ResetSession(std::uint32_t newSessionId)
        {
            m_sessionId = newSessionId;
            m_hostTick = 0;
            m_nextObjectId = 1;
            m_nextEventId = 1;
            m_nextSequence = 1;
            m_accumSubUnits = 0;
            m_clampedFrameCount = 0;
            m_lifecycleFlushCount = 0;
            m_totalDestroyedCount = 0;
            m_lastAppliedMoveBits.fill(protocol::MoveBit::NONE);
            m_lastAppliedFireHeld.fill(0);
            m_playerSlotObjectIds.fill(0);
            m_objects.clear();
            m_pendingSpawns.clear();
            m_pendingDespawns.clear();
        }

        // 생성 요청: 틱 도중 컨테이너를 재할당하지 않고 대기열에 넣은 뒤, 틱 종료 경계에서 반영
        protocol::ObjectId RequestSpawn(
            protocol::NetObjectKind kind,
            std::uint8_t subType,
            protocol::PlayerSlot ownerPlayerId,
            protocol::FixedVec2 pos,
            protocol::FixedVec2 vel = {},
            std::int32_t hp = 3,
            std::int32_t damage = 1)
        {
            const protocol::ObjectId assignedId = m_nextObjectId++;

            HostObjectValue obj{};
            obj.objectId = assignedId;
            obj.kind = kind;
            obj.subType = subType;
            obj.ownerPlayerId = ownerPlayerId;
            obj.pos = pos;
            obj.vel = vel;
            obj.hp = hp;
            obj.maxHp = hp;
            obj.damage = damage;
            obj.pendingDestroy = false;

            m_pendingSpawns.push_back(obj);
            return assignedId;
        }

        // 삭제 요청:
        // - 첫 요청 시 즉시 pendingDestroy = true로 전환하여 같은 틱의 이후 판정에서 제외
        // - 동일 객체에 대한 중복 삭제 요청은 false를 반환하고 무시함 (이중 해제/중복 DESPAWN 방지)
        bool RequestDestroy(protocol::ObjectId objectId, protocol::DespawnReason reason)
        {
            if (objectId == 0)
            {
                return false;
            }

            for (HostObjectValue& obj : m_objects)
            {
                if (obj.objectId == objectId)
                {
                    if (obj.pendingDestroy)
                    {
                        return false; // 이미 삭제 예정 상태 -> 중복 요청 무시
                    }

                    obj.pendingDestroy = true;
                    m_pendingDespawns.push_back(PendingDespawnEntry{ objectId, reason });
                    return true;
                }
            }

            return false;
        }

        // 한 프레임 전진:
        // - elapsedUs: FakeClock 또는 프레임 타이머가 측정한 경과 마이크로초
        // - externalRenderFdt: CTimeMgr의 전역 fDT 값 (표시 기록용으로만 받고 계산에는 절대 쓰지 않음)
        FrameStepReport StepFrame(
            std::uint64_t elapsedUs,
            InputScript& inputScript,
            net::ISendSink& sendSink,
            float externalRenderFdt = 0.0f)
        {
            FrameStepReport report{};
            report.fixedDtUsed = SIM_FIXED_DT;
            report.ignoredRenderFdt = externalRenderFdt;

            m_accumSubUnits += (elapsedUs * SUB_UNITS_PER_US);

            while (m_accumSubUnits >= SUB_UNITS_PER_TICK
                && report.ticksExecuted < MAX_TICKS_PER_FRAME)
            {
                m_accumSubUnits -= SUB_UNITS_PER_TICK;
                ++m_hostTick;
                ++report.ticksExecuted;
                report.executedHostTicks.push_back(m_hostTick);

                ExecuteSingleTick(m_hostTick, inputScript, sendSink);
            }

            // 5틱 실행 후에도 1틱 이상의 잔여 시간이 남아 있다면 상한 제한 발동 기록 (잔여 시간은 보존)
            if (m_accumSubUnits >= SUB_UNITS_PER_TICK)
            {
                report.clampedByMaxTicks = true;
                ++m_clampedFrameCount;
            }

            report.residualSubUnits = m_accumSubUnits;
            return report;
        }

        // 렌더링/표시 단계 모사:
        // - const 메서드로 선언하여 CScene::render와 달리 객체 삭제(erase)나 상태 변경을 절대 수행하지 않음
        std::size_t RenderPresentationOnly() const noexcept
        {
            std::size_t visibleCount = 0;
            for (const HostObjectValue& obj : m_objects)
            {
                if (!obj.pendingDestroy)
                {
                    ++visibleCount;
                }
            }
            return visibleCount;
        }

        // 조회용 접근자
        std::uint32_t GetHostTick() const noexcept { return m_hostTick; }
        std::uint64_t GetResidualSubUnits() const noexcept { return m_accumSubUnits; }
        std::uint64_t GetResidualUs() const noexcept { return m_accumSubUnits / SUB_UNITS_PER_US; }
        std::uint32_t GetClampedFrameCount() const noexcept { return m_clampedFrameCount; }
        std::uint32_t GetLifecycleFlushCount() const noexcept { return m_lifecycleFlushCount; }
        std::uint32_t GetTotalDestroyedCount() const noexcept { return m_totalDestroyedCount; }
        const std::vector<HostObjectValue>& GetActiveObjects() const noexcept { return m_objects; }

        protocol::ObjectId GetPlayerSlotObjectId(protocol::PlayerSlot slot) const noexcept
        {
            return (slot < protocol::MAX_PROTOCOL_PLAYERS) ? m_playerSlotObjectIds[slot] : 0;
        }

        std::uint8_t GetLastAppliedMoveBits(protocol::PlayerSlot slot) const noexcept
        {
            return (slot < protocol::MAX_PROTOCOL_PLAYERS) ? m_lastAppliedMoveBits[slot] : 0;
        }

        // 아직 반영되지 않은 생성 대기열을 즉시 틱 시작 전 초기 배치용으로 플러시할 때 사용
        void FlushLifecycleAtTickEnd(net::ISendSink& sendSink)
        {
            ++m_lifecycleFlushCount;

            // 1) 대기 중인 생성(Spawn) 반영 및 SPAWN(TCP) 이벤트 송신
            for (const HostObjectValue& spawned : m_pendingSpawns)
            {
                m_objects.push_back(spawned);
                if (spawned.kind == protocol::NetObjectKind::PLAYER
                    && spawned.ownerPlayerId < protocol::MAX_PROTOCOL_PLAYERS)
                {
                    m_playerSlotObjectIds[spawned.ownerPlayerId] = spawned.objectId;
                }

                protocol::Message spawnMsg{};
                spawnMsg.header.magic = protocol::MAGIC;
                spawnMsg.header.version = protocol::VERSION;
                spawnMsg.header.type = protocol::MessageType::SPAWN;
                spawnMsg.header.flags = protocol::FLAGS_NONE;
                spawnMsg.header.totalBytes = 68;
                spawnMsg.header.sessionId = m_sessionId;
                spawnMsg.header.roundId = 1;
                spawnMsg.header.sequence = m_nextSequence++;
                spawnMsg.header.hostTick = m_hostTick;
                spawnMsg.header.reserved = protocol::RESERVED_VALUE;

                protocol::SpawnPayload payload{};
                payload.eventId = m_nextEventId++;
                payload.objectId = spawned.objectId;
                payload.kind = spawned.kind;
                payload.subType = spawned.subType;
                payload.ownerPlayerId = spawned.ownerPlayerId;
                payload.patternType = spawned.patternType;
                payload.pos = spawned.pos;
                payload.vel = spawned.vel;
                payload.hp = spawned.hp;
                payload.damage = spawned.damage;
                spawnMsg.payload = payload;

                sendSink.EnqueueSend(net::Channel::Tcp, spawnMsg);
            }
            m_pendingSpawns.clear();

            // 2) 대기 중인 삭제(Despawn) 반영: 슬롯 참조 정리 + 그룹 컨테이너 제거 + DESPAWN(TCP) 이벤트 송신
            for (const PendingDespawnEntry& despawn : m_pendingDespawns)
            {
                for (protocol::PlayerSlot slot = 0; slot < protocol::MAX_PROTOCOL_PLAYERS; ++slot)
                {
                    if (m_playerSlotObjectIds[slot] == despawn.objectId)
                    {
                        m_playerSlotObjectIds[slot] = 0;
                    }
                }

                const auto prevSize = m_objects.size();
                std::erase_if(
                    m_objects,
                    [&](const HostObjectValue& obj)
                    {
                        return obj.objectId == despawn.objectId;
                    });

                if (m_objects.size() < prevSize)
                {
                    ++m_totalDestroyedCount;

                    protocol::Message despawnMsg{};
                    despawnMsg.header.magic = protocol::MAGIC;
                    despawnMsg.header.version = protocol::VERSION;
                    despawnMsg.header.type = protocol::MessageType::DESPAWN;
                    despawnMsg.header.flags = protocol::FLAGS_NONE;
                    despawnMsg.header.totalBytes = 44;
                    despawnMsg.header.sessionId = m_sessionId;
                    despawnMsg.header.roundId = 1;
                    despawnMsg.header.sequence = m_nextSequence++;
                    despawnMsg.header.hostTick = m_hostTick;
                    despawnMsg.header.reserved = protocol::RESERVED_VALUE;

                    protocol::DespawnPayload payload{};
                    payload.eventId = m_nextEventId++;
                    payload.objectId = despawn.objectId;
                    payload.reason = despawn.reason;
                    payload.reserved0 = 0;
                    payload.reserved1 = 0;
                    despawnMsg.payload = payload;

                    sendSink.EnqueueSend(net::Channel::Tcp, despawnMsg);
                }
            }
            m_pendingDespawns.clear();
        }

    private:
        struct PendingDespawnEntry
        {
            protocol::ObjectId objectId = 0;
            protocol::DespawnReason reason = protocol::DespawnReason::DESTROYED;
        };

        void ExecuteSingleTick(
            std::uint32_t currentTick,
            InputScript& inputScript,
            net::ISendSink& sendSink)
        {
            // Step 1~3: 해당 틱에 예약된 입력 수집 및 플레이어 속도 반영
            const std::vector<ScheduledInputEntry> inputs = inputScript.ConsumeForTick(currentTick);
            for (const ScheduledInputEntry& entry : inputs)
            {
                const protocol::PlayerSlot slot = entry.payload.playerId;
                if (slot < protocol::MAX_PROTOCOL_PLAYERS
                    && (entry.payload.moveBits & ~protocol::MoveBit::ALL_VALID) == 0)
                {
                    m_lastAppliedMoveBits[slot] = entry.payload.moveBits;
                    m_lastAppliedFireHeld[slot] = entry.payload.fireHeld;
                }
            }

            // Step 4: 고정 SIM_FIXED_DT(1/60초) 기준 이동 계산 (전역 fDT 미사용)
            // - 속도(vel)는 1/100 px/s 단위이므로, 60Hz 1틱당 위치 변화량은 vel / 60 이다.
            for (HostObjectValue& obj : m_objects)
            {
                if (obj.pendingDestroy)
                {
                    continue; // 삭제 예정 객체는 같은 틱 계산에서 즉시 제외
                }

                if (obj.kind == protocol::NetObjectKind::PLAYER
                    && obj.ownerPlayerId < protocol::MAX_PROTOCOL_PLAYERS)
                {
                    const std::uint8_t bits = m_lastAppliedMoveBits[obj.ownerPlayerId];
                    std::int32_t vx = 0;
                    std::int32_t vy = 0;
                    constexpr std::int32_t PLAYER_SPEED_FIXED_PER_SEC = 18'000; // 180 px/s = 18,000 (1/100 px/s), 1틱당 300 (3 px)

                    if ((bits & protocol::MoveBit::LEFT) != 0)  vx -= PLAYER_SPEED_FIXED_PER_SEC;
                    if ((bits & protocol::MoveBit::RIGHT) != 0) vx += PLAYER_SPEED_FIXED_PER_SEC;
                    if ((bits & protocol::MoveBit::UP) != 0)    vy -= PLAYER_SPEED_FIXED_PER_SEC;
                    if ((bits & protocol::MoveBit::DOWN) != 0)  vy += PLAYER_SPEED_FIXED_PER_SEC;

                    obj.vel = protocol::FixedVec2{ vx, vy };
                }

                obj.pos.x += (obj.vel.x / static_cast<std::int32_t>(SIM_TICK_HZ));
                obj.pos.y += (obj.vel.y / static_cast<std::int32_t>(SIM_TICK_HZ));
            }

            // Step 7: 틱 종료 경계에서 안전한 생성/삭제 및 슬롯 참조 정리 수행 (render가 아닌 시뮬레이션 틱 경계)
            FlushLifecycleAtTickEnd(sendSink);

            // Step 8: 20Hz 주기(3틱마다)이고 활성 객체가 존재하면 STATE_SNAPSHOT 배치 발행
            if ((currentTick % STATE_PUBLISH_INTERVAL_TICKS) == 0 && !m_objects.empty())
            {
                PublishSnapshotBatch(currentTick, sendSink);
            }
        }

        void PublishSnapshotBatch(std::uint32_t currentTick, net::ISendSink& sendSink)
        {
            protocol::StateSnapshotPayload snap{};
            snap.snapshotTick = currentTick;
            snap.partIndex = 0;
            snap.partCount = 1;

            for (const HostObjectValue& obj : m_objects)
            {
                if (obj.pendingDestroy)
                {
                    continue;
                }

                protocol::ObjectStateRecord rec{};
                rec.objectId = obj.objectId;
                rec.kind = obj.kind;
                rec.subType = obj.subType;
                rec.alive = (obj.hp > 0) ? 1 : 0;
                rec.fsmState = obj.fsmState;
                rec.pos = obj.pos;
                rec.vel = obj.vel;
                rec.hp = obj.hp;
                rec.maxHp = obj.maxHp;
                snap.objects.push_back(rec);
            }

            if (snap.objects.empty())
            {
                return;
            }

            protocol::Message msg{};
            msg.header.magic = protocol::MAGIC;
            msg.header.version = protocol::VERSION;
            msg.header.type = protocol::MessageType::STATE_SNAPSHOT;
            msg.header.flags = protocol::FLAGS_NONE;
            msg.header.totalBytes = static_cast<std::uint32_t>(
                protocol::HEADER_BYTES + 8 + (snap.objects.size() * 32));
            msg.header.sessionId = m_sessionId;
            msg.header.roundId = 1;
            msg.header.sequence = m_nextSequence++;
            msg.header.hostTick = currentTick;
            msg.header.reserved = protocol::RESERVED_VALUE;
            msg.payload = snap;

            net::SnapshotBatch batch{};
            batch.parts.push_back(std::move(msg));
            sendSink.EnqueueSnapshotBatch(batch);
        }

    private:
        std::uint32_t m_sessionId = 1000;
        std::uint32_t m_hostTick = 0;
        protocol::ObjectId m_nextObjectId = 1;
        protocol::EventId m_nextEventId = 1;
        std::uint32_t m_nextSequence = 1;
        std::uint64_t m_accumSubUnits = 0;
        std::uint32_t m_clampedFrameCount = 0;
        std::uint32_t m_lifecycleFlushCount = 0;
        std::uint32_t m_totalDestroyedCount = 0;

        std::array<std::uint8_t, protocol::MAX_PROTOCOL_PLAYERS> m_lastAppliedMoveBits{};
        std::array<std::uint8_t, protocol::MAX_PROTOCOL_PLAYERS> m_lastAppliedFireHeld{};
        std::array<protocol::ObjectId, protocol::MAX_PROTOCOL_PLAYERS> m_playerSlotObjectIds{};

        std::vector<HostObjectValue> m_objects;
        std::vector<HostObjectValue> m_pendingSpawns;
        std::vector<PendingDespawnEntry> m_pendingDespawns;
    };
}
