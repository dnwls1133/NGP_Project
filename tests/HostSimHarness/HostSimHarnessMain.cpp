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
}

int main()
{
    TestCounters counters{};

    std::cout << "=== Virus Striker HostSimHarness (11/08 Fixed Tick & Lifecycle) ===\n";
    RunHostSim1108Suite(counters);
    RunHostSim1109Suite(counters);
    RunHostSim1110Suite(counters);
    RunHostSim1111Suite(counters);
    std::cout << "-------------------------------------------------------------------\n";
    std::cout << "Passed: " << counters.passed << '\n';
    std::cout << "Failed: " << counters.failed << '\n';

    return (counters.failed == 0) ? 0 : 1;
}
