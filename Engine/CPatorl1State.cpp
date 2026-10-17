#include "pch.h"
#include "CPatorl1State.h"

#include "MissileMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CPlayer.h"
#include "CMonster.h"
#include "CMissile.h"

#include "CAnimator.h"
#include "CAnimation.h"


#include "CTimeMgr.h"
CPatorl1State::CPatorl1State()
    :CState(MON_STATE::PATROL1)
{
}

CPatorl1State::~CPatorl1State()
{
}

void CPatorl1State::update()
{
    // 타겟팅 된 Player 를 쫒아간다.
    CPlayer* pPlayer = (CPlayer*)CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
    Vec2 vPlayerPos = pPlayer->GetPos();
    Vec2 vMonPos = GetMonster()->GetPos();
    Vec2 vMonScale = GetMonster()->GetScale();
    Vec2 vMonDir = vPlayerPos - vMonPos;
    vMonDir.Normalize();
    Vec2 vStraight = { 1,-10 };
    vStraight.Normalize();
    if (vMonPos.y <= GetMonster()->GetInfo().vDestPos.y - 50.f)
    {
        vStraight.y *= -1;
    }
    vMonPos += vStraight * GetMonster()->GetInfo().fSpeed * 0.1f * fDT;
    
    m_fAdt += fDT;
    m_fSTimeAcc += fDT;
    switch (GetMonster()->GetInfo().ePattern)
    {
    case MISSILE_PTRN::PTRN1:
    {
        if (m_fSTimeAcc >= 7.f)
        {
            m_fSTimeAcc = 0.f;
            ChangeAIState(GetAi(), MON_STATE::RUN);
        }
    }
    break;
    case MISSILE_PTRN::PTRN2:
    {
        if (m_fSTimeAcc >= 3.5f)
        {
            m_fSTimeAcc = 0.f;
            ChangeAIState(GetAi(), MON_STATE::RUN);
        }
    }
    break;
    }
   
    if (m_fAdt > 0.8f)
    {
        if (0.8f + fDT >= m_fAdt && m_fAdt > 0.8f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);

            CreateMissile1(Vec2(vMonPos.x + 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(Vec2(vMonPos.x + 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);

            CreateMissile1(Vec2(vMonPos.x - 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(Vec2(vMonPos.x - 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
        }

        if (1.0f + fDT >= m_fAdt && m_fAdt > 1.0f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);

            CreateMissile1(Vec2(vMonPos.x + 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(Vec2(vMonPos.x + 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);

            CreateMissile1(Vec2(vMonPos.x - 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(Vec2(vMonPos.x - 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
           
        }

        if (1.2f + fDT >= m_fAdt && m_fAdt > 1.2f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);

            CreateMissile1(Vec2(vMonPos.x + 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(Vec2(vMonPos.x + 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);

            CreateMissile1(Vec2(vMonPos.x - 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(Vec2(vMonPos.x - 50.f, vMonPos.y), vMonScale, MISSILE_TYPE::SMALL);
            m_fAdt = 0.f;
        }
       
       
       
       // CreateMissile3(vMonPos, vMonScale);
       

    }
    

    GetMonster()->SetPos(vMonPos);
   
}

void CPatorl1State::Enter()
{
}

void CPatorl1State::Exit()
{
    
}



