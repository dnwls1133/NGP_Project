#include "pch.h"

#include "CSceneMgr.h"
#include "CScene.h"

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

void CMonster::update()
{
    m_pAI->update();
    Vec2 vBPos = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetPos();
    Vec2 vBScale = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetScale();
    Vec2 vMonPos = GetPos();
    if (m_bHit)
    {
        dAccTime0 += fDT;
        if (dAccTime0 > 0.05f)
        {
            dAccTime0 = 0.f;
            m_bHit = false;
            if (m_signaldead == false)
            {
                SetAnim(m_tInfo.eMType);
            }
           
        }
    }
    if (vMonPos.x < vBPos.x - vBScale.x - 100.f
        || vMonPos.x > vBPos.x + vBScale.x + 100.f
        || vMonPos.y < vBPos.y - vBScale.y - 100.f
        || vMonPos.y > vBPos.y + vBScale.y + 100.f)
    {
        DeleteObject(this);
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
    CPlayer* pInfo = CurScene->GetPlayer(0);
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
    CPlayer* pPlayerobj = (CPlayer*)CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
    int iPAtk = pPlayerobj->GetPlayerAtk();
    if (pOtherobj->GetName() == L"Missile_Player")
    {
        m_bHit = true;
        switch (m_tInfo.eMType)
        {
        case MON_TYPE::NORMAL1:
        {
            //GetAnimator()->LoadAnimation(L"animation\\N1EnemyH_Hit.anim");
           // GetAnimator()->Play(L"N1EnemyH_Hit", true);
        }
        break;
        case MON_TYPE::NORMAL2:
        {
            GetAnimator()->LoadAnimation(L"animation\\N2Enemy_Hit.anim");
            GetAnimator()->Play(L"N2Enemy_Hit", true);
        }
        break;
        case MON_TYPE::NORMAL3:
        {
            GetAnimator()->LoadAnimation(L"animation\\N3Enemy_Hit.anim");
            GetAnimator()->Play(L"N3Enemy_Hit", true);
        }
        break;
        case MON_TYPE::NORMAL4:
        {
            GetAnimator()->LoadAnimation(L"animation\\N4Enemy_Hit.anim");
            GetAnimator()->Play(L"N4Enemy_Hit", true);
        }
        break;
        case MON_TYPE::BOSS:
        {
            GetAnimator()->LoadAnimation(L"animation\\Boss1Enemy_Hit.anim");
            GetAnimator()->Play(L"Boss1Enemy_Hit", true);
        }
        break;
        case MON_TYPE::EPIC:
        {
            GetAnimator()->LoadAnimation(L"animation\\E1Enemy_Hit.anim");
            GetAnimator()->Play(L"E1Enemy_Hit", true);
        }
        break;
        case MON_TYPE::EPIC2:
        {
            GetAnimator()->LoadAnimation(L"animation\\E2Enemy_Hit.anim");
            GetAnimator()->Play(L"E2Enemy_Hit", true);
        }
        break;
        
        default:
            break;
        }
        m_tInfo.fHP -= iPAtk;

        if (m_tInfo.fHP < 0 && m_signaldead == false)
        {
            CSoundMgr::GetInst()->PlaySE(m_pDeadSE);
            m_pAI->ChangeState(MON_STATE::DEAD);
            m_signaldead = true;
            SetCollideroff();
            //DeleteObject(this);

            CRankMgr::GetInst()->CurrentRanking.Score +=
                ((int)m_tInfo.eMType) * 250;
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

