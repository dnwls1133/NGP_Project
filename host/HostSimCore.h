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

    // 플레이어 기본 발사 간격: 0.15초 = 60Hz 기준 9틱
    inline constexpr std::uint32_t PLAYER_FIRE_INTERVAL_TICKS = 9;

    // 일반 적 기본 FSM 전환 간격: IDLE(0) -> PAROL/RUN(1) (0.5초 = 30틱)
    inline constexpr std::uint32_t MONSTER_IDLE_TO_PATROL_TICKS = 30;

    // 11/22: 보스 등장 틱(130초 = 7,800틱), 보스 subType(MON_TYPE::BOSS=6), 2페이즈 HP 경계
    inline constexpr std::uint32_t BOSS_SPAWN_TICK = 130 * SIM_TICK_HZ;
    inline constexpr std::uint8_t MONSTER_SUBTYPE_BOSS = 6;
    inline constexpr std::int32_t BOSS_PHASE2_HP_THRESHOLD = 4000;

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
    enum class InputRejectReason : std::uint8_t
    {
        None = 0,                    // 정상 채택
        OutOfRange = 1,              // playerId, moveBits, fireHeld, reserved 범위 위반
        NonMonotonicTime = 2,        // 단조 시계 수신 시각 역전 (이전 유효 시각보다 작음)
        DuplicateOrStaleSequence = 3 // 중복 또는 이전 순번 (inputSequence <= lastAcceptedSeq)
    };

    struct PlayerInputState
    {
        bool hasInput = false;
        std::uint32_t lastSequence = 0;
        std::uint64_t lastReceiveTimeMs = 0;
    };

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
        std::uint32_t stateTickAccum = 0; // 추가 (틱 기반 FSM 전환 누적기)
        bool deathConfirmed = false; // 추가 사망,점수,eventId 1회 확정 플래그
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
    // 5-1. 11/23 CHostWorldSync: 36개 단위 독립 상태 조각 분할기 (모든 조각 <= 1,200B)
    // ─────────────────────────────────────────────────────────────────────────
    class CHostWorldSync
    {
    public:
        static net::SendResult BuildSnapshotBatch(
            std::uint32_t sessionId,
            std::uint32_t hostTick,
            std::uint32_t& nextSequence,
            const std::vector<protocol::ObjectStateRecord>& records,
            net::SnapshotBatch& outBatch)
        {
            outBatch.parts.clear();

            if (sessionId == 0)
                return net::SendResult::InvalidRequest;

            constexpr std::size_t perPart = protocol::MAX_SNAPSHOT_OBJECTS_PER_PACKET; //36
            const std::size_t partCount = records.empty()
                ? 1
                : ((records.size() + perPart - 1) / perPart);

            if (partCount > net::MAX_SNAPSHOT_PARTS_PER_BATCH)
                return net::SendResult::TooLarge;

            outBatch.parts.reserve(partCount);

            for (std::size_t partIndex = 0; partIndex < partCount; ++partIndex) {
                protocol::StateSnapshotPayload snap{};
                snap.snapshotTick = hostTick;
                snap.partIndex = static_cast<std::uint8_t>(partIndex);
                snap.partCount = static_cast<std::uint8_t>(partCount);

                if (!records.empty()) {
                    const std::size_t offset = partIndex * perPart;
                    const std::size_t count = (std::min)(perPart, records.size() - offset);
                    snap.objects.assign(
                        records.begin() + static_cast<std::ptrdiff_t>(offset),
                        records.begin() + static_cast<std::ptrdiff_t>(offset + count));
                }
                protocol::Message msg{};
                msg.header.magic = protocol::MAGIC;
                msg.header.version = protocol::VERSION;
                msg.header.type = protocol::MessageType::STATE_SNAPSHOT;
                msg.header.flags = protocol::FLAGS_NONE;
                msg.header.totalBytes = static_cast<std::uint32_t>(
                     protocol::HEADER_BYTES
                    + protocol::SNAPSHOT_HEADER_BYTES
                    + (snap.objects.size() * protocol::OBJECT_STATE_RECORD_BYTES));
                msg.header.sessionId = sessionId;
                msg.header.roundId = 1;
                msg.header.sequence = nextSequence++;
                msg.header.hostTick = hostTick;
                msg.header.reserved = protocol::RESERVED_VALUE;
                msg.payload = std::move(snap);

                outBatch.parts.push_back(std::move(msg));


            }
            return net::ValidateSnapshotBatch(outBatch);
        }
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
            m_deathCandiateCount = 0;
            m_teamScore = 0;
            m_bossSpawned = false;
            m_lastAppliedMoveBits.fill(protocol::MoveBit::NONE);
            m_lastAppliedFireHeld.fill(0);
            m_fireTickAccum.fill(0);
            m_hasValidInput.fill(false);
            m_lastAcceptedInputSeq.fill(0);
            m_lastValidReceivedTimeMs.fill(0);
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
            std::int32_t damage = 1,
            std::uint8_t patternType = 0)
        {

            if (kind == protocol::NetObjectKind::PLAYER) {
                if (ownerPlayerId >= protocol::MAX_PROTOCOL_PLAYERS
                    || m_playerSlotObjectIds[ownerPlayerId] != 0) {
                    return 0; // 잘못된 플레이어 슬롯이거나 이미 등록된 슬롯 거부
                }
                for (const HostObjectValue& pending : m_pendingSpawns) {
                    if (pending.kind == protocol::NetObjectKind::PLAYER
                        && pending.ownerPlayerId == ownerPlayerId) {
                        return 0; // 같은 틱 내 중복 슬롯 생성 거부
                    }
                }
            }


            const protocol::ObjectId assignedId = m_nextObjectId++;

            HostObjectValue obj{};
            obj.objectId = assignedId;
            obj.kind = kind;
            obj.subType = subType;
            obj.ownerPlayerId = ownerPlayerId;
            obj.patternType = patternType;
            if (kind == protocol::NetObjectKind::PROJ_MONSTER)
                obj.fsmState = patternType;
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
        std::uint32_t GetDeathCandidateCount() const noexcept { return m_deathCandiateCount; }
        std::uint32_t GetTeamScore() const noexcept { return m_teamScore; }
        const std::vector<HostObjectValue>& GetActiveObjects() const noexcept { return m_objects; }

        protocol::ObjectId GetPlayerSlotObjectId(protocol::PlayerSlot slot) const noexcept
        {
            return (slot < protocol::MAX_PROTOCOL_PLAYERS) ? m_playerSlotObjectIds[slot] : 0;
        }

        const HostObjectValue* FindObjectById(protocol::ObjectId objectId) const noexcept
        {
            for (const HostObjectValue& obj : m_objects) {
                if (obj.objectId == objectId) {
                    return &obj;
                }
            }
            return nullptr;
        }

        std::uint8_t GetLastAppliedMoveBits(protocol::PlayerSlot slot) const noexcept
        {
            return (slot < protocol::MAX_PROTOCOL_PLAYERS) ? m_lastAppliedMoveBits[slot] : 0;
        }

        std::uint8_t GetLastAppliedFireHeld(protocol::PlayerSlot slot) const noexcept
        {
            return (slot < protocol::MAX_PROTOCOL_PLAYERS) ? m_lastAppliedFireHeld[slot] : 0;
        }

        bool SetObjectHp(protocol::ObjectId objectId, std::int32_t newHp) noexcept
        {
            for (HostObjectValue& obj : m_objects) {
                if (obj.objectId == objectId && !obj.pendingDestroy) {
                    obj.hp = newHp;
                    return true;
                }
             
            }
            return false;
        }

        // 11/15: 생존 타깃 선택 (가까운 생존자 우선, 동률이면 낮은 PlayerId, 생존자 없으면
        // INVALID_PLAYER_SLOT)
        protocol::PlayerSlot SelectNearestAlivePlayerId(protocol::FixedVec2 fromPos) const noexcept
        {
            protocol::PlayerSlot bestSlot = protocol::INVALID_PLAYER_SLOT;
            std::int64_t bestDistSq = 0;

            for (protocol::PlayerSlot slot = 0; slot < protocol::MAX_PROTOCOL_PLAYERS; ++slot) {
                const protocol::ObjectId objId = m_playerSlotObjectIds[slot];
                const HostObjectValue* player = FindObjectById(objId);
                if (player == nullptr || player->pendingDestroy || player->hp <= 0) {
                    continue;
                }

                const std::int64_t dx = static_cast<std::int64_t>(player->pos.x) - fromPos.x;
                const std::int64_t dy = static_cast<std::int64_t>(player->pos.y) - fromPos.y;
                const std::int64_t distSq = (dx * dx) + (dy * dy);

                if (bestSlot == protocol::INVALID_PLAYER_SLOT || distSq < bestDistSq) {
                    bestSlot = slot;
                    bestDistSq = distSq;
                }
            }
            return bestSlot;
        }

        // 11/15: 적 조준 발사 (타깃이 없으면 안전하게 발사 생략하고 0 반환)
        protocol::ObjectId TryFireEnemyAimedProjectile(protocol::ObjectId monsterId)
        {
            const HostObjectValue* monster = FindObjectById(monsterId);
            if (monster == nullptr || monster->pendingDestroy || monster->kind !=
                protocol::NetObjectKind::MONSTER || monster->hp <= 0) {
                return 0;
            }

            const protocol::PlayerSlot targetSlot = SelectNearestAlivePlayerId(monster->pos);
            if (targetSlot == protocol::INVALID_PLAYER_SLOT) {
                return 0; // 생존자 없음 -> 발사 생략
            }

            const HostObjectValue* target = FindObjectById(m_playerSlotObjectIds[targetSlot]);
            const protocol::FixedVec2 vel{
                target->pos.x - monster->pos.x,
                target->pos.y - monster->pos.y
            };
            return RequestSpawn(
                protocol::NetObjectKind::PROJ_MONSTER,
                1, // subType
                targetSlot, // ownerPlayerId
                monster->pos,
                vel,
                1,
                monster->damage);

        }

        // 11/16: 탄환 소유자·피해 정보 기반 피격 처리 (GetPlayer(0)을 읽지 않고 탄환의
        // ownerPlayerId·damage 사용)
        bool ApplyProjectileHit(
            protocol::ObjectId projectileId,
            protocol::ObjectId monsterId,
            net::ISendSink& sendSink)
        {
            HostObjectValue* proj = nullptr;
            HostObjectValue* monster = nullptr;
            for (HostObjectValue& obj : m_objects)
            {
                if (!obj.pendingDestroy)
                {
                    if (obj.objectId == projectileId && obj.kind == protocol::NetObjectKind::PROJ_PLAYER)
                        proj = &obj;
                    else if (obj.objectId == monsterId && obj.kind == protocol::NetObjectKind::MONSTER)
                        monster = &obj;
                }
            }
            if (proj == nullptr || monster == nullptr || monster->hp <= 0)
            {
                return false;
            }
            monster->hp = std::max<std::int32_t>(0, monster->hp - proj->damage);
            RequestDestroy(projectileId, protocol::DespawnReason::DESTROYED);

            // 11/17: HP==0 도달 시 사망 후보 1회 및 점수 처리 추가
            std::int32_t scoreDelta = 0;
            bool justDied = false;
            if (monster->hp == 0 && !monster->deathConfirmed) {
                monster->deathConfirmed = true;
                monster->fsmState = 2; // MON_STATE::DEAD
                justDied = true;
                ++m_deathCandiateCount;
                scoreDelta = static_cast<std::int32_t>(monster->subType) * 250;
                m_teamScore += static_cast<std::int32_t>(scoreDelta);
                RequestDestroy(monsterId, protocol::DespawnReason::DESTROYED);
            }

            protocol::Message combatMsg{};
            combatMsg.header.magic = protocol::MAGIC;
            combatMsg.header.version = protocol::VERSION;
            combatMsg.header.type = protocol::MessageType::COMBAT_EVENT;
            combatMsg.header.flags = protocol::FLAGS_NONE;
            combatMsg.header.totalBytes = 60;
            combatMsg.header.sessionId = m_sessionId;
            combatMsg.header.roundId = 1;
            combatMsg.header.sequence = m_nextSequence++;
            combatMsg.header.hostTick = m_hostTick;
            combatMsg.header.reserved = protocol::RESERVED_VALUE;
            protocol::CombatEventPayload payload{};
            payload.eventId = m_nextEventId++;
            payload.kind = protocol::CombatEventKind::DAMAGE_APPLIED;
            payload.instigatorPlayerId = proj->ownerPlayerId;
            payload.updatedPlayerLevel = 0;
            payload.reserved = 0;
            payload.sourceObjectId = projectileId;
            payload.targetObjectId = monsterId;
            payload.confirmedHp = monster->hp;
            payload.scoreDelta = scoreDelta;
            payload.teamScore = m_teamScore;
            combatMsg.payload = payload;
            sendSink.EnqueueSend(net::Channel::Tcp, combatMsg);

            if (justDied) { // 사망 확정 시 고유 eventId를 가진 ENTITY_DIED 이벤트 1회 발생
                protocol::Message diedMsg = combatMsg;
                diedMsg.header.sequence = m_nextSequence++;
                protocol::CombatEventPayload& diedPayload = payload;
                diedPayload.eventId = m_nextEventId++;
                diedPayload.kind = protocol::CombatEventKind::ENTITY_DIED;
                diedMsg.payload = diedPayload;
                sendSink.EnqueueSend(net::Channel::Tcp, diedMsg);
            }
            return true;
        }

        // 11/18: 동시 접촉을 모아 단일 획득자 결정 (충돌 순서 무관, 가까운 생존자 우선,
        // 동률시 작은 PlayerId, 동시 보상 없음)
        protocol::PlayerSlot ResolveItemPickup(
            protocol::ObjectId itemId,
            const std::vector<protocol::PlayerSlot>& touchingSlots,
            net::ISendSink& sendSink) {
            HostObjectValue* item = nullptr;
            for (HostObjectValue& obj : m_objects) {
                if (!obj.pendingDestroy && obj.objectId == itemId
                    && obj.kind == protocol::NetObjectKind::ITEM) {
                    item = &obj;
                    break;
                }
            }
            if (item == nullptr || touchingSlots.empty())
            {
                return protocol::INVALID_PLAYER_SLOT;
            }

            protocol::PlayerSlot winnerSlot = protocol::INVALID_PLAYER_SLOT;
            HostObjectValue* winnerObj = nullptr;
            std::int64_t bestDistSq = 0;

            for (const protocol::PlayerSlot slot : touchingSlots) {
                if (slot >= protocol::MAX_PROTOCOL_PLAYERS)
                    continue;

                const protocol::ObjectId playerObjId = m_playerSlotObjectIds[slot];
                HostObjectValue* candidate = nullptr;
                for (HostObjectValue& obj : m_objects)
                {
                    if (!obj.pendingDestroy && obj.objectId == playerObjId && obj.hp > 0)
                    {
                        candidate = &obj;
                        break;
                    }
                }
                if (candidate == nullptr)
                    continue;
                const std::int64_t dx = static_cast<std::int64_t>(candidate->pos.x) - item->pos.x;
                const std::int64_t dy = static_cast<std::int64_t>(candidate->pos.y) - item->pos.y;
                const std::int64_t distSq = (dx * dx) + (dy * dy);
                if (winnerObj == nullptr
                    || distSq < bestDistSq
                    || (distSq == bestDistSq && slot < winnerSlot))
                {
                    winnerSlot = slot;
                    winnerObj = candidate;
                    bestDistSq = distSq;
                }
            }
            if (winnerObj == nullptr)
            {
                return protocol::INVALID_PLAYER_SLOT;
            }
            std::int32_t scoreDelta = 0;
            if (winnerObj->subType < 5)
            {
                winnerObj->subType += 1;
            }
            else
            {
                scoreDelta = 100;
                m_teamScore += 100;
            }
            RequestDestroy(itemId, protocol::DespawnReason::COLLECTED);
            protocol::Message combatMsg{};
            combatMsg.header.magic = protocol::MAGIC;
            combatMsg.header.version = protocol::VERSION;
            combatMsg.header.type = protocol::MessageType::COMBAT_EVENT;
            combatMsg.header.flags = protocol::FLAGS_NONE;
            combatMsg.header.totalBytes = 60;
            combatMsg.header.sessionId = m_sessionId;
            combatMsg.header.roundId = 1;
            combatMsg.header.sequence = m_nextSequence++;
            combatMsg.header.hostTick = m_hostTick;
            combatMsg.header.reserved = protocol::RESERVED_VALUE;
            protocol::CombatEventPayload payload{};
            payload.eventId = m_nextEventId++;
            payload.kind = protocol::CombatEventKind::ITEM_PICKED;
            payload.instigatorPlayerId = winnerSlot;
            payload.updatedPlayerLevel = winnerObj->subType;
            payload.reserved = 0;
            payload.sourceObjectId = itemId;
            payload.targetObjectId = winnerObj->objectId;
            payload.confirmedHp = winnerObj->hp;
            payload.scoreDelta = scoreDelta;
            payload.teamScore = m_teamScore;
            combatMsg.payload = payload;
            sendSink.EnqueueSend(net::Channel::Tcp, combatMsg);
            return winnerSlot;
        }

        // 11/20 : 동시 공격 탄환들의 입력 순서가 바뀌어도 계약( 낮은 ownerPlayerId ->
        // 낮은 objectId 순)에 따라 동일한 결과를 확정
        std::size_t ResolveSimultaneousProjectileHits(
            protocol::ObjectId monsterId,
            std::vector<protocol::ObjectId> projectileIds,
            net::ISendSink& sendSink)
        {
            std::sort(
                projectileIds.begin(),
                projectileIds.end(),
                [this](protocol::ObjectId a, protocol::ObjectId b)
                {
                    const HostObjectValue* objA = FindObjectById(a);
                    const HostObjectValue* objB = FindObjectById(b);
                    const protocol::PlayerSlot ownerA = (objA != nullptr) ?
                        objA->ownerPlayerId : protocol::INVALID_PLAYER_SLOT;
                    const protocol::PlayerSlot ownerB = (objB != nullptr) ?
                        objB->ownerPlayerId : protocol::INVALID_PLAYER_SLOT;
                    if (ownerA != ownerB)
                        return ownerA < ownerB;

                    return a < b;
                });

            std::size_t appliedCount = 0;
            for (const protocol::ObjectId projId : projectileIds) {
                if (ApplyProjectileHit(projId, monsterId, sendSink)) {
                    ++appliedCount;
                }
            }
            return appliedCount;
        }

        // 11/20: 씬 전환 시 이전 씬의 미처리 지연 생성/삭제 이벤트를 폐기하여
        // 새 씬 객체에 적용 되지 않도록 보장
        void TransitionScene()
        {
            m_pendingSpawns.clear();
            m_pendingDespawns.clear();
            m_objects.clear();
            m_playerSlotObjectIds.fill(0);
            m_fireTickAccum.fill(0);
            m_bossSpawned = false;
        }

        // 11/22: 보스 등장 시간 경계(130초 = 7,800틱) 1회 생성
        protocol::ObjectId TrySpawnScheduledBoss(std::uint32_t currentTick)
        {
            if (m_bossSpawned ||currentTick < BOSS_SPAWN_TICK)
            {
                return 0;
            }

            m_bossSpawned = true;
            return RequestSpawn(
                protocol::NetObjectKind::MONSTER,
                MONSTER_SUBTYPE_BOSS,
                protocol::INVALID_PLAYER_SLOT,
                protocol::FixedVec2{ 27'000, -20'000 },
                {},
                8000,
                1);
        }

        // 11/22: 보스 HP/틱 경계 FSM 1회 전환 (전환 발생 시 true, 중복 호출 시 false)
        // fsmState: 0=IDLE, 10=MBOSS0, 11=MBOSS1, 2=DEAD
        bool EvaluateBossFsmTransition(protocol::ObjectId bossId) noexcept
        {
            for (HostObjectValue& obj : m_objects) {
                if (obj.objectId != bossId || obj.pendingDestroy
                    || obj.kind != protocol::NetObjectKind::MONSTER
                    || obj.subType != MONSTER_SUBTYPE_BOSS
                    || obj.deathConfirmed || obj.hp <= 0)
                {
                    continue;
                }

                if (obj.fsmState == 0 && obj.stateTickAccum >= MONSTER_IDLE_TO_PATROL_TICKS) {
                    obj.fsmState = 10;
                    return true;
                }
                if (obj.fsmState == 10 && obj.hp <= BOSS_PHASE2_HP_THRESHOLD) {
                    obj.fsmState = 11;
                    return true;
                }
            }

            return false;
        }



        // 11/11: 마지막 유효 입력 시각 기준 300ms 경과 시 이동·발사 중립화
        // - 경계 직전(elapsedMs == 299): 유지
        // - 정확한 경계(elapsedMs == 300) 및 직후(elapsedMs >= 301) :
        // - moveBits = 0, fireHeld = 0로 중립화
        void ApplyInputNeutralization(std::uint64_t nowMs) noexcept
        {
            for (protocol::PlayerSlot slot = 0; slot < protocol::MAX_PROTOCOL_PLAYERS
                ; ++slot)
            {
                if (m_hasValidInput[slot]
                    && nowMs >= m_lastValidReceivedTimeMs[slot]
                    && (nowMs - m_lastValidReceivedTimeMs[slot] >=
                        INPUT_NEUTRAL_TIMEOUT_MS)) {
                    m_lastAppliedMoveBits[slot] = protocol::MoveBit::NONE;
                    m_lastAppliedFireHeld[slot] = 0;
                }
            }
        }



        // 11/09: 승인 입력의 범위·수신 시각·순번 검사 및 채택 함수
        InputRejectReason SubmitPlayerInput(
            const protocol::PlayerInputPayload& payload,
            std::uint64_t receivedTimeMs) noexcept
        {
            const protocol::PlayerSlot slot = payload.playerId;
            if (slot >= protocol::MAX_PROTOCOL_PLAYERS
                || (payload.moveBits & ~protocol::MoveBit::ALL_VALID) != 0
                || payload.fireHeld > 1
                || payload.reserved != 0)
            {
                return InputRejectReason::OutOfRange;
            }

            if (m_hasValidInput[slot])
            {
                if (receivedTimeMs < m_lastValidReceivedTimeMs[slot])
                {
                    return InputRejectReason::NonMonotonicTime;
                }
                if (payload.inputSequence <= m_lastAcceptedInputSeq[slot])
                {
                    return InputRejectReason::DuplicateOrStaleSequence;
                }
            }

            m_hasValidInput[slot] = true;
            m_lastAcceptedInputSeq[slot] = payload.inputSequence;
            m_lastValidReceivedTimeMs[slot] = receivedTimeMs;
            m_lastAppliedMoveBits[slot] = payload.moveBits;
            m_lastAppliedFireHeld[slot] = payload.fireHeld;
            return InputRejectReason::None;
        }

        std::uint32_t GetLastAcceptedInputSeq(protocol::PlayerSlot slot) const noexcept
        {
            return (slot < protocol::MAX_PROTOCOL_PLAYERS) ? m_lastAcceptedInputSeq[slot] : 0;
        }

        std::uint64_t GetLastValidReceivedTimeMs(protocol::PlayerSlot slot) const noexcept
        {
            return (slot < protocol::MAX_PROTOCOL_PLAYERS) ? m_lastValidReceivedTimeMs[slot] : 0;
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
                SubmitPlayerInput(entry.payload, entry.receivedTimeMs);
            }

            ApplyInputNeutralization((static_cast<std::uint64_t>(currentTick) *
                1'000ULL) / SIM_TICK_HZ); // 11/11: 마지막 유효 입력 시각 기준 300ms 경과 시 이동·발사 중립화


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

                    if (m_lastAppliedFireHeld[obj.ownerPlayerId] != 0)
                    {
                        ++m_fireTickAccum[obj.ownerPlayerId];
                        if (m_fireTickAccum[obj.ownerPlayerId] >= PLAYER_FIRE_INTERVAL_TICKS) {
                            m_fireTickAccum[obj.ownerPlayerId] = 0;
                            RequestSpawn(
                                protocol::NetObjectKind::PROJ_PLAYER,
                                obj.subType,
                                obj.ownerPlayerId,
                                obj.pos,
                                protocol::FixedVec2{ 0, -150'000 },
                                1,
                                obj.damage);
                        
                        }
                    }

                }
                else if (obj.kind == protocol::NetObjectKind::MONSTER && obj.fsmState == 0)
                {
                    ++obj.stateTickAccum;
                    if (obj.stateTickAccum >= MONSTER_IDLE_TO_PATROL_TICKS) {
                        obj.fsmState = (obj.subType == MONSTER_SUBTYPE_BOSS) ? 10 : 1; // 30틱(0.5초) 경과 후 IDLE -> PATROL/RUN(1) 전환
                    }
                }
                else if (obj.kind == protocol::NetObjectKind::PROJ_MONSTER
                    && (obj.patternType == 2 || obj.patternType == 3))
                {
                    ++obj.stateTickAccum;
                    if (obj.patternType == 2 && obj.stateTickAccum == 30) {
                        obj.vel.x = (obj.vel.x * 3) / 2;
                        obj.vel.y = (obj.vel.y * 3) / 2;
                    }
                    else if (obj.patternType == 3 && obj.stateTickAccum == 42) {
                        obj.vel.x /= 2;
                        obj.vel.y /= 2;
                    }
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
            std::vector<protocol::ObjectStateRecord> records;
            records.reserve(m_objects.size());

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
                records.push_back(rec);
            }

            if (records.empty())
            {
                return;
            }

            
            net::SnapshotBatch batch{};
            if (CHostWorldSync::BuildSnapshotBatch(m_sessionId, currentTick, m_nextSequence, records, batch)
                == net::SendResult::Queued)
            {
                sendSink.EnqueueSnapshotBatch(batch);
            }
            
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
        std::uint32_t m_deathCandiateCount = 0;
        std::uint32_t m_teamScore = 0;
        bool m_bossSpawned = false;

        std::array<std::uint8_t, protocol::MAX_PROTOCOL_PLAYERS> m_lastAppliedMoveBits{};
        std::array<std::uint8_t, protocol::MAX_PROTOCOL_PLAYERS> m_lastAppliedFireHeld{};
        std::array<bool, protocol::MAX_PROTOCOL_PLAYERS> m_hasValidInput{};
        std::array<std::uint32_t, protocol::MAX_PROTOCOL_PLAYERS> m_lastAcceptedInputSeq{};
        std::array<std::uint64_t, protocol::MAX_PROTOCOL_PLAYERS> m_lastValidReceivedTimeMs{};
        std::array<protocol::ObjectId, protocol::MAX_PROTOCOL_PLAYERS> m_playerSlotObjectIds{};
        std::array<std::uint32_t, protocol::MAX_PROTOCOL_PLAYERS> m_fireTickAccum{};

        std::vector<HostObjectValue> m_objects;
        std::vector<HostObjectValue> m_pendingSpawns;
        std::vector<PendingDespawnEntry> m_pendingDespawns;
    };
}
