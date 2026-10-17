#include "pch.h"
#include "CPatorl2State.h"

#include "MissileMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CPlayer.h"
#include "CMonster.h"
#include "CMissile.h"

#include "CTimeMgr.h"
CPatorl2State::CPatorl2State()
    :CState(MON_STATE::PATROL2)
{
}

CPatorl2State::~CPatorl2State()
{
}

void CPatorl2State::update()
{
    // 타겟팅 된 Player 를 쫒아간다.
    CPlayer* pPlayer = (CPlayer*)CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
    Vec2 vPlayerPos = pPlayer->GetPos();
    Vec2 vMonPos = GetMonster()->GetPos();
    Vec2 vMonScale = GetMonster()->GetScale();
    Vec2 vMonDir = vPlayerPos - vMonPos;
    vMonDir.Normalize();
    Vec2 vStraight = { 1,-10 };
  
    vMonPos += vStraight * GetMonster()->GetInfo().fSpeed * 0.01f * fDT;
    switch (GetMonster()->GetInfo().ePattern)
    {
    case MISSILE_PTRN::PTRN1:
    {
        m_fSTimeAcc += fDT;
        if (m_fSTimeAcc >= 1.f)
        {
            m_fSTimeAcc = 0.f;
            CreateMissile(vMonPos, vMonScale,1,MISSILE_TYPE::SMALL);
            GetMonster()->SetSpeed(350.f);
            ChangeAIState(GetAi(), MON_STATE::RUN);
        }
    }
    break;
    case MISSILE_PTRN::PTRN2:
    {
        m_fAdt += fDT;
        m_fSTimeAcc += fDT;

        if (m_fAdt > 0.5f)
        {
            CreateMissile(vMonPos, vMonScale,1,MISSILE_TYPE::SMALL);
            m_fAdt = 0.f;
        }
        
        if (m_fSTimeAcc >= 1.5f)
        {
            
            GetMonster()->SetSpeed(450.f);
            ChangeAIState(GetAi(), MON_STATE::RUN);
        }
    }
    break;
    }

    
   


    GetMonster()->SetPos(vMonPos);
}

void CPatorl2State::Enter()
{
    
}

void CPatorl2State::Exit()
{
}


