#include "pch.h"

#include "CCore.h"
#include "CSessionMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CScene_Stage01.h"

#include "CMonster.h"
#include "CPlayer.h"
#include "CTimeMgr.h"
#include "CResMgr.h"
#include "CTexture.h"

#include "AI.h"

#include "CMissile.h"
#include "CCollider.h"

#include "CAnimator.h"
#include "CAnimation.h"

#include "CSound.h"
#include "CSoundMgr.h"

#include "CRankMgr.h"
#include "../host/HostSimCore.h"

namespace
{
    constexpr float MonsterCorrectionDuration = host::SIM_FIXED_DT * host::STATE_PUBLISH_INTERVAL_TICKS;
    constexpr float MonsterPredictionLimit = 2.f * MonsterCorrectionDuration;
}

CMonster::CMonster()
	: m_tInfo{}
	, dAccTime0(0.)
	, dAccTime1(0.)
	, m_signaldead(false)
    , m_pAI(nullptr)
    , m_bHit(false)
{

    CreaeteCollider();
    //GetCollider()->SetScale(Vec2(50.f, 50.f));

	//Texture 로딩하기
    CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"N34Enemy_Dead", L"texture\\Enemies\\normal34_Enemy_Dead.png");
    CreaeteAnimator();
    
    m_pHurtSE = CResMgr::GetInst()->LoadSound(L"Enemy Dead", L"sound\\SE\\SE_EnemyHurt.mp3");
    m_pDeadSE = CResMgr::GetInst()->LoadSound(L"Enemy Dead", L"sound\\SE\\SE_EnemyHurt.mp3");
    {
        // m_pTex = CResMgr::GetInst()->LoadTexture(L"E2Enemy_Hit", L"texture\\Enemies\\Epic2_Enemy_Hit.png");
        //GetAnimator()->CreateAnimation(L"E2Enemy_Hit", m_pTex, Vec2(0.f, 0.f), Vec2(450.f, 450.f), Vec2(450.f, 0.f), 0.05f, 40);
        ////Animation 저장
        // GetAnimator()->FindAnimation(L"E2Enemy_Hit")->Save(L"animation\\E2Enemy_Hit.anim");

        // m_pTex = CResMgr::GetInst()->LoadTexture(L"N1Enemy_Hit", L"texture\\Enemies\\normal1H_enemy_Hit.png");
        // GetAnimator()->CreateAnimation(L"N1Enemy_Hit", m_pTex, Vec2(0.f, 0.f), Vec2(400.f, 400.f), Vec2(400.f, 0.f), 0.05f, 40);
        // //Animation 저장
        // GetAnimator()->FindAnimation(L"N1Enemy_Hit")->Save(L"animation\\N1Enemy_Hit.anim");

        // m_pTex = CResMgr::GetInst()->LoadTexture(L"N2Enemy_Hit", L"texture\\Enemies\\normal2_enemy_Hit.png");
        // GetAnimator()->CreateAnimation(L"N2Enemy_Hit", m_pTex, Vec2(0.f, 0.f), Vec2(150.f, 150.f), Vec2(150.f, 0.f), 0.05f, 40);
        // //Animation 저장
        // GetAnimator()->FindAnimation(L"N2Enemy_Hit")->Save(L"animation\\N2Enemy_Hit.anim");


        // m_pTex = CResMgr::GetInst()->LoadTexture(L"N3Enemy_Hit", L"texture\\Enemies\\normal3_enemy_Hit.png");
        // GetAnimator()->CreateAnimation(L"N3Enemy_Hit", m_pTex, Vec2(0.f, 0.f), Vec2(100.f, 100.f), Vec2(100.f, 0.f), 0.05f, 20);
        // //Animation 저장
        // GetAnimator()->FindAnimation(L"N3Enemy_Hit")->Save(L"animation\\N3Enemy_Hit.anim");

        // m_pTex = CResMgr::GetInst()->LoadTexture(L"N4Enemy_Hit", L"texture\\Enemies\\normal3_enemy_Hit.png");
        // GetAnimator()->CreateAnimation(L"N4Enemy_Hit", m_pTex, Vec2(0.f, 0.f), Vec2(100.f, 100.f), Vec2(100.f, 0.f), 0.05f, 20);
        // //Animation 저장
        // GetAnimator()->FindAnimation(L"N4Enemy_Hit")->Save(L"animation\\N4Enemy_Hit.anim");

        // m_pTex = CResMgr::GetInst()->LoadTexture(L"E1Enemy_Hit", L"texture\\Enemies\\Epic1_Enemy_Hit.png");
        // GetAnimator()->CreateAnimation(L"E1Enemy_Hit", m_pTex, Vec2(0.f, 0.f), Vec2(300.f, 300.f), Vec2(300.f, 0.f), 0.05f, 40);
        // //Animation 저장
        // GetAnimator()->FindAnimation(L"E1Enemy_Hit")->Save(L"animation\\E1Enemy_Hit.anim");

        // m_pTex = CResMgr::GetInst()->LoadTexture(L"Boss1Enemy_Hit", L"texture\\Enemies\\Boss1_Enemy_Move.png");
        // GetAnimator()->CreateAnimation(L"Boss1Enemy_Hit", m_pTex, Vec2(0.f, 0.f), Vec2(400.f, 400.f), Vec2(400.f, 0.f), 0.05f, 60);
        // //Animation 저장
        // GetAnimator()->FindAnimation(L"Boss1Enemy_Hit")->Save(L"animation\\Boss1Enemy_Hit.anim");

        
    }
   
   
}

CMonster::~CMonster()
{
    if (nullptr != m_pAI)
    {
        delete m_pAI;
    }
}

void CMonster::ApplyMonsterSnapshot(const protocol::ObjectStateRecord& state, std::uint32_t tick)
{
    if (IsDead() || (m_signaldead && state.alive)
        || (m_lastSnapshotTick && tick <= *m_lastSnapshotTick))
        return;

    Vec2 position(state.pos.x / 100.f, state.pos.y / 100.f);
    m_snapshotVelocity = Vec2{};
    if (m_lastSnapshotTick && state.alive)
    {
        const auto tickGap = tick - *m_lastSnapshotTick;
        if (tickGap <= 2 * host::STATE_PUBLISH_INTERVAL_TICKS)
            m_snapshotVelocity = (position - m_snapshotPosition) / (tickGap * host::SIM_FIXED_DT);
    }

    m_lastSnapshotTick = tick;
    m_snapshotPosition = position;
    m_snapshotOffset = GetPos() - position;
    m_snapshotElapsed = 0.f;
    SetConfirmedHP(static_cast<float>(state.hp));

    if (!state.alive)
    {
        m_snapshotElapsed = MonsterPredictionLimit;
        SetPos(position);
    }
}

void CMonster::finalupdate()
{
    if (m_lastSnapshotTick && !IsDead() && !m_signaldead && m_snapshotElapsed < MonsterPredictionLimit)
    {
        m_snapshotElapsed = (std::min)(m_snapshotElapsed + fDT, MonsterPredictionLimit);
        const float remaining = (std::max)(1.f - m_snapshotElapsed / MonsterCorrectionDuration, 0.f);
        SetPos(m_snapshotPosition + m_snapshotVelocity * m_snapshotElapsed + m_snapshotOffset * remaining);
    }

    CObject::finalupdate();
}

void CMonster::PlayHitAnimation()
{
    if (IsDead() || m_signaldead)
        return;

    m_bHit = true;
    dAccTime0 = 0.f;

    const wchar_t* animationName;
    switch (m_tInfo.eMType)
    {
    case MON_TYPE::NORMAL2: animationName = L"N2Enemy_Hit"; break;
    case MON_TYPE::NORMAL3: animationName = L"N3Enemy_Hit"; break;
    case MON_TYPE::NORMAL4: animationName = L"N4Enemy_Hit"; break;
    case MON_TYPE::BOSS: animationName = L"Boss1Enemy_Hit"; break;
    case MON_TYPE::EPIC: animationName = L"E1Enemy_Hit"; break;
    case MON_TYPE::EPIC2: animationName = L"E2Enemy_Hit"; break;
    default: return;
    }

    auto* animator = GetAnimator();
    animator->LoadAnimation(std::wstring(L"animation\\") + animationName + L".anim");
    animator->Play(animationName, true);
}

void CMonster::BeginDeath()
{
    if (IsDead() || m_signaldead)
        return;

    m_signaldead = true;
    SetCollideroff();
    CSoundMgr::GetInst()->PlaySE(m_pDeadSE);
    m_pAI->ChangeState(MON_STATE::DEAD);
}

void CMonster::update()
{
    if (m_signaldead)
    {
        m_pAI->update();
        return;
    }

    if (m_bHit)
    {
        dAccTime0 += fDT;
        if (dAccTime0 > 0.05f)
        {
            dAccTime0 = 0.f;
            m_bHit = false;
            SetAnim(m_tInfo.eMType);
        }
    }

    if (!CCore::GetInst()->GetSession().IsHost()) // 11/21 클라이언트는 AI 이동 발사 자체 삭제 실행 안함
        return;

    m_pAI->update();
    Vec2 vBPos = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetPos();
    Vec2 vBScale = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetScale();
    Vec2 vMonPos = GetPos();
    if (vMonPos.x < vBPos.x - vBScale.x - 100.f
        || vMonPos.x > vBPos.x + vBScale.x + 100.f
        || vMonPos.y < vBPos.y - vBScale.y - 100.f
        || vMonPos.y > vBPos.y + vBScale.y + 100.f)
    {
        if (auto* stage = dynamic_cast<CScene_Stage01*>(CSceneMgr::GetInst()
            ->GetCurScene()))
        {
            stage->QueueDespawn(GetObjectId(),
                protocol::DespawnReason::OUT_OF_BOUNDS);
            DeleteObject(this);
        }
    }
   
}

void CMonster::render(HDC _dc)
{
	
	component_render(_dc);
}

void CMonster::CreateMissile(int type)
{
	Vec2 vMissilePos = GetPos();
	vMissilePos.y += GetScale().y / 2.f;
	vMissilePos.x += GetScale().x * type;
	CScene* CurScene = CSceneMgr::GetInst()->GetCurScene();
    CPlayer* pInfo = CurScene->SelectNearestAlivePlayer(vMissilePos);
    if (pInfo == nullptr)
    {
        return;
    }
	Vec2 vpPos = pInfo->GetPos();
	double dx = vMissilePos.x;
	double dy = vMissilePos.y;
	double distance = sqrt(dx * dx + dy * dy);
	if (vMissilePos.x - vpPos.x < 0)
	{
		vpPos.x = -(vMissilePos.x - vpPos.x);
	}
	else if (vMissilePos.x - vpPos.x > 0)
	{
		vpPos.x = -(vMissilePos.x - vpPos.x);
	}
	vpPos.y = vMissilePos.y - vpPos.y;
	// Missile Object
	CMissile* pMissile = new CMissile;
	pMissile->init(GROUP_TYPE::PROJ_MONSTER);
	pMissile->SetPos(vMissilePos);
	pMissile->SetScale(Vec2(25.f, 25.f));
	pMissile->SetDir(vpPos);
	pMissile->SetVec(500);
	pMissile->SetType(1);
	pMissile->SetName(L"MsMissile");

	CreateObject(pMissile, GROUP_TYPE::PROJ_MONSTER);
}


void CMonster::OnCollisionEnter(CCollider* _pOther)
{
	CObject* pOtherobj = _pOther->GetObj();
    /*CPlayer* pPlayerobj = (CPlayer*)CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
    int iPAtk = pPlayerobj->GetPlayerAtk();*/
    if (pOtherobj->GetName() == L"Missile_Player")
    {
        if (IsDead() || m_signaldead || m_tInfo.fHP <= 0 || pOtherobj->IsDead())
            return;

        CMissile* pMissile = static_cast<CMissile*>(pOtherobj); // 충돌한 탄환에서 직접 피해량 읽기
        pMissile->PlayHitEffect();
        const int iDamage = pMissile->GetDamage();

        m_tInfo.fHP = max(0.f, m_tInfo.fHP - iDamage);

        bool justDied = false;
        int scoreDelta = 0;

        if (m_tInfo.fHP <= 0 && m_signaldead == false)
        {
            BeginDeath();
            justDied = true;

            scoreDelta = ((int)m_tInfo.eMType) * 250;
            CRankMgr::GetInst()->CurrentRanking.Score +=
                ((int)m_tInfo.eMType) * 250;
        }
        else
        {
            PlayHitAnimation();
        }

        // 11/21 호스트가 확정한 적 피격 사망 팀 점수를 COMBAT_EVENT로 큐에 등록
        if (auto* stage = dynamic_cast<CScene_Stage01*>(CSceneMgr::GetInst()
            ->GetCurScene()))
        {
            if (justDied && m_tInfo.eMType == MON_TYPE::BOSS)
                stage->MarkBossDefeated();

            stage->ReportMonsterCombat(
                GetObjectId(),
                pMissile->GetObjectId(),
                pMissile->GetOwnerPlayerId(),
                static_cast<std::int32_t>(m_tInfo.fHP),
                justDied,
                scoreDelta,
                static_cast<std::uint32_t>(CRankMgr::GetInst()->CurrentRanking.Score));
        }
       
    }
   
}
void CMonster::SetAI(AI* _AI)
{
    m_pAI = _AI;
    m_pAI->m_pOwner = this;
}

void CMonster::SetAnim(MON_TYPE eType)
{
    switch (eType)
    {
    case MON_TYPE::NORMAL1:
    {
        GetAnimator()->LoadAnimation(L"animation\\N1EnemyH_Move.anim");
        GetAnimator()->Play(L"N1EnemyH_Move", true);
    }
        break;
    case MON_TYPE::NORMAL2:
    {
        GetAnimator()->LoadAnimation(L"animation\\N2Enemy_Move.anim");
        GetAnimator()->Play(L"N2Enemy_Move", true);
    }
        break;
    case MON_TYPE::NORMAL3:
    {
        GetAnimator()->LoadAnimation(L"animation\\N3Enemy_Move.anim");
        GetAnimator()->Play(L"N3Enemy_Move", true);
    }
        break;
    case MON_TYPE::NORMAL4:
    {
        GetAnimator()->LoadAnimation(L"animation\\N4Enemy_Move.anim");
        GetAnimator()->Play(L"N4Enemy_Move", true);
    }
        break;
    case MON_TYPE::BOSS:
    {
        GetAnimator()->LoadAnimation(L"animation\\Boss1Enemy_Move.anim");
        GetAnimator()->Play(L"Boss1Enemy_Move", true);
    }
        break;
    case MON_TYPE::EPIC:
    {
        GetAnimator()->LoadAnimation(L"animation\\E1Enemy_Move.anim");
        GetAnimator()->Play(L"E1Enemy_Move", true);
    }
        break;
    case MON_TYPE::EPIC2:
    {
        GetAnimator()->LoadAnimation(L"animation\\E2Enemy_Move.anim");
        GetAnimator()->Play(L"E2Enemy_Move", true);
    }
    break;
    default:
        break;
    }
   
   
}

protocol::ObjectStateRecord CMonster::ExportStateRecord() const
{
    protocol::ObjectStateRecord rec{};
    rec.objectId = GetObjectId();
    rec.kind = protocol::NetObjectKind::MONSTER;
    rec.subType = static_cast<std::uint8_t>(m_tInfo.eMType);
    rec.alive = (!IsDead() && !m_signaldead && m_tInfo.fHP > 0.f) ? 1 : 0;
    rec.fsmState = m_signaldead ? 2 : (m_bHit ? 1 : 0);
    rec.pos.x = static_cast<std::int32_t>(GetPos().x * 100.f);
    rec.pos.y = static_cast<std::int32_t>(GetPos().y * 100.f);
    rec.hp = static_cast<std::int32_t>(m_tInfo.fHP);
    rec.maxHp = static_cast<std::int32_t>(m_fMaxHP);
    return rec;

}



protocol::SpawnPayload CMonster::ExportSpawnPayload(protocol::EventId eventId) const
{
    protocol::SpawnPayload spawn{};
    spawn.eventId = eventId;
    spawn.objectId = GetObjectId();
    spawn.kind = protocol::NetObjectKind::MONSTER;
    spawn.subType = static_cast<std::uint8_t>(m_tInfo.eMType);
    spawn.ownerPlayerId = protocol::INVALID_PLAYER_SLOT;
    spawn.patternType = static_cast<std::uint8_t>(m_tInfo.ePattern);
    spawn.pos.x = static_cast<std::int32_t>(GetPos().x * 100.f);
    spawn.pos.y = static_cast<std::int32_t>(GetPos().y * 100.f);
    spawn.hp = static_cast<std::int32_t>(m_tInfo.fHP);
    spawn.damage = static_cast<std::int32_t>(m_tInfo.fAtt);
    return spawn;
}

