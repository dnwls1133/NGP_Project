#include "pch.h"
#include "CDeadState.h"

#include "CTimeMgr.h"
#include "CResMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CScene_Stage01.h"

#include "CTexture.h"
#include "CObject.h"
#include "CMonster.h"

#include "CExposion.h"

#include "CItem.h"
#include "CAnimation.h"
#include "CAnimator.h"
CDeadState::CDeadState()
    :CState(MON_STATE::DEAD)
{
}

CDeadState::~CDeadState()
{
}
void CDeadState::update()
{
    m_fAdt += fDT;
    switch (GetMonster()->GetInfo().eMType)
    {
    case MON_TYPE::RARE:
    {
        Vec2 vPos = GetMonster()->GetPos();
        if (1.0f + fDT >= m_fAdt && m_fAdt > 1.0f)
        {
            CExposion* Exposion = new CExposion;


            Exposion->SetPos(Vec2(vPos.x - 80, vPos.y - 70));
            Exposion->SetName(L"Player_Dead");
            CreateObject(Exposion, GROUP_TYPE::DEAD_PLAYER);
        }
        if (1.15f + fDT >= m_fAdt && m_fAdt > 1.15f)
        {
            CExposion* Exposion1 = new CExposion;
            Exposion1->SetPos(Vec2(vPos.x + 70, vPos.y - 100));
            Exposion1->SetName(L"Player_Dead");
            CreateObject(Exposion1, GROUP_TYPE::DEAD_PLAYER);
        }
        if (1.3f + fDT >= m_fAdt && m_fAdt > 1.3f)
        {
            CExposion* Exposion2 = new CExposion;
            Exposion2->SetPos(Vec2(vPos.x - 100, vPos.y + 90));
            Exposion2->SetName(L"Player_Dead");
            CreateObject(Exposion2, GROUP_TYPE::DEAD_PLAYER);
        }
        if (1.45f + fDT >= m_fAdt && m_fAdt > 1.45f)
        {
            CExposion* Exposion3 = new CExposion;
            Exposion3->SetPos(Vec2(vPos.x + 60, vPos.y + 80));
            Exposion3->SetName(L"Player_Dead");
            CreateObject(Exposion3, GROUP_TYPE::DEAD_PLAYER);
        }
        if (1.6f + fDT >= m_fAdt && m_fAdt > 1.6f)
        {
            CExposion* Exposion3 = new CExposion;
            Exposion3->SetPos(Vec2(vPos.x + 50, vPos.y + 70));
            Exposion3->SetName(L"Player_Dead");
            CreateObject(Exposion3, GROUP_TYPE::DEAD_PLAYER);
        }
        if (1.75f + fDT >= m_fAdt && m_fAdt > 1.75f)
        {
            CExposion* Exposion3 = new CExposion;
            Exposion3->SetPos(Vec2(vPos.x - 60, vPos.y - 80));
            Exposion3->SetName(L"Player_Dead");
            CreateObject(Exposion3, GROUP_TYPE::DEAD_PLAYER);
        }
        if (1.8f + fDT >= m_fAdt && m_fAdt > 1.8f)
        {
            CExposion* Exposion3 = new CExposion;
            Exposion3->SetPos(Vec2(vPos.x - 50, vPos.y + 110));
            Exposion3->SetName(L"Player_Dead");
            CreateObject(Exposion3, GROUP_TYPE::DEAD_PLAYER);
        }
       
        if (m_fAdt > 5.0f)
        {
            m_fAdt = 0;
           
            Exit();
        }
    }
    break;
    default:
    {
        if (m_fAdt > 1.0f)
        {

            m_fAdt = 0;
            Exit();
        }
    }
    break;
    }
    
}

void CDeadState::Enter()
{
    MON_TYPE eType = GetMonster()->GetInfo().eMType;
    switch (eType)
    {
    case MON_TYPE::NORMAL1:
    {
       // CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"N1Enemy_Dead", L"texture\\Enemies\\normal1_enemy_Dead.png");
        //GetMonster()->GetAnimator()->CreateAnimation(L"N1Enemy_Dead", m_pTex, Vec2(0.f, 0.f), Vec2(240.f, 240.f), Vec2(240.f, 0.f), 0.05f, 13);
        ////Animation 저장
        //GetMonster()->GetAnimator()->FindAnimation(L"N1Enemy_Dead")->Save(L"animation\\N1Enemy_Dead.anim");
        GetMonster()->GetAnimator()->LoadAnimation(L"animation\\N1EnemyH_Dead.anim");
        GetMonster()->GetAnimator()->Play(L"N1EnemyH_Dead", false);
    }
    break;
    case MON_TYPE::NORMAL2:
    {
       
        
        GetMonster()->GetAnimator()->LoadAnimation(L"animation\\N2Enemy_Dead.anim");
        GetMonster()->GetAnimator()->Play(L"N2Enemy_Dead", false);
    }
    break;
    case MON_TYPE::NORMAL3:
    {
        Vec2 vMpos = GetMonster()->GetPos();
        CItem* item = new CItem;
        item->SetPos(vMpos);
        item->SetScale(Vec2(0, 0));
        item->SetName(L"Item");
        CreateObject(item, GROUP_TYPE::ITEM);
        GetMonster()->GetAnimator()->LoadAnimation(L"animation\\N34Enemy_Dead.anim");
        GetMonster()->GetAnimator()->Play(L"N34Enemy_Dead", false);
    }
    break;
    case MON_TYPE::NORMAL4:
    {
        GetMonster()->GetAnimator()->LoadAnimation(L"animation\\N34Enemy_Dead.anim");
        GetMonster()->GetAnimator()->Play(L"N34Enemy_Dead", false);
    }
    break;
    case MON_TYPE::NORMAL5:
    {
        GetMonster()->GetAnimator()->LoadAnimation(L"animation\\N34Enemy_Dead.anim");
        GetMonster()->GetAnimator()->Play(L"N34Enemy_Dead", false);
    }
    break;
    case MON_TYPE::RARE:
    {
        CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"Boss1Enemy_Dead", L"texture\\Enemies\\Boss1_Enemy_Dead.png");
        GetMonster()->GetAnimator()->CreateAnimation(L"Boss1Enemy_Dead", m_pTex, Vec2(0.f, 0.f), Vec2(400.f, 400.f), Vec2(400.f, 0.f), 0.1f, 50);
        //Animation 저장
        GetMonster()->GetAnimator()->FindAnimation(L"Boss1Enemy_Dead")->Save(L"animation\\Boss1Enemy_Dead.anim");
        GetMonster()->GetAnimator()->LoadAnimation(L"animation\\Boss1Enemy_Dead.anim");
        GetMonster()->GetAnimator()->Play(L"Boss1Enemy_Dead", false);

        

       





    }
    break;
    case MON_TYPE::EPIC:
    {
        GetMonster()->GetAnimator()->LoadAnimation(L"animation\\E1Enemy_Dead.anim");
        GetMonster()->GetAnimator()->Play(L"E1Enemy_Dead", false);
    }
    break;
    case MON_TYPE::EPIC2:
    {
        GetMonster()->GetAnimator()->LoadAnimation(L"animation\\E2Enemy_Dead.anim");
        GetMonster()->GetAnimator()->Play(L"E2Enemy_Dead", false);
    }
    break;
    case MON_TYPE::BOSS:
    {
         GetMonster()->GetAnimator()->LoadAnimation(L"animation\\N2Enemy_Dead.anim");
         GetMonster()->GetAnimator()->Play(L"N2Enemy_Dead", false);
    }
    break;
    }
}

void CDeadState::Exit()
{
    if (GetMonster()->GetInfo().eMType == MON_TYPE::RARE)
    {
        CScene_Stage01* curscene = (CScene_Stage01*)CSceneMgr::GetInst()->GetCurScene();
        curscene->Clear();
    }
    
    DeleteObject(GetMonster());
}



