#include <cstdint>
#include <iostream>
#include <string_view>

#include "../../host/HostSimCore.h"

namespace
{
    struct TestCounters
    {
        int passed = 0;
        int failed = 0;
    };

    void Check(bool condition, std::string_view name, TestCounters& counters)
    {
        if (condition)
        {
            ++counters.passed;
            std::cout << "[PASS] " << name << '\n';
        }
        else
        {
            ++counters.failed;
            std::cout << "[FAIL] " << name << '\n';
        }
    }

    void RunHostSim1108Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. 100ms 주입 시 첫 프레임 최대 5틱 실행 및 잔여 시간(1틱 = 1,000,000 sub-unit) 보존 확인
        {
            FakeClock clock;
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine(1000);

            clock.AdvanceMs(100); // 100ms = 100,000us (60Hz 기준 정확히 6틱 분량)
            const FrameStepReport frame1 = engine.StepFrame(clock.NowUs(), script, sink, 0.100f);

            const bool frame1Ok =
                (frame1.ticksExecuted == MAX_TICKS_PER_FRAME) // 최대 5틱까지만 실행
                && frame1.clampedByMaxTicks
                && (engine.GetHostTick() == 5)
                && (frame1.residualSubUnits == SUB_UNITS_PER_TICK) // 정확히 1틱 분량(16.666...ms) 보존
                && (engine.GetResidualUs() == 16'666);

            // 추가 경과 시간(0us) 없이 두 번째 프레임을 돌리면 보존된 잔여 시간으로 6번째 틱이 실행되어야 함
            const FrameStepReport frame2 = engine.StepFrame(0, script, sink, 0.0f);
            const bool frame2Ok =
                (frame2.ticksExecuted == 1)
                && !frame2.clampedByMaxTicks
                && (engine.GetHostTick() == 6)
                && (frame2.residualSubUnits == 0);

            Check(frame1Ok && frame2Ok,
                "HostSim1108: 100ms injection runs max 5 ticks in frame 1 and preserves 1-tick residual for frame 2",
                counters);
        }

        // 2. 부분 프레임 누적 확인: 10ms + 10ms = 20ms 주입 시 1틱(16.666...ms) 실행 후 3.333...ms(200,000 sub-unit) 보존
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine(1000);

            const FrameStepReport stepA = engine.StepFrame(10'000, script, sink, 0.010f); // 10ms (600,000 sub-unit)
            const FrameStepReport stepB = engine.StepFrame(10'000, script, sink, 0.010f); // +10ms = 20ms (1,200,000 sub-unit)

            Check(stepA.ticksExecuted == 0
                && stepA.residualSubUnits == 600'000ULL
                && stepB.ticksExecuted == 1
                && stepB.residualSubUnits == 200'000ULL
                && engine.GetHostTick() == 1,
                "HostSim1108: Sub-tick frame deltas (10ms + 10ms) accumulate without precision loss",
                counters);
        }

        // 3. 전역 fDT 분리 검증: 서로 다른 렌더 FPS(외부 fDT=0.25f vs 0.004f)에서도 동일 시간·입력 주입 시 권위 위치 100% 일치
        {
            InputScript scriptA;
            InputScript scriptB;
            scriptA.ScheduleInput(1, 0, MoveBit::RIGHT, 1, 1, 16);
            scriptB.ScheduleInput(1, 0, MoveBit::RIGHT, 1, 1, 16);

            CaptureSink sinkA;
            CaptureSink sinkB;
            HostSimEngine engineA(1000);
            HostSimEngine engineB(1000);

            engineA.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 20'000 });
            engineB.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 20'000 });
            engineA.FlushLifecycleAtTickEnd(sinkA); // 1틱 시작 전 초기 플레이어 경기장 배치 확정
            engineB.FlushLifecycleAtTickEnd(sinkB); // 1틱 시작 전 초기 플레이어 경기장 배치 확정


            // engineA는 50ms(3틱)를 한 번에(외부 fDT=0.5f), engineB는 25ms씩 두 번에(외부 fDT=0.001f) 진행
            engineA.StepFrame(50'000, scriptA, sinkA, 0.500f);
            engineB.StepFrame(25'000, scriptB, sinkB, 0.001f);
            engineB.StepFrame(25'000, scriptB, sinkB, 0.001f);

            const bool sameTick = (engineA.GetHostTick() == 3 && engineB.GetHostTick() == 3);
            const bool sameObjects =
                (engineA.GetActiveObjects().size() == 1)
                && (engineB.GetActiveObjects().size() == 1)
                && (engineA.GetActiveObjects()[0].pos == engineB.GetActiveObjects()[0].pos)
                && (engineA.GetActiveObjects()[0].pos.x == 10'900); // 10,000 + 3틱 * 300(3px/tick) = 10,900

            Check(sameTick && sameObjects,
                "HostSim1108: Fixed tick simulation ignores global render fDT and produces identical authoritative state",
                counters);
        }

        // 4. FakeClock + InputScript + CaptureSink 연결 검증: 100ms(6틱) 동안 3틱·6틱에서 20Hz SnapshotBatch 2회 수집
        {
            FakeClock clock;
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine(1000);

            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 5'000, 5'000 });
            engine.FlushLifecycleAtTickEnd(sink); // 1틱 시작 전 초기 플레이어 경기장 배치 확정

            script.ScheduleInput(1, 0, MoveBit::UP, 0, 1, 0);
            script.ScheduleInput(4, 0, MoveBit::DOWN, 1, 2, 50);

            clock.AdvanceMs(50); // 첫 50ms -> 3틱 (tick 1, 2, 3)
            engine.StepFrame(50'000, script, sink);

            clock.AdvanceMs(50); // 다음 50ms -> 3틱 (tick 4, 5, 6)
            engine.StepFrame(50'000, script, sink);

            const auto& batches = sink.GetCapturedBatches();
            bool batchesOk = (batches.size() == 2);
            if (batchesOk)
            {
                const auto& snap1 = std::get<StateSnapshotPayload>(batches[0].parts[0].payload);
                const auto& snap2 = std::get<StateSnapshotPayload>(batches[1].parts[0].payload);
                batchesOk =
                    (snap1.snapshotTick == 3 && snap1.objects.size() == 1 && snap1.objects[0].objectId == p0Id)
                    && (snap2.snapshotTick == 6 && snap2.objects.size() == 1 && snap2.objects[0].objectId == p0Id)
                    && (snap1.objects[0].pos.y == 4'100)  // 5,000 - 3 * 300 = 4,100
                    && (snap2.objects[0].pos.y == 5'000); // 4,100 + 3 * 300 = 5,000
            }

            Check(batchesOk,
                "HostSim1108: FakeClock + InputScript + CaptureSink captures 20Hz SnapshotBatches at tick 3 and tick 6",
                counters);
        }

        // 5. 안전한 생성/삭제 경계 검증: render가 아닌 틱 종료 경계에서만 제거되며, 중복 삭제 요청은 1회만 처리됨
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine(1000);

            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 1'000, 1'000 });
            const ObjectId monId = engine.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT, FixedVec2{ 2'000, 2'000 });

            // 1틱 진행하여 두 객체 생성 확정
            engine.StepFrame(16'667, script, sink);
            const bool spawnedBoth =
                (engine.GetActiveObjects().size() == 2)
                && (engine.GetPlayerSlotObjectId(0) == p0Id);

            // 같은 틱 도중 p0Id에 대해 삭제 요청을 2번 호출 -> 첫 번째만 true, 두 번째는 false여야 함
            const bool firstDestroyAccepted = engine.RequestDestroy(p0Id, DespawnReason::DESTROYED);
            const bool secondDestroyIgnored = !engine.RequestDestroy(p0Id, DespawnReason::DESTROYED);

            // 아직 다음 틱 종료 전이므로 m_objects 크기는 2이지만, RenderPresentationOnly()에서는 pendingDestroy가 제외되어 1개만 보임
            const bool renderDoesNotErase =
                (engine.RenderPresentationOnly() == 1)
                && (engine.GetActiveObjects().size() == 2);

            // 다음 1틱 진행 -> 틱 종료 경계(FlushLifecycleAtTickEnd)에서 실제 제거 및 플레이어 슬롯 0 정리, DESPAWN 1회 발행
            engine.StepFrame(16'667, script, sink);

            std::size_t despawnEventCount = 0;
            for (const auto& entry : sink.GetCapturedMessages())
            {
                if (entry.message.header.type == MessageType::DESPAWN)
                {
                    ++despawnEventCount;
                }
            }

            const bool cleanedAtTickEnd =
                (engine.GetActiveObjects().size() == 1)
                && (engine.GetActiveObjects()[0].objectId == monId)
                && (engine.GetPlayerSlotObjectId(0) == 0)
                && (engine.GetTotalDestroyedCount() == 1)
                && (despawnEventCount == 1);

            Check(spawnedBoth
                && firstDestroyAccepted
                && secondDestroyIgnored
                && renderDoesNotErase
                && cleanedAtTickEnd,
                "HostSim1108: Duplicate destroy is ignored, render does not mutate container, and tick-end flushes slot/object once",
                counters);
        }
    }

    void RunHostSim1109Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. 순번 검사: sequence 10 -> 9 -> 10 -> 11 주입 시 10과 11만 채택
        {
            HostSimEngine engine{ 1000 };

            const auto r10 = engine.SubmitPlayerInput(
                PlayerInputPayload{ 0, MoveBit::LEFT, 0,0,10 }, 100);
            const auto r9 = engine.SubmitPlayerInput(
                PlayerInputPayload{ 0, MoveBit::RIGHT, 1,0,9 }, 110);
            const auto r10Dup = engine.SubmitPlayerInput(
                PlayerInputPayload{ 0, MoveBit::UP, 1,0,10 }, 120);
            const auto r11 = engine.SubmitPlayerInput(
                PlayerInputPayload{ 0, MoveBit::DOWN, 1,0,11 }, 130);

            Check(r10 == InputRejectReason::None
                && r9 == InputRejectReason::DuplicateOrStaleSequence
                && r10Dup == InputRejectReason::DuplicateOrStaleSequence
                && r11 == InputRejectReason::None
                && engine.GetLastAcceptedInputSeq(0) == 11
                && engine.GetLastAppliedMoveBits(0) == MoveBit::DOWN
                && engine.GetLastValidReceivedTimeMs(0) == 130,
                "HostSim1109: Sequence 10->9->10->11 accepts only 10 and 11",
                counters);

        }

        // 2. 범위 및 단조 수신 시각 검사: 오류 입력 거부 및 기존 유호 상태 보존
        {
            HostSimEngine engine{ 1000 };
            engine.SubmitPlayerInput(
                PlayerInputPayload{ 1, MoveBit::UP,1,0,5 }, 200);

            const auto badSlot = engine.SubmitPlayerInput(
                PlayerInputPayload{ 2, MoveBit::LEFT, 0,0,6 }, 210);
            const auto badMove = engine.SubmitPlayerInput(
                PlayerInputPayload{ 1, 0x10, 0, 0, 6 }, 210);
            const auto badFire = engine.SubmitPlayerInput(
                PlayerInputPayload{ 1, MoveBit::DOWN, 2, 0, 6 }, 210);
            const auto badTime = engine.SubmitPlayerInput(
                PlayerInputPayload{ 1, MoveBit::DOWN, 0, 0, 6 }, 199);

            Check(badSlot == InputRejectReason::OutOfRange
                && badMove == InputRejectReason::OutOfRange
                && badFire == InputRejectReason::OutOfRange
                && badTime == InputRejectReason::NonMonotonicTime
                && engine.GetLastAcceptedInputSeq(1) == 5
                && engine.GetLastAppliedMoveBits(1) == MoveBit::UP
                && engine.GetLastValidReceivedTimeMs(1) == 200,
                "HostSim1109: Out-of-range fields and non-monotonic receive time are rejected without mutating state",
                counters);
        }
    }

    void RunHostSim1110Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. 두 플레이어(P0, P1) 생성·슬롯 등록·ID 검증 및 잘못된/중복 슬롯 거부
        {
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0,
                FixedVec2{ 10'000, 50'000 });
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1,
                FixedVec2{ 20'000, 50'000 });
            const ObjectId dupP0 = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0,
                FixedVec2{ 15'000, 50'000 });
            const ObjectId badSlot = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 2,
                FixedVec2{ 30'000, 50'000 });

            engine.FlushLifecycleAtTickEnd(sink);

            Check(p0Id != 0
                && p1Id != 0
                && p0Id != p1Id
                && dupP0 == 0
                && badSlot == 0
                && engine.GetPlayerSlotObjectId(0) == p0Id
                && engine.GetPlayerSlotObjectId(1) == p1Id,
                "HostSim1110: Spawns P0/P1 with distinct ObjectIds and rejects invalid or duplicate player slots",
                counters);
        }

        // 2. 두 입력 스트림 독립 이동: P0 오른쪽(RIGHT), P1 왼쪽(LEFT) 입력 시 독립 이동 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0,
                FixedVec2{ 10'000, 50'000 });
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1,
                FixedVec2{ 20'000, 50'000 });
            engine.FlushLifecycleAtTickEnd(sink);

            script.ScheduleInput(1, 0, MoveBit::RIGHT, 0, 1, 10);
            script.ScheduleInput(1, 1, MoveBit::LEFT, 0, 1, 10);

            engine.StepFrame(50'000, script, sink); // 50ms = 3틱 (1틱당 300 이동 -> 3틱 = 900)

            const HostObjectValue* p0 = engine.FindObjectById(p0Id);
            const HostObjectValue* p1 = engine.FindObjectById(p1Id);

            Check(p0 != nullptr
                && p1 != nullptr
                && p0->pos.x == 10'900 // 10,000 + 900 (오른쪽)
                && p1->pos.x == 19'100 // 20,000 - 900 (왼쪽)
                && p0->pos.y == 50'000
                && p1->pos.y == 50'000,
                "HostSim1110: P0 moves right and P1 moves left independently from two input streams",
                counters);
        }
    }

    void RunHostSim1111Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. 300ms 중립화 경계(직전 299ms / 정확한 경계 300ms / 직후 301ms) 및 무효·역순 입력 타이머 연장 불가 확인
        {
            HostSimEngine engine{ 1000 };
            // t = 100ms에 유효 입력 채택 (RIGHT + 발사 유지)
            engine.SubmitPlayerInput(PlayerInputPayload{ 0, MoveBit::RIGHT, 1, 0, 10 }, 100);
            // t = 250ms에 역순(seq=9) 및 범위 오류(moveBits=0x80) 주입 -> 거부되어 타이머(100ms)가 연장되지 않아야 함
            engine.SubmitPlayerInput(PlayerInputPayload{ 0, MoveBit::LEFT, 1, 0, 9 }, 250);
            engine.SubmitPlayerInput(PlayerInputPayload{ 0, 0x80, 1, 0, 11 }, 250);
            // 경계 직전: t = 399ms (경과 299ms) -> 입력 유지
            engine.ApplyInputNeutralization(399);
            const bool beforeBoundaryOk =
                (engine.GetLastAppliedMoveBits(0) == MoveBit::RIGHT)
                && (engine.GetLastAppliedFireHeld(0) == 1)
                && (engine.GetLastValidReceivedTimeMs(0) == 100);
            // 정확한 경계: t = 400ms (경과 300ms) -> 즉시 중립화 (moveBits = 0, fireHeld = 0)
            engine.ApplyInputNeutralization(400);
            const bool exactBoundaryOk =
                (engine.GetLastAppliedMoveBits(0) == MoveBit::NONE)
                && (engine.GetLastAppliedFireHeld(0) == 0);
            // 경계 직후: t = 401ms (경과 301ms) -> 중립 상태 유지
            engine.ApplyInputNeutralization(401);
            const bool afterBoundaryOk =
                (engine.GetLastAppliedMoveBits(0) == MoveBit::NONE)
                && (engine.GetLastAppliedFireHeld(0) == 0);
            Check(beforeBoundaryOk && exactBoundaryOk && afterBoundaryOk,
                "HostSim1111: 300ms neutralization holds at 299ms, clears at 300ms/301ms, and ignores invalid inputs",
                counters);
        }
        // 2. 기본 상태(위치/HP/alive) 추출 및 단일 조각 배치 미전송 교체 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 20'000 }, {}, 3);
            engine.FlushLifecycleAtTickEnd(sink);
            script.ScheduleInput(1, 0, MoveBit::RIGHT, 1, 1, 0);
            // 첫 50ms(tick 3): 단일 조각 상태 배치 등록 (pos.x = 10,900, hp = 3)
            engine.StepFrame(50'000, script, sink);
            // P0 피격 가정(HP 3 -> 1) 후 다음 50ms(tick 6) 진행 -> 미전송 대기 배치가 최신 tick 6 배치로 교체됨
            engine.SetObjectHp(p0Id, 1);
            engine.StepFrame(50'000, script, sink);
            const auto pending = sink.GetInnerFakeSink().TakePendingBatchForSend();
            bool replacedOk = pending.has_value() && (pending->parts.size() == 1);
            if (replacedOk)
            {
                const auto& snap = std::get<StateSnapshotPayload>(pending->parts[0].payload);
                replacedOk =
                    (snap.snapshotTick == 6)
                    && (snap.partIndex == 0 && snap.partCount == 1)
                    && (snap.objects.size() == 1)
                    && (snap.objects[0].objectId == p0Id)
                    && (snap.objects[0].pos.x == 11'800) // 10,000 + 6틱 * 300 = 11,800
                    && (snap.objects[0].hp == 1)
                    && (snap.objects[0].alive == 1);
            }
            Check(replacedOk,
                "HostSim1111: Extracts position/HP state and replaces unsent single-part SnapshotBatch with latest tick",
                counters);
        }
    }

    void RunHostSim1112Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. ObjectId 할당/조회/제거 및 같은 세션 내 ID 재사용 금지 확인
        {
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            const ObjectId id1 = engine.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT,
                FixedVec2{ 1'000, 1'000 });
            const ObjectId id2 = engine.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT,
                FixedVec2{ 2'000, 2'000 });
            engine.FlushLifecycleAtTickEnd(sink);

            // id1 삭제 후 새 객체 id3 생성 -> 삭제된 id1(1)을 재사용하지 않고 3이 할당되어야 함
            engine.RequestDestroy(id1, DespawnReason::DESTROYED);
            engine.FlushLifecycleAtTickEnd(sink);

            const ObjectId id3 = engine.RequestSpawn(NetObjectKind::ITEM, 1, INVALID_PLAYER_SLOT,
                FixedVec2{ 3'000, 3'000 });
            engine.FlushLifecycleAtTickEnd(sink);

            Check(id1 == 1 && id2 == 2 && id3 == 3
                && engine.FindObjectById(id1) == nullptr
                && engine.FindObjectById(id2) != nullptr
                && engine.FindObjectById(id3) != nullptr,
                "HostSim1112: ObjectId asslocation/lookup/removal never reuses destryoed IDs in the same session",
                counters);

        }

        // 2. 순회 중 해제 없음, 중복 삭제 요청시 해제 1회, 플레이어 슬롯 참조 정리 확인
        {
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER
                , 1, 0, FixedVec2{ 10'000, 20'000 });
            engine.FlushLifecycleAtTickEnd(sink);

            // 활성 객체 순회 도중 동일 객체 삭제 요청 2회 -> 첫 요청만 채택되고 순회 중 컨테이너 크기는 유지됨
            bool firstReq = false;
            bool secondReq = true;
            for (const HostObjectValue& obj : engine.GetActiveObjects()) {
                if (obj.objectId == p0Id) {
                    firstReq = engine.RequestDestroy(obj.objectId, DespawnReason::DESTROYED);
                    secondReq = engine.RequestDestroy(obj.objectId, DespawnReason::DESTROYED);
                }
            }
            const bool keptDuringIteration = (engine.GetActiveObjects().size() == 1);

            // 틱 종료 경계에서 1회만 해제되고 슬롯 참조가 0으로 정리되며, 바로 새 P0 생성이 가능해야 함.
            engine.FlushLifecycleAtTickEnd(sink);
            const ObjectId newP0Id = engine.RequestSpawn(NetObjectKind::PLAYER,
                1, 0, FixedVec2{ 10'000, 20'000 });
            engine.FlushLifecycleAtTickEnd(sink);

            Check(firstReq && !secondReq && keptDuringIteration
                && engine.GetTotalDestroyedCount() == 1
                && newP0Id == 2
                && engine.GetPlayerSlotObjectId(0) == newP0Id,
                "HostSim1112: Duplicate destroy frees once at tick-end without mutating during iteration and clears player slot",
                counters);

        }
    }

    void RunHostSim1113Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. 서로 다른 플레이어(P0 damamge=2, P1 damage=5)의 발사자 ID와 피해량 보존 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 50'000 }, {}, 3,2);
            engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 50'000 }, {}, 3,5);
            engine.FlushLifecycleAtTickEnd(sink);

            script.ScheduleInput(1, 0, MoveBit::RIGHT, 1, 1, 0);
            script.ScheduleInput(1, 1, MoveBit::LEFT, 1, 1, 0);

            // 150ms(9틱)를 75ms 두 프레임으로 진행 -> 9번째 틱에서 각 플레이어 탄환 1발씩 생성
            engine.StepFrame(75'000, script, sink);
            engine.StepFrame(75'000, script, sink);

            int p0ProjDamage = 0;
            int p1ProjDamage = 0;
            for (const HostObjectValue& obj : engine.GetActiveObjects()) {
                if (obj.kind == NetObjectKind::PROJ_PLAYER) {
                    if (obj.ownerPlayerId == 0) p0ProjDamage = obj.damage;
                    if (obj.ownerPlayerId == 1) p1ProjDamage = obj.damage;
                }
            }
            Check(p0ProjDamage == 2 && p1ProjDamage == 5,
                "HostSim1113: Preserves distinct ownerPlayerId and damage (P0=2, P1=5) on spawned projectiles",
                counters);
        }

        // 2. 같은 시간(300ms = 18틱) 입력에서 화면 FPS(externalRenderFdt)와 무한한 발사 수(2발) 확인
        {
            InputScript scriptA;
            InputScript scriptB;
            scriptA.ScheduleInput(1, 0, MoveBit::NONE, 1, 1, 16);
            scriptB.ScheduleInput(1, 0, MoveBit::NONE, 1, 1, 16);

            CaptureSink sinkA;
            CaptureSink sinkB;
            HostSimEngine engineA{ 1000 };
            HostSimEngine engineB{ 1000 };

            engineA.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 50'000 }, {}, 3, 2);
            engineB.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 50'000 }, {}, 3, 2);
            engineA.FlushLifecycleAtTickEnd(sinkA);
            engineB.FlushLifecycleAtTickEnd(sinkB);

            // engineA: 75ms씩 4번 (총 300ms = 18틱, 외부 fDT = 0.25f)
            for (int i = 0; i < 4; ++i) {
                engineA.StepFrame(75'000, scriptA, sinkA, 0.25f);
            }
            // engineB: 50ms씩 6번 (총 300ms = 18틱, 외부 fDT = 0.005f)
            for (int i = 0; i < 6; ++i) {
                engineB.StepFrame(50'000, scriptB, sinkB, 0.005f);
            }

            int countA = 0;
            int countB = 0;
            for (const HostObjectValue& obj : engineA.GetActiveObjects())
                if (obj.kind == NetObjectKind::PROJ_PLAYER) ++countA;
            for (const HostObjectValue& obj : engineB.GetActiveObjects())
                if (obj.kind == NetObjectKind::PROJ_PLAYER) ++countB;

            Check(countA == 2 && countB == 2,
                "HostSim1113: Produces identical projectile count (2 shots in 300ms) regardless of render FPS",
                counters);
        }
    }

    void RunHostSim1114Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. P0·P1 동시 이동·발사(9틱=150ms) 후 틱/ID/HP/발사 일치 및 P1 입력 단절(300ms 경과) 후 중립화 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 50'000 }, {}, 3, 2);
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 50'000 }, {}, 3, 2);
            engine.FlushLifecycleAtTickEnd(sink);
            script.ScheduleInput(1, 0, MoveBit::RIGHT, 1, 1, 16);
            script.ScheduleInput(1, 1, MoveBit::LEFT, 1, 1, 16);
            script.ScheduleInput(10, 0, MoveBit::RIGHT, 1, 2, 166);
            engine.StepFrame(75'000, script, sink);
            engine.StepFrame(75'000, script, sink);
            const HostObjectValue* p0After9 = engine.FindObjectById(p0Id);
            const HostObjectValue* p1After9 = engine.FindObjectById(p1Id);
            const bool syncedAtTick9 = (engine.GetHostTick() == 9)
                && (p0After9 != nullptr && p1After9 != nullptr)
                && (p0After9->hp == 3 && p1After9->hp == 3)
                && (p0After9->pos.x == 12'700 && p1After9->pos.x == 17'300);
            engine.StepFrame(83'334, script, sink);
            engine.StepFrame(83'334, script, sink);
            Check(syncedAtTick9
                && engine.GetHostTick() == 19
                && engine.GetLastAppliedMoveBits(0) == MoveBit::RIGHT
                && engine.GetLastAppliedFireHeld(0) == 1
                && engine.GetLastAppliedMoveBits(1) == MoveBit::NONE
                && engine.GetLastAppliedFireHeld(1) == 0,
                "HostSim1114: Verifies P0/P1 tick/ID/HP/fire parity and neutralizes disconnected P1 after 300ms",
                counters);
        }
    }

    void RunHostSim1115Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. 일반 적 FSM의 고정 틱(30틱 = 500ms) 전환 확인 (29틱에는 IDLE=0, 30틱에 PATROL/RUN=1)
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId monId = engine.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT, FixedVec2{ 20'000, 10'000 }, {}, 20, 1);
            engine.FlushLifecycleAtTickEnd(sink);
            // 29틱(483,334us) 진행 -> 아직 IDLE(fsmState == 0)
            for (int i = 0; i < 5; ++i) engine.StepFrame(83'334, script, sink); // 25틱
            engine.StepFrame(66'667, script, sink);                             // +4틱 = 29틱
            const std::uint8_t stateAt29 = engine.FindObjectById(monId)->fsmState;
            // +1틱(16,667us) 진행 -> 총 30틱에서 fsmState == 1 전환
            engine.StepFrame(16'667, script, sink);
            const std::uint8_t stateAt30 = engine.FindObjectById(monId)->fsmState;
            Check(stateAt29 == 0 && stateAt30 == 1,
                "HostSim1115: Transitions enemy FSM on exact fixed host tick (tick 29=IDLE, tick 30=PATROL/RUN)",
                counters);
        }
        // 2. 생존 타깃 선택: 가까운 생존자 우선, 동률 시 낮은 PlayerId(P0), 전원 사망 시 발사 안전 생략
        {
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 3, 2);
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 3, 2);
            const ObjectId monId = engine.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT, FixedVec2{ 18'000, 30'000 }, {}, 20, 1);
            engine.FlushLifecycleAtTickEnd(sink);
            // (a) 적 위치(18,000)에서 P1(20,000, 거리 2,000)이 P0(10,000, 거리 8,000)보다 가까움 -> P1(1) 선택
            const PlayerSlot nearestWhenBothAlive = engine.SelectNearestAlivePlayerId(FixedVec2{ 18'000, 30'000 });
            // (b) 적 위치가 정중앙(15,000)이어서 P0·P1 거리가 5,000으로 동률일 때 -> 낮은 PlayerId인 P0(0) 선택
            const PlayerSlot tieBreakSlot = engine.SelectNearestAlivePlayerId(FixedVec2{ 15'000, 30'000 });
            // (c) 가까운 P1이 사망(hp=0)하면 생존한 P0(0) 선택
            engine.SetObjectHp(p1Id, 0);
            const PlayerSlot afterP1Dead = engine.SelectNearestAlivePlayerId(FixedVec2{ 18'000, 30'000 });
            // (d) P0도 사망(hp=0)하여 생존 타깃이 없으면 INVALID_PLAYER_SLOT 반환 및 적 탄환 발사 안전 생략(0 반환)
            engine.SetObjectHp(p0Id, 0);
            const PlayerSlot whenAllDead = engine.SelectNearestAlivePlayerId(FixedVec2{ 18'000, 30'000 });
            const ObjectId skippedProjId = engine.TryFireEnemyAimedProjectile(monId);
            engine.FlushLifecycleAtTickEnd(sink);
            Check(nearestWhenBothAlive == 1
                && tieBreakSlot == 0
                && afterP1Dead == 0
                && whenAllDead == INVALID_PLAYER_SLOT
                && skippedProjId == 0,
                "HostSim1115: Selects nearest alive player, breaks ties by lower PlayerId, and safely skips firing when no target is alive",
                counters);
        }
    }
    void RunHostSim1116Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. 피해 2(P0)와 피해 7(P1) 탄환의 Spawn/Combat 데이터 보존 및 GetPlayer(0) 공격력 미참조 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 50'000 }, {}, 3, 2);
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 50'000 }, {}, 3, 7);
            const ObjectId monId = engine.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 20'000 }, {}, 20, 1);
            engine.FlushLifecycleAtTickEnd(sink);
            script.ScheduleInput(1, 0, MoveBit::NONE, 1, 1, 16);
            script.ScheduleInput(1, 1, MoveBit::NONE, 1, 1, 16);
            // 9틱(150ms) 진행 -> P0 탄환(피해 2)과 P1 탄환(피해 7) 각각 생성 및 SPAWN 메시지 기록
            engine.StepFrame(75'000, script, sink);
            engine.StepFrame(75'000, script, sink);
            ObjectId projP0 = 0;
            ObjectId projP1 = 0;
            for (const auto& entry : sink.GetCapturedMessages())
            {
                if (const auto* sp = std::get_if<SpawnPayload>(&entry.message.payload))
                {
                    if (sp->kind == NetObjectKind::PROJ_PLAYER && sp->ownerPlayerId == 0 && sp->damage == 2)
                        projP0 = sp->objectId;
                    if (sp->kind == NetObjectKind::PROJ_PLAYER && sp->ownerPlayerId == 1 && sp->damage == 7)
                        projP1 = sp->objectId;
                }
            }
            // P0 객체를 제거한 뒤에도(적이 GetPlayer(0)을 읽지 않음), 이미 발사된 P0 탄환(2)과 P1 탄환(7)의 피해가 그대로 적용되는지 확인
            engine.RequestDestroy(p0Id, DespawnReason::DESTROYED);
            engine.FlushLifecycleAtTickEnd(sink);
            const bool hit0 = engine.ApplyProjectileHit(projP0, monId, sink); // 20 - 2 = 18
            const std::int32_t hpAfterHit0 = engine.FindObjectById(monId)->hp;
            const bool hit1 = engine.ApplyProjectileHit(projP1, monId, sink); // 18 - 7 = 11
            const std::int32_t hpAfterHit1 = engine.FindObjectById(monId)->hp;
            bool combat0Ok = false;
            bool combat1Ok = false;
            for (const auto& entry : sink.GetCapturedMessages())
            {
                if (const auto* ce = std::get_if<CombatEventPayload>(&entry.message.payload))
                {
                    if (ce->kind == CombatEventKind::DAMAGE_APPLIED && ce->instigatorPlayerId == 0 && ce->confirmedHp == 18)
                        combat0Ok = true;
                    if (ce->kind == CombatEventKind::DAMAGE_APPLIED && ce->instigatorPlayerId == 1 && ce->confirmedHp == 11)
                        combat1Ok = true;
                }
            }
            Check(projP0 != 0 && projP1 != 0 && p1Id != 0
                && hit0 && hit1
                && hpAfterHit0 == 18 && hpAfterHit1 == 11
                && combat0Ok && combat1Ok,
                "HostSim1116: Preserves projectile owner/damage (2 and 7) across Spawn/Combat without reading GetPlayer(0)",
                counters);
        }
    }

    void RunHostSim1117Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. HP=10에 피해 6+4면 HP=0, 사망 확정 후보 1회, 이미 사망·삭제 예정인
        // 적 피격 제외 및 추가 점수 없음
        {
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            const ObjectId monId = engine.RequestSpawn(NetObjectKind::MONSTER,
                1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 20'000 }, {}, 10, 1);
            const ObjectId proj6 = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER,
                1, 0, FixedVec2{ 15'000, 20'000 }, {}, 1, 6);
            const ObjectId proj4 = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER,
                1, 0, FixedVec2{ 15'000, 20'000 }, {}, 1, 4);
            const ObjectId projExtra = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER,
                1, 0, FixedVec2{ 15'000, 20'000 }, {}, 1, 5);
            engine.FlushLifecycleAtTickEnd(sink);

            // 첫 피격(6): HP 10 -> 4, 아직 생존(사망 후보 0회, 점수 0)
            const bool firstHit = engine.ApplyProjectileHit(proj6, monId, sink);
            const std::int32_t hpAfter6 = engine.FindObjectById(monId)->hp;
            const std::uint32_t deathAfter6 = engine.GetDeathCandidateCount();

            // 두 번째 피격(4): HP 4 -> 0, 사망 확정 후보 1회 등록, 점수 +250
            const bool secondHit = engine.ApplyProjectileHit(proj4, monId, sink);
            const std::int32_t hpAfter6Plus4 = engine.FindObjectById(monId)->hp;
            const std::uint32_t deathAfter10 = engine.GetDeathCandidateCount();
            const std::uint32_t scoreAfter10 = engine.GetTeamScore();

            // 이미 사망(HP==0, pendingDestroy)한 적에게 추가 탄환(5) 피격 시도 -> 피격 제외(false), 사망 후보 1회 유지, 추가 점수 없음
            const bool extraHitOnDead = engine.ApplyProjectileHit(projExtra, monId, sink);
            Check(firstHit && hpAfter6 == 4 && deathAfter6 == 0
                && secondHit && hpAfter6Plus4 == 0 && deathAfter10 == 1 && scoreAfter10 == 250
                && !extraHitOnDead
                && engine.GetDeathCandidateCount() == 1
                && engine.GetTeamScore() == 250,
                "HostSim1117: HP=10 hit by 6+4 becomes HP=0 with 1 death candidate and excludes dead/pendingDestroy targets without extra score",
                counters);
        }
    }

    void RunHostSim1118Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. 동시 접촉 충돌 순서를 뒤집어도({0, 1} vs {1, 0}) 동률 시 작은 PlayerId(P0) 단일 획득 확정 및 동시 보상 없음 확인
        {
            CaptureSink sinkA;
            HostSimEngine engineA{ 1000 };
            const ObjectId p0A = engineA.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 3, 2);
            const ObjectId p1A = engineA.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 3, 2);
            const ObjectId itemA = engineA.RequestSpawn(NetObjectKind::ITEM, 1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 30'000 }, {}, 1, 0);
            engineA.FlushLifecycleAtTickEnd(sinkA);
            CaptureSink sinkB;
            HostSimEngine engineB{ 1000 };
            const ObjectId p0B = engineB.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 3, 2);
            const ObjectId p1B = engineB.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 3, 2);
            const ObjectId itemB = engineB.RequestSpawn(NetObjectKind::ITEM, 1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 30'000 }, {}, 1, 0);
            engineB.FlushLifecycleAtTickEnd(sinkB);
            // engineA는 {0, 1} 순서, engineB는 뒤집힌 {1, 0} 순서로 동시 접촉 주입
            const PlayerSlot winnerA = engineA.ResolveItemPickup(itemA, { 0, 1 }, sinkA);
            const PlayerSlot winnerB = engineB.ResolveItemPickup(itemB, { 1, 0 }, sinkB);
            // 이미 획득된 아이템에 재접촉 시도 시 거부(INVALID_PLAYER_SLOT)
            const PlayerSlot duplicateAttempt = engineA.ResolveItemPickup(itemA, { 1 }, sinkA);
            Check(winnerA == 0 && winnerB == 0
                && duplicateAttempt == INVALID_PLAYER_SLOT
                && engineA.FindObjectById(p0A)->subType == 2
                && engineA.FindObjectById(p1A)->subType == 1
                && engineB.FindObjectById(p0B)->subType == 2
                && engineB.FindObjectById(p1B)->subType == 1,
                "HostSim1118: Resolves simultaneous item pickup to single winner (smaller PlayerId on tie) regardless of collision order without double reward",
                counters);
        }
    }

    void RunHostSim1119Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. 중복 접촉·중복 삭제 요청·사망 애니메이션 틱 진행에도 사망(ENTITY_DIED)·점수·고유 eventId 1회 확정 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId monId = engine.RequestSpawn(NetObjectKind::MONSTER, 2, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 20'000 }, {}, 10, 1);
            const ObjectId proj1 = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 0, FixedVec2{ 15'000, 20'000 }, {}, 1, 10);
            const ObjectId proj2 = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 1, FixedVec2{ 15'000, 20'000 }, {}, 1, 10);
            engine.FlushLifecycleAtTickEnd(sink);
            // 첫 치명타 접촉(피해 10 -> HP 0): 사망·점수(+500) 1회 확정
            const bool hit1 = engine.ApplyProjectileHit(proj1, monId, sink);
            // 동일 틱 중복 접촉 및 중복 삭제 요청 주입 -> 모두 무시되어야 함
            const bool hit2 = engine.ApplyProjectileHit(proj2, monId, sink);
            const bool dupDestroy = engine.RequestDestroy(monId, DespawnReason::DESTROYED);
            // 이후 여러 틱(애니메이션 재생 시간) 진행해도 판정이 반복되지 않아야 함
            engine.StepFrame(50'000, script, sink);
            engine.StepFrame(50'000, script, sink);
            int diedEventCount = 0;
            EventId damageEventId = 0;
            EventId diedEventId = 0;
            for (const auto& entry : sink.GetCapturedMessages())
            {
                if (const auto* ce = std::get_if<CombatEventPayload>(&entry.message.payload))
                {
                    if (ce->targetObjectId == monId && ce->kind == CombatEventKind::DAMAGE_APPLIED)
                        damageEventId = ce->eventId;
                    if (ce->targetObjectId == monId && ce->kind == CombatEventKind::ENTITY_DIED)
                    {
                        ++diedEventCount;
                        diedEventId = ce->eventId;
                    }
                }
            }
            Check(hit1 && !hit2 && !dupDestroy
                && diedEventCount == 1
                && damageEventId != 0 && diedEventId == damageEventId + 1
                && engine.GetDeathCandidateCount() == 1
                && engine.GetTeamScore() == 500,
                "HostSim1119: Confirms ENTITY_DIED, score, and monotonic eventId exactly once despite duplicate hits, destroy requests, or animation ticks",
                counters);
        }
    }
    void RunHostSim1120Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. 동시 공격·획득 입력 순서를 바꿔도({P0, P1} vs {P1, P0}) 계약에 맞는 동일한 확정 HP/사망/획득/점수 결과 확인
        {
            CaptureSink sinkA;
            HostSimEngine engineA{ 1000 };
            engineA.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 3, 10);
            engineA.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 3, 10);
            const ObjectId monA = engineA.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 20'000 }, {}, 10, 1);
            const ObjectId proj0A = engineA.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 0, FixedVec2{ 15'000, 20'000 }, {}, 1, 10);
            const ObjectId proj1A = engineA.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 1, FixedVec2{ 15'000, 20'000 }, {}, 1, 10);
            const ObjectId itemA = engineA.RequestSpawn(NetObjectKind::ITEM, 1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 30'000 }, {}, 1, 0);
            engineA.FlushLifecycleAtTickEnd(sinkA);
            CaptureSink sinkB;
            HostSimEngine engineB{ 1000 };
            engineB.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 3, 10);
            engineB.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 3, 10);
            const ObjectId monB = engineB.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 20'000 }, {}, 10, 1);
            const ObjectId proj0B = engineB.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 0, FixedVec2{ 15'000, 20'000 }, {}, 1, 10);
            const ObjectId proj1B = engineB.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 1, FixedVec2{ 15'000, 20'000 }, {}, 1, 10);
            const ObjectId itemB = engineB.RequestSpawn(NetObjectKind::ITEM, 1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 30'000 }, {}, 1, 0);
            engineB.FlushLifecycleAtTickEnd(sinkB);
            // 동시 치명타 공격 및 동시 아이템 접촉 순서를 정방향({P0, P1})과 역방향({P1, P0})으로 각각 주입
            const std::size_t hitsA = engineA.ResolveSimultaneousProjectileHits(monA, { proj0A, proj1A }, sinkA);
            const std::size_t hitsB = engineB.ResolveSimultaneousProjectileHits(monB, { proj1B, proj0B }, sinkB);
            const PlayerSlot pickA = engineA.ResolveItemPickup(itemA, { 0, 1 }, sinkA);
            const PlayerSlot pickB = engineB.ResolveItemPickup(itemB, { 1, 0 }, sinkB);
            PlayerSlot killerA = INVALID_PLAYER_SLOT;
            PlayerSlot killerB = INVALID_PLAYER_SLOT;
            for (const auto& e : sinkA.GetCapturedMessages())
                if (const auto* ce = std::get_if<CombatEventPayload>(&e.message.payload))
                    if (ce->kind == CombatEventKind::ENTITY_DIED) killerA = ce->instigatorPlayerId;
            for (const auto& e : sinkB.GetCapturedMessages())
                if (const auto* ce = std::get_if<CombatEventPayload>(&e.message.payload))
                    if (ce->kind == CombatEventKind::ENTITY_DIED) killerB = ce->instigatorPlayerId;
            Check(hitsA == 1 && hitsB == 1
                && killerA == 0 && killerB == 0
                && pickA == 0 && pickB == 0
                && engineA.GetTeamScore() == 250 && engineB.GetTeamScore() == 250,
                "HostSim1120: Reversing simultaneous attack and pickup input order produces identical killer, pickup winner, and score",
                counters);
        }
        // 2. 이전 씬 지연 이벤트(대기 중인 생성·삭제)가 씬 전환 이후 새 씬 객체에 적용되지 않음 확인
        {
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId oldP0 = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 });
            engine.FlushLifecycleAtTickEnd(sink);
            // 이전 씬에서 지연 생성(미사일)과 지연 삭제(oldP0)가 대기열에 들어간 직후 씬 전환 발생
            const ObjectId stalePendingMissile = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 0, FixedVec2{ 10'000, 25'000 });
            engine.RequestDestroy(oldP0, DespawnReason::STAGE_END);
            engine.TransitionScene();
            // 새 씬에서 P0 생성 후 틱 종료 플러시 실행 -> 이전 씬의 지연 미사일이나 삭제가 섞이지 않고 새 P0만 존재해야 함
            const ObjectId newP0 = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 12'000, 30'000 });
            engine.FlushLifecycleAtTickEnd(sink);
            Check(newP0 > stalePendingMissile
                && engine.GetActiveObjects().size() == 1
                && engine.FindObjectById(newP0) != nullptr
                && engine.FindObjectById(stalePendingMissile) == nullptr
                && engine.FindObjectById(oldP0) == nullptr,
                "HostSim1120: Delayed spawn/despawn events from previous scene are discarded on scene transition and never affect new scene objects",
                counters);
        }
    }

    void RunHostSim1121Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. 3주차 공동 일반 전투 통합: 적 HP·제거·아이템 단일 획득자·팀 점수·스냅샷 권위 상태 일치 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 3, 6);
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 3, 4);
            const ObjectId monId = engine.RequestSpawn(NetObjectKind::MONSTER, 2, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 20'000 }, {}, 10, 1);
            const ObjectId proj0 = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 0, FixedVec2{ 15'000, 20'000 }, {}, 1, 6);
            const ObjectId proj1 = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 1, FixedVec2{ 15'000, 20'000 }, {}, 1, 4);
            const ObjectId itemId = engine.RequestSpawn(NetObjectKind::ITEM, 1, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 30'000 }, {}, 1, 0);
            engine.FlushLifecycleAtTickEnd(sink);

            // 동시 피격(6 + 4 = 10 -> HP 0, 사망 +500점) 및 동시 아이템 접촉(동률 -> P0 레벨 2, P1 레벨 1 유지)
            const std::size_t hitCount = engine.ResolveSimultaneousProjectileHits(monId, { proj1, proj0 }, sink);
            const PlayerSlot itemWinner = engine.ResolveItemPickup(itemId, { 1, 0 }, sink);

            // 3틱(50ms) 진행 -> 틱 종료 시 죽은 몬스터/탄환/아이템 제거(DESPAWN) 및 20Hz 스냅샷 발행
            engine.StepFrame(50'000, script, sink);

            bool snapshotHasOnlyPlayers = false;
            if (!sink.GetCapturedBatches().empty())
            {
                const auto& lastMsg = sink.GetCapturedBatches().back().parts.front();
                if (const auto* snap = std::get_if<StateSnapshotPayload>(&lastMsg.payload))
                {
                    snapshotHasOnlyPlayers = (snap->objects.size() == 2)
                        && (engine.FindObjectById(monId) == nullptr)
                        && (engine.FindObjectById(itemId) == nullptr);
                }
            }

            Check(hitCount == 2
                && itemWinner == 0
                && engine.GetTeamScore() == 500
                && engine.FindObjectById(p0Id)->subType == 2
                && engine.FindObjectById(p1Id)->subType == 1
                && snapshotHasOnlyPlayers,
                "HostSim1121: Authoritative combat state matches across enemy HP/despawn, single item winner, team score, and snapshot",
                counters);
        }
    }

    void RunHostSim1122Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. 보스 등장 틱 경계(7799 미등장 / 7800 1회 등장) 및 HP 경계(4001 유지 / 4000에서 MBOSS0->MBOSS1 1회 전환)
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId beforeTick = engine.TrySpawnScheduledBoss(BOSS_SPAWN_TICK - 1);
            const ObjectId bossId = engine.TrySpawnScheduledBoss(BOSS_SPAWN_TICK);
            const ObjectId dupBoss = engine.TrySpawnScheduledBoss(BOSS_SPAWN_TICK + 1);
            engine.FlushLifecycleAtTickEnd(sink);
            // 30틱(500ms) 진행 -> IDLE(0) -> MBOSS0(10)
            for (int i = 0; i < 6; ++i)
                engine.StepFrame(83'334, script, sink);
            const std::uint8_t stateAtStart = engine.FindObjectById(bossId)->fsmState;
            // HP 4001: 경계 직전 -> 전환 안 됨
            engine.SetObjectHp(bossId, BOSS_PHASE2_HP_THRESHOLD + 1);
            const bool transAt4001 = engine.EvaluateBossFsmTransition(bossId);
            // HP 4000: 정확한 경계 -> MBOSS1(11) 1회 전환, 재호출 시 중복 전환 없음
            engine.SetObjectHp(bossId, BOSS_PHASE2_HP_THRESHOLD);
            const bool transAt4000First = engine.EvaluateBossFsmTransition(bossId);
            const bool transAt4000Dup = engine.EvaluateBossFsmTransition(bossId);
            Check(beforeTick == 0 && bossId != 0 && dupBoss == 0
                && stateAtStart == 10
                && !transAt4001
                && transAt4000First && !transAt4000Dup
                && engine.FindObjectById(bossId)->fsmState == 11,
                "HostSim1122: Spawns boss once at tick 7800 and transitions MBOSS0->MBOSS1 once at HP<=4000 boundary",
                counters);
        }
        // 2. 보스 생존 타깃 정책: 가까운 생존자 우선, 동률 P0, 전원 사망 시 발사 안전 생략
        {
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 3, 2);
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 3, 2);
            const ObjectId bossId = engine.TrySpawnScheduledBoss(BOSS_SPAWN_TICK);
            engine.FlushLifecycleAtTickEnd(sink);
            const PlayerSlot nearP1 = engine.SelectNearestAlivePlayerId(FixedVec2{ 19'000, 10'000 });
            const PlayerSlot tieP0 = engine.SelectNearestAlivePlayerId(FixedVec2{ 15'000, 10'000 });
            engine.SetObjectHp(p0Id, 0);
            engine.SetObjectHp(p1Id, 0);
            const ObjectId skippedFire = engine.TryFireEnemyAimedProjectile(bossId);
            Check(nearP1 == 1 && tieP0 == 0 && skippedFire == 0,
                "HostSim1122: Boss target policy selects nearest alive player, breaks ties to P0, and skips firing when all dead",
                counters);
        }
    }
    void RunHostSim1123Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;
        // 1. CHostWorldSync 분할기: 0/1/36/37/9181(255조각 초과) 실제 Encode 크기(<=1,200B), 단독 Decode, 다중 조각 미전송 교체 확인
        {
            auto makeRecords = [](std::size_t n) {
                std::vector<ObjectStateRecord> recs;
                recs.reserve(n);
                for (std::size_t i = 0; i < n; ++i)
                {
                    ObjectStateRecord r{};
                    r.objectId = static_cast<ObjectId>(i + 1);
                    r.kind = NetObjectKind::MONSTER;
                    r.subType = 1;
                    r.alive = 1;
                    r.hp = 100;
                    r.maxHp = 100;
                    recs.push_back(r);
                }
                return recs;
                };
            std::uint32_t seq = 1;
            net::SnapshotBatch b0{}, b1{}, b36{}, b37{}, bOver{};
            const auto r0 = CHostWorldSync::BuildSnapshotBatch(1000, 10, seq, makeRecords(0), b0);
            const auto r1 = CHostWorldSync::BuildSnapshotBatch(1000, 11, seq, makeRecords(1), b1);
            const auto r36 = CHostWorldSync::BuildSnapshotBatch(1000, 12, seq, makeRecords(36), b36);
            const auto r37 = CHostWorldSync::BuildSnapshotBatch(1000, 13, seq, makeRecords(37), b37);
            const auto rOver = CHostWorldSync::BuildSnapshotBatch(
                1000, 14, seq, makeRecords(MAX_SNAPSHOT_OBJECTS_PER_PACKET * net::MAX_SNAPSHOT_PARTS_PER_BATCH + 1), bOver);
            std::vector<std::uint8_t> bytes0, bytes1, bytes36, bytes37p0, bytes37p1;
            const bool encodeOk =
                (CPacketCodec::Encode(b0.parts[0], bytes0) == CodecError::NONE && bytes0.size() == 40)
                && (CPacketCodec::Encode(b1.parts[0], bytes1) == CodecError::NONE && bytes1.size() == 72)
                && (CPacketCodec::Encode(b36.parts[0], bytes36) == CodecError::NONE && bytes36.size() == 1192)
                && (CPacketCodec::Encode(b37.parts[0], bytes37p0) == CodecError::NONE && bytes37p0.size() == 1192)
                && (CPacketCodec::Encode(b37.parts[1], bytes37p1) == CodecError::NONE && bytes37p1.size() == 72)
                && (bytes36.size() <= MAX_UDP_BYTES && bytes37p0.size() <= MAX_UDP_BYTES);
            // 타 조각(part 0) 없이 part 1 단독 Decode 해석 가능 확인
            Message decodedPart1{};
            const bool standaloneDecodeOk =
                (CPacketCodec::Decode(bytes37p1, decodedPart1) == CodecError::NONE)
                && (std::get<StateSnapshotPayload>(decodedPart1.payload).partIndex == 1)
                && (std::get<StateSnapshotPayload>(decodedPart1.payload).objects.size() == 1)
                && (std::get<StateSnapshotPayload>(decodedPart1.payload).objects[0].objectId == 37);
            // 다중 조각 배치 미전송 교체 확인 (틱 13 -> 틱 15 교체 시 혼합 없음)
            net::FakeSendSink sink;
            net::SnapshotBatch b37Next{};
            CHostWorldSync::BuildSnapshotBatch(1000, 15, seq, makeRecords(37), b37Next);
            const auto q1 = sink.EnqueueSnapshotBatch(b37);
            const auto q2 = sink.EnqueueSnapshotBatch(b37Next);
            const auto taken = sink.TakePendingBatchForSend();
            const bool replaceOk =
                (q1 == net::SendResult::Queued)
                && (q2 == net::SendResult::ReplacedPending)
                && taken.has_value()
                && (taken->parts.size() == 2)
                && (taken->parts[0].header.hostTick == 15 && taken->parts[1].header.hostTick == 15);
            Check(r0 == net::SendResult::Queued && b0.parts.size() == 1
                && r1 == net::SendResult::Queued && b1.parts.size() == 1
                && r36 == net::SendResult::Queued && b36.parts.size() == 1
                && r37 == net::SendResult::Queued && b37.parts.size() == 2
                && rOver == net::SendResult::TooLarge
                && encodeOk && standaloneDecodeOk && replaceOk,
                "HostSim1123: Splits 0/1/36/37/overflow snapshots <=1200B, decodes parts independently, and replaces pending multi-part batch",
                counters);
        }
        // 2. 특수 탄환(가속 patternType=2 30틱 x1.5 / 감속 patternType=3 42틱 /2) 속도·패턴 스냅샷 보존 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };
            const ObjectId accelId = engine.RequestSpawn(
                NetObjectKind::PROJ_MONSTER, 2, INVALID_PLAYER_SLOT,
                FixedVec2{ 10'000, 10'000 }, FixedVec2{ 0, 40'000 }, 1, 1, 2);
            const ObjectId decelId = engine.RequestSpawn(
                NetObjectKind::PROJ_MONSTER, 3, INVALID_PLAYER_SLOT,
                FixedVec2{ 20'000, 10'000 }, FixedVec2{ 0, 40'000 }, 1, 1, 3);
            engine.FlushLifecycleAtTickEnd(sink);
            // 30틱(500ms) 진행 -> 가속 탄환만 40,000 -> 60,000 전환
            for (int i = 0; i < 6; ++i)
                engine.StepFrame(83'334, script, sink);
            const bool at30Ok =
                (engine.FindObjectById(accelId)->vel.y == 60'000)
                && (engine.FindObjectById(decelId)->vel.y == 40'000);
            // +12틱(총 42틱 = 700ms) 진행 -> 감속 탄환 40,000 -> 20,000 전환 및 42틱 스냅샷 반영
            engine.StepFrame(83'334, script, sink);
            engine.StepFrame(83'334, script, sink);
            engine.StepFrame(33'334, script, sink);
            bool snapOk = false;
            if (!sink.GetCapturedBatches().empty())
            {
                const auto& snap = std::get<StateSnapshotPayload>(sink.GetCapturedBatches().back().parts[0].payload);
                std::int32_t accelVelY = 0, decelVelY = 0;
                std::uint8_t accelState = 0, decelState = 0;
                for (const auto& rec : snap.objects)
                {
                    if (rec.objectId == accelId) { accelVelY = rec.vel.y; accelState = rec.fsmState; }
                    if (rec.objectId == decelId) { decelVelY = rec.vel.y; decelState = rec.fsmState; }
                }
                snapOk = (snap.snapshotTick == 42)
                    && (accelVelY == 60'000 && accelState == 2)
                    && (decelVelY == 20'000 && decelState == 3);
            }
            Check(at30Ok && snapOk,
                "HostSim1123: Preserves special projectile patternType and tick-based accel/decel velocity in snapshot",
                counters);
        }
    }

    void RunHostSim1124Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. 무적/재등장(fsmState=1), 개인 전투 불능(hp=0, 이동/발사 제외, P1 생존 계속), 객체 삭제(RequestDestroy) 구분
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 2, 2);
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 2, 2);
            engine.FlushLifecycleAtTickEnd(sink);

            // P0 피격 (HP 2->1): 재등장 무적(fsmState=1) 진입, 무적 중 추가 피격 무시, 120틱 후 무적 해제
            const bool firstHit = engine.ApplyDamageToPlayer(0, 1);
            const bool ignoredDuringInv = !engine.ApplyDamageToPlayer(0, 1);
            const std::uint8_t stateInv = engine.FindObjectById(p0Id)->fsmState;
            for (int i = 0; i < 24; ++i)
                engine.StepFrame(83'334, script, sink); // 24 * 5 = 120틱 (2초)
            const std::uint8_t stateAfterInv = engine.FindObjectById(p0Id)->fsmState;

            // P0 치명타 (HP 1->0): 개인 전투 불능(alive=0, 객체는 유지), P1 생존으로 게임 계속(!AreAllPlayersDead)
            const bool lethalHit = engine.ApplyDamageToPlayer(0, 1);
            const bool continuesWithP1 = !engine.AreAllPlayersDead() && (engine.FindObjectById(p0Id) != nullptr);

            // P0 이동/발사 입력을 넣어도 위치 고정 및 탄환 미생성 확인
            const FixedVec2 deadPosBefore = engine.FindObjectById(p0Id)->pos;
            script.ScheduleInput(121, 0, MoveBit::RIGHT, 1, 1, 2000);
            for (int i = 0; i < 3; ++i)
                engine.StepFrame(83'334, script, sink); // 15틱 진행
            bool deadNoMoveNoFire = (engine.FindObjectById(p0Id)->pos == deadPosBefore);
            for (const auto& obj : engine.GetActiveObjects())
            {
                if (obj.kind == NetObjectKind::PROJ_PLAYER && obj.ownerPlayerId == 0)
                    deadNoMoveNoFire = false;
            }

            // P1도 사망 시 전원 사망, 객체 삭제 요청 후에는 FindObjectById == nullptr
            engine.ApplyDamageToPlayer(1, 2);
            const bool allDead = engine.AreAllPlayersDead();
            engine.RequestDestroy(p0Id, DespawnReason::STAGE_END);
            engine.FlushLifecycleAtTickEnd(sink);

            Check(firstHit && ignoredDuringInv && stateInv == 1 && stateAfterInv == 0
                && lethalHit && continuesWithP1 && deadNoMoveNoFire
                && allDead && engine.FindObjectById(p0Id) == nullptr && p1Id != 0,
                "HostSim1124: Distinguishes respawn invincibility, combat-disabled player, and object destruction while continuing if P1 lives",
                counters);
        }

        // 2. 사망자 조준·아이템 획득 대상 제외 확인 (P0 사망·P1 생존 시 P1만 조준/획득)
        {
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            const ObjectId p0Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 1, 2);
            const ObjectId p1Id = engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 30'000, 30'000 }, {}, 1, 2);
            const ObjectId monId = engine.RequestSpawn(NetObjectKind::MONSTER, 1, INVALID_PLAYER_SLOT, FixedVec2{ 10'100, 29'000 }, {}, 50, 1);
            const ObjectId itemId = engine.RequestSpawn(NetObjectKind::ITEM, 1, INVALID_PLAYER_SLOT, FixedVec2{ 10'100, 30'000 }, {}, 1, 0);
            engine.FlushLifecycleAtTickEnd(sink);

            // P0 사망 -> 적/아이템 바로 옆에 P0가 있어도 조준·획득 모두 P1 선택
            engine.ApplyDamageToPlayer(0, 1);
            const PlayerSlot aimedSlot = engine.SelectNearestAlivePlayerId(FixedVec2{ 10'100, 29'000 });
            const ObjectId firedProj = engine.TryFireEnemyAimedProjectile(monId);
            const PlayerSlot itemWinner = engine.ResolveItemPickup(itemId, { 0, 1 }, sink);

            Check(aimedSlot == 1 && firedProj != 0 && itemWinner == 1
                && engine.FindObjectById(p0Id)->subType == 1
                && engine.FindObjectById(p1Id)->subType == 2,
                "HostSim1124: Excludes combat-disabled P0 from enemy aiming and item pickup",
                counters);
        }
    }

    void RunHostSim1125Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. 동일 틱 보스 사망 + 전원 사망: 콜백 순서({보스->전원} vs {전원->보스})와 무관하게 틱 종료 후 VICTORY 확정
        {
            auto runSameTick = [](bool bossFirst) {
                InputScript script;
                CaptureSink sink;
                HostSimEngine engine{ 1000 };

                engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 1, 100);
                engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 1, 100);
                const ObjectId bossId = engine.RequestSpawn(NetObjectKind::MONSTER, MONSTER_SUBTYPE_BOSS, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 10'000 }, {}, 100, 1);
                const ObjectId projId = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 0, FixedVec2{ 15'000, 10'000 }, {}, 1, 100);
                engine.FlushLifecycleAtTickEnd(sink);

                if (bossFirst)
                {
                    engine.ApplyProjectileHit(projId, bossId, sink);
                    engine.ApplyDamageToPlayer(0, 1);
                    engine.ApplyDamageToPlayer(1, 1);
                }
                else
                {
                    engine.ApplyDamageToPlayer(0, 1);
                    engine.ApplyDamageToPlayer(1, 1);
                    engine.ApplyProjectileHit(projId, bossId, sink);
                }

                // 개별 충돌 콜백 직후(틱 종료 전)에는 결과가 먼저 확정되지 않아야 함
                const bool deferredBeforeTickEnd = !engine.GetTeamOutcome().has_value();

                // 1틱 종료 후 집계 -> VICTORY 우선 확정
                engine.StepFrame(16'667, script, sink);
                return deferredBeforeTickEnd
                    && engine.GetTeamOutcome().has_value()
                    && (*engine.GetTeamOutcome() == MatchOutcome::VICTORY);
                };

            Check(runSameTick(true) && runSameTick(false),
                "HostSim1125: Same-tick boss death + all players dead resolves to VICTORY at tick-end regardless of callback order",
                counters);
        }

        // 2. 보스 생존 중 1명 사망이면 계속(nullopt), 전원 사망이면 틱 종료 후 DEFEAT 확정
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 1, 2);
            engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 1, 2);
            engine.RequestSpawn(NetObjectKind::MONSTER, MONSTER_SUBTYPE_BOSS, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 10'000 }, {}, 100, 1);
            engine.FlushLifecycleAtTickEnd(sink);

            // P0만 사망 -> 틱 종료 후에도 미확정(계속 진행)
            engine.ApplyDamageToPlayer(0, 1);
            engine.StepFrame(16'667, script, sink);
            const bool stillRunning = !engine.GetTeamOutcome().has_value();

            // P1도 사망 -> 콜백 직후 미확정, 틱 종료 후 DEFEAT 확정
            engine.ApplyDamageToPlayer(1, 1);
            const bool deferred = !engine.GetTeamOutcome().has_value();
            engine.StepFrame(16'667, script, sink);
            const bool defeatConfirmed = engine.GetTeamOutcome().has_value()
                && (*engine.GetTeamOutcome() == MatchOutcome::DEFEAT);

            Check(stillRunning && deferred && defeatConfirmed,
                "HostSim1125: Continues while one player lives and confirms DEFEAT at tick-end when all players die",
                counters);
        }
    }

    void RunHostSim1126Suite(TestCounters& counters)
    {
        using namespace host;
        using namespace protocol;

        // 1. 승리 조건이 연속 5틱 이상 유지되어도 RESULT(VICTORY) 이벤트·팀 점수가 정확히 1회만 발행됨 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 3, 100);
            engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 3, 100);
            const ObjectId bossId = engine.RequestSpawn(NetObjectKind::MONSTER, MONSTER_SUBTYPE_BOSS, INVALID_PLAYER_SLOT, FixedVec2{ 15'000, 10'000 }, {}, 100, 1);
            const ObjectId projId = engine.RequestSpawn(NetObjectKind::PROJ_PLAYER, 1, 0, FixedVec2{ 15'000, 10'000 }, {}, 1, 100);
            engine.FlushLifecycleAtTickEnd(sink);

            engine.ApplyProjectileHit(projId, bossId, sink); // 보스 처치 -> 팀 점수 +1500
            // 연속 5틱 진행 (같은 승리 조건 유지)
            engine.StepFrame(83'334, script, sink);

            int resultCount = 0;
            ResultPayload lastResult{};
            for (const auto& entry : sink.GetCapturedMessages())
            {
                if (entry.channel == net::Channel::Tcp
                    && entry.message.header.type == MessageType::RESULT)
                {
                    ++resultCount;
                    lastResult = std::get<ResultPayload>(entry.message.payload);
                }
            }

            Check(resultCount == 1
                && lastResult.eventId != 0
                && lastResult.outcome == MatchOutcome::VICTORY
                && lastResult.finalTeamScore == engine.GetTeamScore()
                && lastResult.finalTeamScore == 1500,
                "HostSim1126: Emits RESULT(VICTORY) with finalTeamScore exactly once across consecutive ticks",
                counters);
        }

        // 2. 패배 조건이 연속 5틱 이상 유지되어도 RESULT(DEFEAT) 이벤트가 정확히 1회만 발행됨 확인
        {
            InputScript script;
            CaptureSink sink;
            HostSimEngine engine{ 1000 };

            engine.RequestSpawn(NetObjectKind::PLAYER, 1, 0, FixedVec2{ 10'000, 30'000 }, {}, 1, 2);
            engine.RequestSpawn(NetObjectKind::PLAYER, 1, 1, FixedVec2{ 20'000, 30'000 }, {}, 1, 2);
            engine.FlushLifecycleAtTickEnd(sink);

            engine.ApplyDamageToPlayer(0, 1);
            engine.ApplyDamageToPlayer(1, 1);
            engine.StepFrame(83'334, script, sink); // 연속 5틱 진행

            int resultCount = 0;
            ResultPayload lastResult{};
            for (const auto& entry : sink.GetCapturedMessages())
            {
                if (entry.channel == net::Channel::Tcp
                    && entry.message.header.type == MessageType::RESULT)
                {
                    ++resultCount;
                    lastResult = std::get<ResultPayload>(entry.message.payload);
                }
            }

            Check(resultCount == 1
                && lastResult.eventId != 0
                && lastResult.outcome == MatchOutcome::DEFEAT
                && lastResult.finalTeamScore == engine.GetTeamScore(),
                "HostSim1126: Emits RESULT(DEFEAT) exactly once across consecutive ticks",
                counters);
        }
    }
}

int main()
{
    TestCounters counters{};

    std::cout << "=== Virus Striker HostSimHarness (11/08 Fixed Tick & Lifecycle) ===\n";
    RunHostSim1108Suite(counters);
    RunHostSim1109Suite(counters);
    RunHostSim1110Suite(counters);
    RunHostSim1111Suite(counters);
    RunHostSim1112Suite(counters);
    RunHostSim1113Suite(counters);
    RunHostSim1114Suite(counters);
    RunHostSim1115Suite(counters);
    RunHostSim1116Suite(counters);
    RunHostSim1117Suite(counters);
    RunHostSim1118Suite(counters);
    RunHostSim1119Suite(counters);
    RunHostSim1120Suite(counters);
    RunHostSim1121Suite(counters);
    RunHostSim1122Suite(counters);
    RunHostSim1123Suite(counters);
    RunHostSim1124Suite(counters);
    RunHostSim1125Suite(counters);
    RunHostSim1126Suite(counters);
    std::cout << "-------------------------------------------------------------------\n";
    std::cout << "Passed: " << counters.passed << '\n';
    std::cout << "Failed: " << counters.failed << '\n';

    return (counters.failed == 0) ? 0 : 1;
}
