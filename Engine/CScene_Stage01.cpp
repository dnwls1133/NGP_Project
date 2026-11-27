#include "pch.h"
#include "CScene_Stage01.h"
#include "CObject.h"

#include "CPlayer.h"
#include "CPlayerDead.h"
#include "CItem.h"
#include "CMonster.h"
#include "CMissile.h"
#include "CBackground.h"

#include "CCore.h"

#include "CColliderMgr.h"
#include "CPathMgr.h"
#include "CKeyMgr.h"
#include "CSceneMgr.h"
#include "CTimeMgr.h"
#include "CResMgr.h"

#include "CTexture.h"
#include "CSound.h"
#include "CSoundMgr.h"
#include "CSessionMgr.h"
#include "CCamera.h"

#include "SelectGDI.h"

#include "AI.h"
#include "CState.h"
#include "CIdleState.h"
#include "CTraceState.h"

#include "CStageCard.h"
#include "CStageHUD.h"
#include "CAnimator.h"

#ifdef _DEBUG
#include <iostream>
#endif
#include <cmath>
#include <utility>

namespace
{
    constexpr float SnapshotLerpDuration = host::SIM_FIXED_DT * host::STATE_PUBLISH_INTERVAL_TICKS;
    constexpr float EntryFadeDuration = 1.5f;
    constexpr float ClearFadeDuration = 3.f;
    constexpr float FailCardDuration = 5.f;
    constexpr float FailFadeDuration = 5.f;
}

#ifdef _DEBUG
namespace
{
    void LogItemPickupApplied(const network::CSessionMgr& session, const protocol::CombatEventPayload& combat)
    {
        const char* role = session.IsHost() ? "[HOST]" : "[CLIENT]";
        const bool pickedBySelf = combat.instigatorPlayerId == session.GetLocalPlayerId();
        const char* message = pickedBySelf ? "ITEM_PICKED applied for self" : "ITEM_PICKED applied for teammate";
        std::cout << role << ' ' << message
            << ", eventId=" << combat.eventId
            << ", itemId=" << combat.sourceObjectId
            << ", pickerSlot=" << static_cast<int>(combat.instigatorPlayerId)
            << ", level=" << static_cast<int>(combat.updatedPlayerLevel)
            << ", teamScore=" << combat.teamScore << '\n';
    }
}
#endif

CScene_Stage01::CScene_Stage01()
    :m_bClear(false)
    , m_bFail(false)
    , m_fClearAcc(0.f)
{
    m_pStageTheme = CResMgr::GetInst()->LoadSound(L"Stage Theme", L"sound\\BGM\\BGM_StageTheme.mp3");
    m_pBossTheme = CResMgr::GetInst()->LoadSound(L"Boss Theme", L"sound\\BGM\\BGM_BossTheme.mp3");
    m_pClearTheme = CResMgr::GetInst()->LoadSound(L"Stage Clear", L"sound\\BGM\\BGM_StageClear.mp3");
    m_pFailTheme = CResMgr::GetInst()->LoadSound(L"Stage Fail", L"sound\\BGM\\BGM_StageFail.mp3");
}

CScene_Stage01::~CScene_Stage01()
{
}

bool CScene_Stage01::ConfirmItemPickup(protocol::ObjectId itemId, protocol::PlayerSlot slot)
{
    assert(itemId != 0);
    assert(slot < m_playerObjectIds.size());

    auto* player = GetPlayer(slot);
    assert(player != nullptr);

    auto& session = CCore::GetInst()->GetSession();
    if (!session.IsHost() ||
        session.GetState() != network::SessionState::Started ||
        !m_playerSpawnsQueued)
    {
        return false;
    }

    if (player->IsDead() || !player->IsAlivePlayer())
    {
        return false;
    }

    const int scoreBefore = CRankMgr::GetInst()->CurrentRanking.Score;
    player->ApplyItemReward();
    const int scoreAfter = CRankMgr::GetInst()->CurrentRanking.Score;

    protocol::CombatEventPayload combat{};
    combat.eventId = m_nextSpawnEventId + 1;
    combat.kind = protocol::CombatEventKind::ITEM_PICKED;
    combat.instigatorPlayerId = slot;
    combat.sourceObjectId = itemId;
    combat.targetObjectId = m_playerObjectIds[slot];
    combat.confirmedHp = player->getHP();
    combat.updatedPlayerLevel = static_cast<std::uint8_t>(player->getLv());
    combat.scoreDelta = scoreAfter - scoreBefore;
    combat.teamScore = static_cast<std::uint32_t>(scoreAfter);

    protocol::Message message{};
    message.header.type = protocol::MessageType::COMBAT_EVENT;
    message.header.sessionId = session.GetSessionId();
    message.header.sequence = combat.eventId;
    message.header.hostTick = m_hostTick;
    message.payload = combat;
    m_pendingItemEvents.push_back(message);

    m_nextSpawnEventId = combat.eventId;

#ifdef _DEBUG
    LogItemPickupApplied(session, combat);
#endif
    return true;
}

void CScene_Stage01::ReportMonsterCombat(protocol::ObjectId monsterId, protocol::ObjectId missileId, protocol::PlayerSlot ownerSlot, std::int32_t confirmedHp, bool died, std::int32_t scoreDelta, std::uint32_t teamScore)
{
    auto& session = CCore::GetInst()->GetSession();
    if (!session.IsHost() || session.GetState() != network::SessionState::Started
        || monsterId == 0)
        return;

    protocol::CombatEventPayload combat{};
    combat.eventId = ++m_nextSpawnEventId;
    combat.kind = protocol::CombatEventKind::DAMAGE_APPLIED;
    combat.instigatorPlayerId = ownerSlot;
    combat.sourceObjectId = missileId;
    combat.targetObjectId = monsterId;
    combat.confirmedHp = confirmedHp;
    combat.scoreDelta = scoreDelta;
    combat.teamScore = teamScore;

    protocol::Message msg{};
    msg.header.type = protocol::MessageType::COMBAT_EVENT;
    msg.header.sessionId = session.GetSessionId();
    msg.header.sequence = combat.eventId;
    msg.header.hostTick = m_hostTick;
    msg.payload = combat;
    m_pendingItemEvents.push_back(msg);

    if (died) {
        combat.eventId = ++m_nextSpawnEventId;
        combat.kind = protocol::CombatEventKind::ENTITY_DIED;
        msg.header.sequence = combat.eventId;
        msg.payload = combat;
        m_pendingItemEvents.push_back(msg);
    }

}

void CScene_Stage01::QueueDespawn(protocol::ObjectId objectId, protocol::DespawnReason reason)
{
    auto& session = CCore::GetInst()->GetSession();
    if (!session.IsHost() || session.GetState() != network::SessionState::Started
        || objectId == 0)
        return;
    protocol::DespawnPayload despawn{};
    despawn.eventId = ++m_nextSpawnEventId;
    despawn.objectId = objectId;
    despawn.reason = reason;

    protocol::Message msg{};
    msg.header.type = protocol::MessageType::DESPAWN;
    msg.header.sessionId = session.GetSessionId();
    msg.header.sequence = despawn.eventId;
    msg.header.hostTick = m_hostTick;
    msg.payload = despawn;
    m_pendingItemEvents.push_back(msg);
}

void CScene_Stage01::BeginHostPlay()
{
    auto& session = CCore::GetInst()->GetSession();

    m_hostTick = session.GetStartInfo().startHostTick;

    // 이번 위치 동기화에서 P0와 P1에 사용할 고정 ID.
    m_playerObjectIds = { 1, 2 };

    bool allQueued = true;

    for (protocol::PlayerSlot slot = 0; slot < protocol::MAX_PROTOCOL_PLAYERS; ++slot)
    {
        const auto* player = GetPlayer(slot);
        if (!player)
        {
            allQueued = false;
            break;
        }

        const auto record = player->ExportStateRecord(m_playerObjectIds[slot]);

        protocol::SpawnPayload spawn{};
        spawn.eventId = static_cast<protocol::EventId>(slot) + 1;
        spawn.objectId = record.objectId;
        spawn.kind = protocol::NetObjectKind::PLAYER;
        spawn.subType = record.subType;
        spawn.ownerPlayerId = slot;
        spawn.pos = record.pos;
        spawn.vel = record.vel;
        spawn.hp = record.hp;

        const auto result = session.SendSpawn(spawn, m_hostTick);
        if (result != net::SendResult::Queued)
        {
#ifdef _DEBUG
            std::cerr << "[HOST] Player SPAWN enqueue failed, slot=" << static_cast<int>(slot) << ", result=" << static_cast<int>(result) << '\n';
#endif
            allQueued = false;
            break;
        }
    }

    m_playerSpawnsQueued = allQueued;


    // 충돌 지정
    // Player 그룹과 Monster 그룹간의 충돌체크
    CColliderMgr::GetInst()->CheckGroup(GROUP_TYPE::PLAYER, GROUP_TYPE::PROJ_MONSTER);
    CColliderMgr::GetInst()->CheckGroup(GROUP_TYPE::MONSTER, GROUP_TYPE::PROJ_PLAYER);
    CColliderMgr::GetInst()->CheckGroup(GROUP_TYPE::PLAYER, GROUP_TYPE::ITEM);
    CColliderMgr::GetInst()->CheckGroup(GROUP_TYPE::PLAYER, GROUP_TYPE::MONSTER);
}

void CScene_Stage01::EvaluateHostMatchOutcome()
{
    if (m_bClear || m_bFail)
        return;

    bool anyAlive = false;
    for (PlayerId slot = 0; slot < MAX_PLAYERS; ++slot) {
        if (auto* player = GetPlayer(slot); player && player->IsAlivePlayer()) {
            anyAlive = true;
            break;
        }
    }

    const auto outcome = host::EvaluateTeamOutcome(m_bBossDefeated, !anyAlive);
    if (!outcome.has_value())
        return;

    if (*outcome == protocol::MatchOutcome::VICTORY) {
        Clear();
    }
    else if (*outcome == protocol::MatchOutcome::DEFEAT) {
        Fail();
    }
}

void CScene_Stage01::SendMatchResultOnce(protocol::MatchOutcome outcome)
{
    if (m_bResultSent)
        return;

    auto& session = CCore::GetInst()->GetSession();
    if (!session.IsHost())
        return;

    protocol::ResultPayload result{};
    result.eventId = ++m_nextSpawnEventId;
    result.outcome = outcome;
    result.finalTeamScore = static_cast<std::uint32_t>(
        CRankMgr::GetInst()->CurrentRanking.Score > 0
        ? CRankMgr::GetInst()->CurrentRanking.Score
        : 0);

    protocol::Message message{};
    message.header.type = protocol::MessageType::RESULT;
    message.header.sessionId = session.GetSessionId();
    message.header.hostTick = m_hostTick;
    message.header.sequence = result.eventId;
    message.payload = result;

    const auto sendResult = session.EnqueueSend(net::Channel::Tcp, message);
    if (sendResult == net::SendResult::Queued) {
        m_bResultSent = true;
    }
#ifdef _DEBUG
    else {
        std::cerr << "[HOST] RESULT enqueue failed"
            << ", sessionId=" << message.header.sessionId
            << ", hostTick=" << message.header.hostTick
            << ", eventId=" << result.eventId
            << ", sendResult=" << static_cast<int>(sendResult) << '\n';
    }
#endif
}

bool CScene_Stage01::FlushItemEvents()
{
    auto& session = CCore::GetInst()->GetSession();
    while (!m_pendingItemEvents.empty())
    {
        const auto result = session.EnqueueSend(net::Channel::Tcp, m_pendingItemEvents.front());
        if (result != net::SendResult::Queued)
        {
#ifdef _DEBUG
            std::cerr << "[HOST] ITEM_PICKED enqueue failed, result=" << static_cast<int>(result) << '\n';
#endif
            return false;
        }
        m_pendingItemEvents.pop_front();
    }
    return true;
}

void CScene_Stage01::OnObjectAdded(CObject* _pObj, GROUP_TYPE _eType)
{
    auto& session = CCore::GetInst()->GetSession();
    if (!session.IsHost() || session.GetState() !=
        network::SessionState::Started)
        return;

    if (_pObj == nullptr || _pObj->IsDead())
        return;

    if (_eType != GROUP_TYPE::PROJ_PLAYER
        && _eType != GROUP_TYPE::PROJ_MONSTER
        && _eType != GROUP_TYPE::MONSTER
        && _eType != GROUP_TYPE::ITEM
        && !(_eType == GROUP_TYPE::DEAD_PLAYER && dynamic_cast<CPlayerDead*>(_pObj)))
    {
        return;
    }

    const protocol::ObjectId objectId = _pObj->GetObjectId();
    if (objectId == 0 || objectId <= m_lastQueuedSpawnObjectId)
        return;

    m_lastQueuedSpawnObjectId = objectId;
    m_pendingSpawns.push_back(_pObj->ExportSpawnPayload(0));
}

void CScene_Stage01::update()
{
    if (m_entryFadeAlpha > 0.f && !CSceneMgr::GetInst()->IsLoadingTransitionActive())
    {
        if (m_waitingForEntryFade)
        {
            m_waitingForEntryFade = false;
        }
        else
        {
            m_entryFadeAlpha = std::clamp(m_entryFadeAlpha - fDT / EntryFadeDuration, 0.f, 1.f);
        }
    }

    auto& session = CCore::GetInst()->GetSession();
    using network::SessionState;
    const auto state = session.GetState();

    if (state == SessionState::Loading)
    {
        if (session.NotifyLocalLoaded() && session.IsHost() && session.HasClientLoaded())
        {
            constexpr std::uint32_t INITIAL_HOST_TICK = 120;
            const auto result = session.RequestStart(INITIAL_HOST_TICK);

#ifdef _DEBUG
            std::cout << "[HOST] START requested. result="
                << static_cast<int>(result) << '\n';
#endif
        }

        return;
    }

    if (state == SessionState::Started && session.IsHost() && !m_hostPlayInitialized)
    {
        BeginHostPlay();
        m_hostPlayInitialized = true;
    }

    if (!m_bFail)
    {
        CScene::update();
        m_dAcc += fDT;
    }

    if (session.IsHost() && state == network::SessionState::Started)
    {
        // Send SPAWN before combat events that reference the new objects.
        while (!m_pendingSpawns.empty())
        {
            auto spawn = m_pendingSpawns.front();
            spawn.eventId = m_nextSpawnEventId + 1;
            const auto result = session.SendSpawn(spawn, m_hostTick);
            if (result == net::SendResult::Queued)
            {
                m_nextSpawnEventId = spawn.eventId;
            }
            else if (result == net::SendResult::QueueFull)
            {
                return;
            }
            else
            {
#ifdef _DEBUG
                std::cerr << "[HOST] SPAWN dropped due to error, objectId=" << spawn.objectId
                    << ", result=" << static_cast<int>(result) << '\n';
#endif
            }
            m_pendingSpawns.pop_front();
        }

        if (!FlushItemEvents())
            return;
    }

    if (m_bClear)
    {
        if (m_fClearAcc == 0.f)
        {
            SendMatchResultOnce(protocol::MatchOutcome::VICTORY);

            m_pHud->SetActive(false);
            CRankMgr::GetInst()->SetPlayer(nullptr);
            CSoundMgr::GetInst()->PlayBGM(m_pClearTheme, false);

            // const Vec2 resolution = CCore::GetInst()->GetResolution();
            // auto clearCard = new CStageCard(false, L"Stage Clear");
            // clearCard->SetName(L"Stage Clear Card");
            // clearCard->SetPos(Vec2(resolution.x / 2.0f, resolution.y / 2.0f));
            // clearCard->SetScale(Vec2(resolution.x, resolution.y));
            // AddObject(clearCard, GROUP_TYPE::UI);
            m_pClearCard = CResMgr::GetInst()->LoadTexture(L"Clear Card", L"texture\\Stage Cards\\StageClear\\slice_16.png");

            for (protocol::PlayerSlot slot = 0; slot < protocol::MAX_PROTOCOL_PLAYERS; ++slot)
            {
                if (auto* player = GetPlayer(slot))
                    player->setclear();
            }
        }
        m_fClearAcc += fDT;
        if (m_fClearAcc >= ClearFadeDuration)
        {
            ChangeScene(SCENE_TYPE::CREDITS);
        }
    }
    else if (m_bFail)
    {
        if (m_fClearAcc == 0.f)
        {
            SendMatchResultOnce(protocol::MatchOutcome::DEFEAT);

            m_pHud->SetActive(false);

            CSoundMgr::GetInst()->PlayBGM(m_pFailTheme, false);

            // const Vec2 resolution = CCore::GetInst()->GetResolution();
            // CCamera::GetInst()->FadeOut(5.f);
            // todo stage clear 패널 띄우기
            // auto failCard = new CStageCard(false, L"StageFail");
            // failCard->SetName(L"Stage Fail Card");
            // failCard->SetPos(Vec2(resolution.x / 2.0f, resolution.y / 2.0f));
            // failCard->SetScale(Vec2(resolution.x, resolution.y));
            // AddObject(failCard, GROUP_TYPE::UI);
            m_pFailCard = CResMgr::GetInst()->LoadTexture(L"Fail Card", L"texture\\Stage Cards\\StageFail\\slice_16.png");
        }
        m_fClearAcc += fDT;

        /*
        if (KEY_TAP(KEY::SPACE))
        {
            CPlayer* player = CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
            player->FullHP();
            m_bFail = false;
            m_fClearAcc = 0.f;
            m_pFailCard = nullptr;
            if (m_dAcc >= 130.f)
            {
                CSoundMgr::GetInst()->PlayBGM(m_pBossTheme);
            }
            else
            {
                CSoundMgr::GetInst()->PlayBGM(m_pStageTheme);
            }

            m_pHud->SetActive(true);

            //CCamera::GetInst()->FadeIn(0.5f);
        }
        */
        if (m_fClearAcc >= FailCardDuration + FailFadeDuration)
        {
            CRankMgr::GetInst()->SetPlayer(nullptr);
            ChangeScene(SCENE_TYPE::TITLE);
        }
    }
    else
    {
        if (state == SessionState::Disconnected || state == SessionState::Failed)
        {
            // TODO: 상대 연결 끊어지거나 종료했을 때 연결
#ifdef _DEBUG
            const auto& event = session.GetLastNetworkEvent();
            const char* reason = "Network failed";

            if (state == SessionState::Disconnected)
            {
                if (event.socketError == WSAETIMEDOUT)
                    reason = "Heartbeat timeout";
                else if (event.socketError == 0)
                    reason = "Peer closed the TCP connection";
                else
                    reason = "Connection lost";
            }

            std::cout << (session.IsHost() ? "[HOST]" : "[CLIENT]")
                << " [Stage01] " << reason
                << ", socketError=" << event.socketError
                << ", codecError=" << static_cast<int>(event.codecError)
                << '\n';
#endif
            if (m_fClearAcc == 0.f)
            {
                if (session.IsHost())
                {
                    protocol::ResultPayload result{};
                    result.eventId = ++m_nextSpawnEventId;
                    result.outcome = protocol::MatchOutcome::DEFEAT;
                    result.finalTeamScore = static_cast<std::uint32_t>(
                        CRankMgr::GetInst()->CurrentRanking.Score > 0
                        ? CRankMgr::GetInst()->CurrentRanking.Score
                        : 0);
                    protocol::Message message{};
                    message.header.type = protocol::MessageType::RESULT;
                    message.header.sessionId = session.GetSessionId();
                    message.header.hostTick = m_hostTick;
                    message.header.sequence = result.eventId;
                    message.payload = result;
                    session.EnqueueSend(net::Channel::Tcp, message);
                }

                m_pHud->SetActive(false);

                CSoundMgr::GetInst()->PlayBGM(m_pFailTheme, false);

                m_pFailCard = CResMgr::GetInst()->LoadTexture(L"Connection Lost Card", L"texture\\Stage Cards\\ConnectionLost.png");
            }
            m_fClearAcc += fDT;

            /*
            if (KEY_TAP(KEY::SPACE))
            {
                CPlayer* player = CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
                player->FullHP();
                m_bFail = false;
                m_fClearAcc = 0.f;
                m_pFailCard = nullptr;
                if (m_dAcc >= 130.f)
                {
                    CSoundMgr::GetInst()->PlayBGM(m_pBossTheme);
                }
                else
                {
                    CSoundMgr::GetInst()->PlayBGM(m_pStageTheme);
                }

                m_pHud->SetActive(true);

                //CCamera::GetInst()->FadeIn(0.5f);
            }
            */
            if (m_fClearAcc >= FailCardDuration + FailFadeDuration)
            {
                CRankMgr::GetInst()->SetPlayer(nullptr);
                ChangeScene(SCENE_TYPE::TITLE);
            }
        }

        if (state != SessionState::Started)
            return;

        if (KEY_TAP(KEY::SPACE))
        {
            for (protocol::PlayerSlot slot = 0; slot < protocol::MAX_PROTOCOL_PLAYERS; ++slot)
            {
                if (auto* player = GetPlayer(slot))
                    player->isMujeok = true;
            }
        }

        // 입력 생성
        const auto moveBits = CPlayer::ReadMoveBits();
        std::uint8_t fireHeld = 0;
        if (KEY_HOLD(KEY::S)) fireHeld = 1;

        if (session.IsHost())
        {
            using Clock = std::chrono::steady_clock;
            const auto nowMs = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());

            // 세션이 승인한 참가자 입력을 호스트 입력 처리기에 전달한다.
            protocol::PlayerInputPayload input{};
            std::uint64_t receivedTimeMs = 0;
            while (session.TryReceivePlayerInput(input, receivedTimeMs)) { m_hostInput.SubmitPlayerInput(input, receivedTimeMs); }

            if (!m_bFail)
            {
                m_hostTickAccumulator += CTimeMgr::GetInst()->GetDT();

                for (std::uint32_t tick = 0; tick < host::MAX_TICKS_PER_FRAME && m_hostTickAccumulator >= host::SIM_FIXED_DT; ++tick)
                {
                    m_hostInput.ApplyInputNeutralization(nowMs);

                    if (auto* p0 = GetPlayer(0); p0 && !p0->IsDead())
                        p0->Simulate(moveBits, fireHeld, host::SIM_FIXED_DT);

                    if (auto* p1 = GetPlayer(1); p1 && !p1->IsDead())
                    {
                        p1->Simulate(
                            m_hostInput.GetLastAppliedMoveBits(1),
                            m_hostInput.GetLastAppliedFireHeld(1),
                            host::SIM_FIXED_DT
                        );
                    }

                    ++m_hostTick;

                    const auto ticksSinceStart = m_hostTick - session.GetStartInfo().startHostTick;
                    if (m_playerSpawnsQueued && ticksSinceStart % host::STATE_PUBLISH_INTERVAL_TICKS == 0)
                    {
                        const auto* p0 = GetPlayer(0);
                        const auto* p1 = GetPlayer(1);

                        if (p0 && p1)
                        {
                            std::vector<protocol::ObjectStateRecord> states;
                            states.push_back(p0->ExportStateRecord(m_playerObjectIds[0]));
                            states.push_back(p1->ExportStateRecord(m_playerObjectIds[1]));

                            // 11/21: 활성 몬스터 상태를 같은 틱의 스냅샷에 추가
                            for (CObject* obj : GetGroupObject(GROUP_TYPE::MONSTER))
                            {
                                if (obj == nullptr || obj->IsDead() || obj->GetObjectId() == 0)
                                    continue;
                                states.push_back(static_cast<const CMonster*>(obj)->ExportStateRecord());
                            }

                            // 11/23: 가속/감속 특수 탄환(m_iType 2, 3) 상태를 스냅샷에 포함
                            for (CObject* obj : GetGroupObject(GROUP_TYPE::PROJ_MONSTER)) {
                                if (obj == nullptr || obj->IsDead() || obj->GetObjectId() == 0)
                                    continue;
                                const auto* missile = static_cast<const CMissile*>(obj);
                                if (missile->IsSpecialPattern())
                                    states.push_back(missile->ExportStateRecord());
                            }

                            const auto result = session.SendSnapshot(states, m_hostTick);

#ifdef _DEBUG
                            if (result != net::SendResult::Queued && result != net::SendResult::ReplacedPending && result != net::SendResult::QueueFull)
                                std::cerr << "[HOST] STATE_SNAPSHOT enqueue failed, tick=" << m_hostTick << ", result=" << static_cast<int>(result) << '\n';
#else
                            (void)result;
#endif
                        }
                    }
                    m_hostTickAccumulator -= host::SIM_FIXED_DT;
                }

                const double hostElapsedSec = static_cast<double>(m_hostTick - session.GetStartInfo().startHostTick) * host::SIM_FIXED_DT;
                for (auto& evt : m_vEvents)
                {
                    if (!evt.triggered && hostElapsedSec >= evt.triggerTime) // 11/15 m_dAcc를 hostElapsedSec으로 수정
                    {

                        if (hostElapsedSec >= 130.f && !m_bBossBgmPlayed)
                        {
                            CSoundMgr::GetInst()->PlayBGM(m_pBossTheme);
                            m_bBossBgmPlayed = true;
                        }

                        CMonster* pMon = CMonFactory::CreateMonster(evt.type, evt.mtype, evt.spawnPos, evt.targetPos, evt.exitPos);
                        pMon->SetName(L"Monster");
                        CreateObject(pMon, GROUP_TYPE::MONSTER);
                        evt.triggered = true;
                    }
                }

                EvaluateHostMatchOutcome();

            }
        }
        else // Client
        {
            const auto inputRate = session.GetStartInfo().inputRateHz;
            if (inputRate > 0)
            {
                const double interval = 1.0 / inputRate;
                m_inputSendAccumulator += CTimeMgr::GetInst()->GetDT();

                if (m_inputSendAccumulator >= interval)
                {
                    // 프레임이 밀려도 이번 키 상태는 한 번만 등록한다.
                    m_inputSendAccumulator = std::fmod(m_inputSendAccumulator, interval);

                    const auto result = session.SendPlayerInput(moveBits, fireHeld);
                }
            }

            network::ReceivedMessage received{};
            protocol::SpawnPayload spawn{};
            protocol::DespawnPayload despawn{};
            protocol::CombatEventPayload combat{};
            protocol::StateSnapshotPayload snapshot{};
            protocol::ResultPayload result{};
            protocol::PacketHeader header{};

            while (true)
            {
                bool consumed = false;

                if (session.TryReceiveSpawn(spawn, header))
                {
                    consumed = true;

                    if (spawn.kind == protocol::NetObjectKind::PLAYER && spawn.ownerPlayerId < m_remotePlayers.size())
                    {
                        auto& remote = m_remotePlayers[spawn.ownerPlayerId];
                        if (remote.objectId != spawn.objectId)
                        {
                            remote.objectId = spawn.objectId;
                            if (auto* player = GetPlayer(spawn.ownerPlayerId))
                                player->ResetNetworkPosition(Vec2(spawn.pos.x / 100.f, spawn.pos.y / 100.f));
                        }
                    }
                    else if (spawn.kind == protocol::NetObjectKind::PROJ_PLAYER ||
                        spawn.kind == protocol::NetObjectKind::PROJ_MONSTER)
                    {
                        const bool isMonsterProjectile = spawn.kind == protocol::NetObjectKind::PROJ_MONSTER;
                        if (isMonsterProjectile && FindObjectById(spawn.objectId) != nullptr)
                            continue;

                        const auto group = isMonsterProjectile ? GROUP_TYPE::PROJ_MONSTER : GROUP_TYPE::PROJ_PLAYER;
                        Vec2 vel(spawn.vel.x / 100.f, spawn.vel.y / 100.f);

                        float speed = vel.Length();

                        CMissile* pMissile = new CMissile();
                        pMissile->SetObjectId(spawn.objectId);
                        pMissile->init(group);
                        pMissile->SetPos(Vec2(spawn.pos.x / 100.f, spawn.pos.y / 100.f));
                        pMissile->SetScale(Vec2(25.f, 25.f));
                        pMissile->SetDir(speed > 0.f ? vel : Vec2(0.f, 1.f));
                        pMissile->ChangemType(static_cast<MISSILE_TYPE>(spawn.subType));
                        pMissile->SetVec(static_cast<int>(speed));
                        pMissile->SetType(spawn.patternType);
                        pMissile->SetOwnerPlayerId(spawn.ownerPlayerId);
                        pMissile->SetDamage(spawn.damage);
                        pMissile->SetName(isMonsterProjectile ? L"MsMissile" : L"Missile_Player");
                        AddObject(pMissile, group);
                    }
                    else if (spawn.kind == protocol::NetObjectKind::ITEM)
                    {
                        if (FindObjectById(spawn.objectId) == nullptr)
                        {
                            auto* item = new CItem();
                            item->ApplySpawnState(spawn);
                            AddObject(item, GROUP_TYPE::ITEM);
                        }
                    }
                    else if (spawn.kind == protocol::NetObjectKind::PLAYER_DEATH_EFFECT)
                    {
                        if (FindObjectById(spawn.objectId) == nullptr)
                        {
                            auto* effect = new CPlayerDead();
                            effect->SetObjectId(spawn.objectId);
                            effect->SetPos(Vec2(spawn.pos.x / 100.f, spawn.pos.y / 100.f));
                            AddObject(effect, GROUP_TYPE::DEAD_PLAYER);
                        }
                    }
                    else if (spawn.kind == protocol::NetObjectKind::MONSTER)
                    {
                        // spawn.objectId를 기준으로 중복 생성을 막고, 호스트와 같은 ID와 초기 위치로 몬스터를 등록
                        if (FindObjectById(spawn.objectId) == nullptr)
                        {
                            Vec2 initPos(spawn.pos.x / 100.f, spawn.pos.y / 100.f);
                            CMonster* pMon = CMonFactory::CreateMonster(
                                static_cast<MON_TYPE>(spawn.subType),
                                static_cast<MISSILE_PTRN>(spawn.patternType),
                                initPos, initPos, initPos);
                            pMon->SetObjectId(spawn.objectId);
                            pMon->SetConfirmedHP(static_cast<float>(spawn.hp));
                            pMon->SetName(L"Monster");
                            AddObject(pMon, GROUP_TYPE::MONSTER);
                        }
                    }
                    continue;
                }

                if (session.TryReceiveDespawn(despawn, header))
                {
                    consumed = true;

                    // despawn.objectId의 객체를 제거하는 코드 연결
                    if (CObject* obj = FindObjectById(despawn.objectId))
                    {
                        auto* mon = dynamic_cast<CMonster*>(obj);
                        if (despawn.reason != protocol::DespawnReason::DESTROYED || mon == nullptr || !mon->GetsignalDead())
                            DeleteObject(obj);
                    }
                }

                if (session.TryReceiveCombatEvent(combat, header))
                {
                    consumed = true;

                    if (combat.kind == protocol::CombatEventKind::ITEM_PICKED)
                    {
                        const auto slot = combat.instigatorPlayerId;
                        if (slot >= m_remotePlayers.size() ||
                            combat.updatedPlayerLevel < 1 ||
                            combat.updatedPlayerLevel > 5)
                        {
                            continue;
                        }

                        const auto& remote = m_remotePlayers[slot];
                        if (remote.objectId == 0 || remote.objectId != combat.targetObjectId)
                        {
                            continue;
                        }

                        auto* player = GetPlayer(slot);
                        assert(player != nullptr);

                        player->ApplyConfirmedItemLevel(combat.updatedPlayerLevel);

                        CRankMgr::GetInst()->CurrentRanking.Score =
                            static_cast<int>(combat.teamScore);

#ifdef _DEBUG
                        LogItemPickupApplied(session, combat);
#endif
                    }
                    else if (combat.kind == protocol::CombatEventKind::DAMAGE_APPLIED
                        || combat.kind == protocol::CombatEventKind::ENTITY_DIED)
                    {
                        if (combat.kind == protocol::CombatEventKind::DAMAGE_APPLIED && combat.sourceObjectId != 0)
                        {
                            auto* missile = dynamic_cast<CMissile*>(FindObjectById(combat.sourceObjectId));
                            if (missile == nullptr || missile->IsDead())
                            {
                                if (auto* target = FindObjectById(combat.targetObjectId))
                                {
                                    missile = new CMissile();
                                    missile->SetObjectId(combat.sourceObjectId);
                                    missile->SetPos(target->GetPos());
                                    missile->SetDir(Vec2(0.f, 1.f));
                                    missile->SetName(L"Missile_Player");
                                    AddObject(missile, GROUP_TYPE::PROJ_PLAYER);
                                }
                            }

                            if (missile != nullptr && !missile->IsDead())
                                missile->PlayHitEffect();
                        }

                        if (auto* mon = dynamic_cast<CMonster*>(FindObjectById(combat.targetObjectId)))
                        {
                            mon->SetConfirmedHP(static_cast<float>(combat.confirmedHp));
                            if (combat.kind == protocol::CombatEventKind::ENTITY_DIED)
                                mon->BeginDeath();
                        }

                        if (combat.kind == protocol::CombatEventKind::ENTITY_DIED)
                            CRankMgr::GetInst()->CurrentRanking.Score = static_cast<int>(combat.teamScore);
                    }
                }

                if (session.TryReceiveSnapshot(snapshot, header))
                {
                    consumed = true;

                    for (const auto& object : snapshot.objects)
                    {
                        if (object.kind == protocol::NetObjectKind::MONSTER)
                        {
                            if (auto* mon = dynamic_cast<CMonster*>(FindObjectById(object.objectId)))
                                mon->ApplyMonsterSnapshot(object, snapshot.snapshotTick);

                            continue;
                        }

                        if (object.kind == protocol::NetObjectKind::PROJ_MONSTER) {
                            if (auto* missile = dynamic_cast<CMissile*>
                                (FindObjectById(object.objectId))) {
                                if (!missile->IsDead() && missile->ApplySnapshotPosition(
                                    Vec2(object.pos.x / 100.f, object.pos.y / 100.f),
                                    snapshot.snapshotTick, SnapshotLerpDuration))
                                {
                                    missile->ApplySnapshotVelocity(
                                        Vec2(object.vel.x / 100.f, object.vel.y / 100.f));
                                }
                            }
                            continue;
                        }

                        if (object.kind != protocol::NetObjectKind::PLAYER)
                            continue;

                        for (protocol::PlayerSlot slot = 0; slot < m_remotePlayers.size(); ++slot)
                        {
                            auto& remote = m_remotePlayers[slot];

                            if (remote.objectId == 0 || remote.objectId != object.objectId)
                                continue;

                            if (auto* player = GetPlayer(slot))
                            {
                                const bool applied = slot == session.GetLocalPlayerId()
                                    ? player->ApplyLocalSnapshotPosition(object, snapshot.snapshotTick,
                                        (snapshot.snapshotTick - session.GetStartInfo().startHostTick) * host::SIM_FIXED_DT,
                                        SnapshotLerpDuration)
                                    : player->ApplySnapshotPosition(Vec2(object.pos.x / 100.f, object.pos.y / 100.f),
                                        snapshot.snapshotTick, object.alive ? SnapshotLerpDuration : 0.f);
                                if (!applied)
                                    break;

                                if (object.alive == 0) {
                                    player->setdead();
                                    player->SetRespawnInvincible(false);
                                }
                                else {
                                    player->SetRespawnInvincible(object.fsmState == 1);
                                }
                            }

                            break;
                        }
                    }
                }

                if (session.TryReceiveResult(result, header))
                {
                    CRankMgr::GetInst()->CurrentRanking.Score = static_cast<int>(result.finalTeamScore);

                    if (result.outcome == protocol::MatchOutcome::DEFEAT)
                        Fail();

                    else if (result.outcome == protocol::MatchOutcome::VICTORY)
                        Clear();

                    break;
                }

                if (!consumed)
                    break;
            }
        }
    }
}

void CScene_Stage01::render(HDC _dc)
{
    CScene::render(_dc);
    CBackground* back = (CBackground*)GetBackground();
    Vec2 backinfo = back->GetScale();
    Vec2 backPos = back->GetPos();
    POINT resolution = CCore::GetInst()->GetResolution();
    SelectGDI gdi(_dc, BRUSH_TYPE::BLACK);
    Rectangle(_dc, 0, 0
        , (int)(backPos.x - backinfo.x / 2)
        , (int)(backPos.y + backinfo.y / 2));
    Rectangle(_dc
        , (int)(backPos.x + backinfo.x / 2)
        , 0
        , (int)resolution.x
        , (int)resolution.y);

    if (m_pClearCard)
    {
        const Vec2 resolution = CCore::GetInst()->GetResolution();

        const Vec2 position = Vec2(resolution.x / 2.0f, resolution.y / 2.0f);  // 중심 위치
        const Vec2 scale = Vec2(resolution.x, resolution.y);     // 출력될 크기

        const int width = static_cast<int>(m_pClearCard->Width());   // 원본 너비
        const int height = static_cast<int>(m_pClearCard->Height());  // 원본 높이

        TransparentBlt(
            _dc,
            static_cast<int>(position.x - width / 2),
            static_cast<int>(position.y - height / 2),
            width,
            height,
            m_pClearCard->GetDC(),
            0, 0,
            width, height,
            RGB(255, 0, 255)
        );
    }

    if (m_pFailCard)
    {
        const Vec2 resolution = CCore::GetInst()->GetResolution();

        const Vec2 position = Vec2(resolution.x / 2.0f, resolution.y / 2.0f);  // 중심 위치
        const Vec2 scale = Vec2(resolution.x, resolution.y);     // 출력될 크기

        const int width = static_cast<int>(m_pFailCard->Width());   // 원본 너비
        const int height = static_cast<int>(m_pFailCard->Height());  // 원본 높이

        TransparentBlt(
            _dc,
            static_cast<int>(position.x - width / 2),
            static_cast<int>(position.y - height / 2),
            width,
            height,
            m_pFailCard->GetDC(),
            0, 0,
            width, height,
            RGB(255, 0, 255)
        );
    }

    float exitFadeAlpha = 0.f;
    if (m_bClear)
    {
        exitFadeAlpha = m_fClearAcc / ClearFadeDuration;
    }
    else if (m_fClearAcc >= FailCardDuration)
    {
        exitFadeAlpha = (m_fClearAcc - FailCardDuration) / FailFadeDuration;
    }

    const float fadeAlpha = (std::max)(m_entryFadeAlpha, std::clamp(exitFadeAlpha, 0.f, 1.f));
    if (fadeAlpha > 0.f && m_fadeTexture)
    {
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = static_cast<BYTE>(fadeAlpha * 255.f);
        AlphaBlend(_dc, 0, 0, resolution.x, resolution.y,
            m_fadeTexture->GetDC(), 0, 0, 1, 1, blend);
    }
}
void CScene_Stage01::Enter()
{
    // Warm the shared texture cache before combat, using the actual animation assets.
    // Only temporary playback objects are destroyed; CResMgr retains the textures.
    {
        CAnimator preload;
        preload.LoadAnimation(L"animation\\Boss1Enemy_Move.anim");
        preload.LoadAnimation(L"animation\\Boss1Enemy_Hit.anim");
        preload.LoadAnimation(L"animation\\Boss1Enemy_Dead.anim");
    }

    CRankMgr::GetInst()->CurrentRanking.Score = 0;

    m_pClearCard = nullptr;
    m_pFailCard = nullptr;

    Vec2 vResolution = CCore::GetInst()->GetResolution();

    // BackGround Object 추가
    CObject* pBackgroundObj = new CBackground;
    pBackgroundObj->SetPos(Vec2(vResolution.x / 2.f, vResolution.y / 2.f));
    pBackgroundObj->SetScale(Vec2(540.f, 960.f));
    pBackgroundObj->SetName(L"Background");
    AddObject(pBackgroundObj, GROUP_TYPE::BACKGROUND);

    RegisterBackground(pBackgroundObj);

    // Player 0 (Host) & Player 1 (Client) Object 추가 및 슬롯 등록
    CPlayer* pPlayer0 = new CPlayer;
    pPlayer0->SetPlayerId(0);
    pPlayer0->SetPos(Vec2(vResolution.x / 2.f - 60.f, vResolution.y + 200.f));
    pPlayer0->SetScale(Vec2(100.f, 100.f));
    pPlayer0->SetName(L"Player");
    AddObject(pPlayer0, GROUP_TYPE::PLAYER);
    RegisterPlayer(0, pPlayer0);
    CRankMgr::GetInst()->SetPlayer(pPlayer0);

    CPlayer* pPlayer1 = new CPlayer;
    pPlayer1->SetPlayerId(1);
    pPlayer1->SetPos(Vec2(vResolution.x / 2.f + 60.f, vResolution.y + 200.f));
    pPlayer1->SetScale(Vec2(100.f, 100.f));
    pPlayer1->SetName(L"Player");
    AddObject(pPlayer1, GROUP_TYPE::PLAYER);
    RegisterPlayer(1, pPlayer1);

    auto& session = CCore::GetInst()->GetSession();

    m_hostTickAccumulator = 0.0;
    m_hostInput.ResetSession(session.GetSessionId());
    m_inputSendAccumulator = 0.0;
    m_remotePlayers = {};
    m_pendingItemEvents.clear();
    m_bBossBgmPlayed = false;
    m_bBossDefeated = false;
    m_bResultSent = false;

    // Monster Object 추가

    InitEvents();


    // 타일 로딩
    //LoadTile(L"Tile\\Start.tile");

    //Camera 지정
    CCamera::GetInst()->SetLookAt(vResolution / 2.f);

    m_fadeTexture = CResMgr::GetInst()->CreateTexture(L"Stage Black Fade", 1, 1);
    PatBlt(m_fadeTexture->GetDC(), 0, 0, 1, 1, BLACKNESS);
    m_entryFadeAlpha = 1.f;
    m_waitingForEntryFade = true;
    m_dAcc = 0.f;

    CSoundMgr::GetInst()->PlayBGM(m_pStageTheme);



    m_pHud = new CStageHUD;
    m_pHud->SetActive(true);
    AddObject(m_pHud, GROUP_TYPE::UI);

    m_playerObjectIds = {};
    m_hostTick = 0;
    m_playerSpawnsQueued = false;
    m_nextSpawnEventId = 2;
    m_lastQueuedSpawnObjectId = 0;
    m_pendingSpawns.clear();
    m_hostPlayInitialized = false;
}

void CScene_Stage01::Exit()
{
    CCore::GetInst()->GetSession().Stop();
    m_pendingItemEvents.clear();
    DeleteAll();
    m_dAcc = 0;
    m_bClear = false;
    m_bFail = false;
    m_fClearAcc = 0.f;
    m_vEvents.clear();
    m_pendingSpawns.clear();
    m_bBossBgmPlayed = false;
    m_bBossDefeated = false;
    m_bResultSent = false;
    CColliderMgr::GetInst()->Reset();
}

void CScene_Stage01::InitEvents()
{
    Vec2 vResolution = CCore::GetInst()->GetResolution();
    Vec2 vBackPos = GetBackground()->GetPos();
    Vec2 vBackScale = GetBackground()->GetScale();
    RECT rBackrect = { (vBackPos.x - vBackScale.x / 2.f),(vBackPos.y - vBackScale.y / 2.f),(vBackPos.x + vBackScale.x / 2.f),(vBackPos.x + vBackScale.y / 2.f) };


    for (int i = 1; i < 80; ++i)
    {
        if (i % 2 == 0)
        {
            m_vEvents.push_back({ 5.0f * i,MON_TYPE::NORMAL3,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left + 200.f, -100.f),Vec2((float)rBackrect.left + 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.right + 400.f,vResolution.y) });
            m_vEvents.push_back({ (5.0f * i) + 0.5f,MON_TYPE::NORMAL4,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left + 150.f, -100.f),Vec2((float)rBackrect.left + 150.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 400.f,vResolution.y) });
            m_vEvents.push_back({ (5.0f * i) + 1.f,MON_TYPE::NORMAL4, MISSILE_PTRN::NONE,Vec2((float)rBackrect.left + 100.f, -100.f),Vec2((float)rBackrect.left + 100.f,(float)rBackrect.top + 100.f),Vec2((float)rBackrect.right + 400.f,vResolution.y) });

        }
        else
        {
            m_vEvents.push_back({ 5.0f * i,MON_TYPE::NORMAL3,MISSILE_PTRN::NONE, Vec2((float)rBackrect.right - 200.f , -100.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
            m_vEvents.push_back({ (5.0f * i) + 0.5f,MON_TYPE::NORMAL4,MISSILE_PTRN::NONE, Vec2((float)rBackrect.right - 150.f , -100.f),Vec2((float)rBackrect.right - 150.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
            m_vEvents.push_back({ (5.0f * i) + 1.f,MON_TYPE::NORMAL4, MISSILE_PTRN::NONE,Vec2((float)rBackrect.right - 100.f , -100.f),Vec2((float)rBackrect.right - 100.f,(float)rBackrect.top + 100.f),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
        }

    }


    // PTRN 2
    {
        for (int i = 0; i < 10; ++i)
        {
            if (i % 2 == 0)
            {
                m_vEvents.push_back({ 8.0f + 0.2f * i,MON_TYPE::NORMAL2, MISSILE_PTRN::PTRN1,Vec2((float)rBackrect.left + 200.f , -100.f),Vec2((float)rBackrect.left + 200.f,(float)rBackrect.top + 500.f - 30.f * i),Vec2((float)rBackrect.right + 400.f,vResolution.y) });
            }
            else
            {
                m_vEvents.push_back({ 8.0f + 0.2f * i,MON_TYPE::NORMAL2, MISSILE_PTRN::PTRN1,Vec2((float)rBackrect.left + 250.f , -100.f),Vec2((float)rBackrect.left + 150.f,(float)rBackrect.top + 500.f - 30.f * i),Vec2((float)rBackrect.right + 400.f,vResolution.y) });
            }
        }
        for (int i = 0; i < 10; ++i)
        {
            if (i % 2 == 0)
            {
                m_vEvents.push_back({ 13.0f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN1, Vec2((float)rBackrect.right - 200.f , -100.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 500.f - 30.f * i),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
            }
            else
            {
                m_vEvents.push_back({ 13.0f + 0.2f * i,MON_TYPE::NORMAL2, MISSILE_PTRN::PTRN1,Vec2((float)rBackrect.right - 250.f , -100.f),Vec2((float)rBackrect.right - 150.f,(float)rBackrect.top + 500.f - 30.f * i),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
            }
        }


    }

    // PTRN 3
    {
        for (int i = 0; i < 10; ++i)
        {
            m_vEvents.push_back({ 15.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left - 250.f,(float)rBackrect.bottom / 2.f),Vec2(vBackPos.x,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 250.f,(float)rBackrect.bottom / 2.f) });

        }
        for (int i = 0; i < 10; ++i)
        {
            m_vEvents.push_back({ 18.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE,Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f),Vec2(vBackPos.x,(float)rBackrect.top + 250.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f) });

        }

    }


    // PTRN 3
    {
        m_vEvents.push_back({ 20.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN1, Vec2((float)rBackrect.left - 200.f,  (float)rBackrect.top + 250.f),Vec2((float)rBackrect.left + 200.f, (float)rBackrect.top + 200.f),Vec2((float)rBackrect.left + 100.f, -400.f) });
        m_vEvents.push_back({ 25.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN1,Vec2((float)rBackrect.right + 200.f, (float)rBackrect.top + 250.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.right - 100.f, -400.f) });

    }

    for (int i = 0; i < 4; ++i)
    {
        if (i % 2 == 0)
        {
            m_vEvents.push_back({ 35.0f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.right - 200.f , -100.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 300.f - 50.f * i),Vec2((float)rBackrect.right + 400.f,vResolution.y / 2.f) });
        }
        else
        {
            m_vEvents.push_back({ 35.0f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.right - 250.f , -100.f),Vec2((float)rBackrect.right - 150.f,(float)rBackrect.top + 300.f - 50.f * i),Vec2((float)rBackrect.right + 400.f,vResolution.y / 2.f) });
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        if (i % 2 == 0)
        {
            m_vEvents.push_back({ 35.5f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.left + 200.f , -100.f),Vec2((float)rBackrect.left + 200.f,(float)rBackrect.top + 300.f - 50.f * i),Vec2((float)rBackrect.left - 400.f,vResolution.y / 2.f) });
        }
        else
        {
            m_vEvents.push_back({ 36.0f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.left + 250.f , -100.f),Vec2((float)rBackrect.left + 150.f,(float)rBackrect.top + 300.f - 50.f * i),Vec2((float)rBackrect.left - 400.f,vResolution.y / 2.f) });
        }
    }



    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 35.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x + 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 40.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE,Vec2((float)rBackrect.right + 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x - 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });
    }
    for (int i = 0; i < 4; ++i)
    {
        m_vEvents.push_back({ 45.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f),Vec2(vBackPos.x,(float)rBackrect.bottom / 2.f - 50.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f) });

    }
    for (int i = 0; i < 4; ++i)
    {
        m_vEvents.push_back({ 47.5f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f - 100.f),Vec2(vBackPos.x,(float)rBackrect.bottom / 2.f - 150.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f - 100.f) });

    }
    for (int i = 0; i < 4; ++i)
    {
        m_vEvents.push_back({ 50.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f - 300.f),Vec2(vBackPos.x,(float)rBackrect.bottom / 2.f - 350.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f - 300.f) });

    }
    for (int i = 0; i < 4; ++i)
    {
        m_vEvents.push_back({ 52.5f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f - 500.f),Vec2(vBackPos.x,(float)rBackrect.bottom / 2.f - 550.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f - 500.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 55.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x + 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 57.5f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE,Vec2((float)rBackrect.right + 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x - 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });
    }

    // PTRN 3
    {
        m_vEvents.push_back({ 60.0f,MON_TYPE::EPIC,MISSILE_PTRN::PTRN1, Vec2(vBackPos.x,  (float)rBackrect.top - 200.f),Vec2((float)vBackPos.x, (float)rBackrect.bottom / 2.f - 300.f),Vec2((float)vBackPos.x, -400.f) });
    }

    // PTRN 4
    {
        m_vEvents.push_back({ 80.0f,MON_TYPE::EPIC2,MISSILE_PTRN::PTRN1, Vec2((float)rBackrect.left + 150.f,  (float)rBackrect.top - 200.f),Vec2((float)rBackrect.left + 230.f, (float)rBackrect.bottom / 2.f - 250.f),Vec2((float)rBackrect.left + 100.f, -400.f) });
        m_vEvents.push_back({ 80.5f,MON_TYPE::EPIC2,MISSILE_PTRN::PTRN1, Vec2((float)rBackrect.right - 150.f,  (float)rBackrect.top - 200.f),Vec2((float)rBackrect.right - 180.f, (float)rBackrect.bottom / 2.f - 300.f),Vec2((float)rBackrect.right - 100.f, -400.f) });
    }

    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 90.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3, Vec2((float)rBackrect.left + 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.left + 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 95.5f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3,Vec2((float)rBackrect.right - 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.right - 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });
    }

    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 105.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3,Vec2((float)rBackrect.right - 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.right - 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });


    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 110.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3, Vec2((float)rBackrect.left + 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.left + 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });
    }
    m_vEvents.push_back({ 100.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.left + 100.f,  (float)rBackrect.top - 200.f),Vec2((float)rBackrect.left + 150.f, (float)rBackrect.top + 200.f),Vec2((float)rBackrect.right + 400.f, -400.f) });
    m_vEvents.push_back({ 105.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.right - 200.f, (float)rBackrect.top - 200.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.left - 400.f, -400.f) });
    m_vEvents.push_back({ 115.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.left + 100.f,  (float)rBackrect.top - 200.f),Vec2((float)rBackrect.left + 150.f, (float)rBackrect.top + 200.f),Vec2((float)rBackrect.right + 400.f, -400.f) });
    m_vEvents.push_back({ 120.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.right - 200.f, (float)rBackrect.top - 200.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.left - 400.f, -400.f) });
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 115.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x + 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 117.5f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE,Vec2((float)rBackrect.right + 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x - 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });
    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 116.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3, Vec2((float)rBackrect.left + 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.left + 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 121.5f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3,Vec2((float)rBackrect.right - 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.right - 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });
    }









    m_vEvents.push_back({ 130.f,MON_TYPE::BOSS,MISSILE_PTRN::PTRN1,Vec2(vBackPos.x,  (float)rBackrect.top - 200.f),Vec2((float)vBackPos.x, (float)rBackrect.bottom / 2.f - 300.f),Vec2((float)vBackPos.x, -400.f) });

}


