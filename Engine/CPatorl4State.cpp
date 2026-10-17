#include "pch.h"
#include "CPatorl4State.h"

#include "MissileMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CPlayer.h"
#include "CMonster.h"
#include "CMissile.h"

#include "CAnimator.h"
#include "CAnimation.h"


#include "CTimeMgr.h"



CPatorl4State::CPatorl4State()
    :CState(MON_STATE::PATROL4)
{
}

CPatorl4State::~CPatorl4State()
{
}
void CPatorl4State::update()
{
    // 타겟팅 된 Player 를 쫒아간다.
    CPlayer * pPlayer = (CPlayer*)CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
    Vec2 vPlayerPos = pPlayer->GetPos();
    Vec2 vMonPos = GetMonster()->GetPos();
    Vec2 vMonScale = GetMonster()->GetScale();
    Vec2 vMonDir = vPlayerPos - vMonPos;
    vMonDir.Normalize();
    Vec2 vStraight = { 1,10 };
    vStraight.Normalize();
    if (vMonPos.y <= GetMonster()->GetInfo().vDestPos.y - 50.f)
    {
        vStraight.y *= -1;
    }
    vMonPos += vStraight * GetMonster()->GetInfo().fSpeed * 0.1f * fDT;

    m_fAdt += fDT;
    m_fSTimeAcc += fDT;
    if (m_fSTimeAcc >= 8.f)
    {
        m_fSTimeAcc = 0.f;
        ChangeAIState(GetAi(), MON_STATE::RUN);
    }
    if ( m_fAdt > 1.5f)
    {
        if ( 1.8f +fDT >= m_fAdt && m_fAdt > 1.8f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
           
        }
        if (2.0f + fDT >= m_fAdt && m_fAdt > 2.0f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);

        }
        if (2.2f + fDT >= m_fAdt && m_fAdt > 2.2f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
           
        }
        if (2.4f + fDT >= m_fAdt && m_fAdt > 2.4f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);

        }
        if (2.6f + fDT >= m_fAdt && m_fAdt > 2.6f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);

        }
        if (2.8f + fDT >= m_fAdt && m_fAdt > 2.8f)
        {
            CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
            m_fAdt = 0.f;
        }
       
        // CreateMissile3(vMonPos, vMonScale);


    }


    GetMonster()->SetPos(vMonPos);
}

void CPatorl4State::Enter()
{
}

void CPatorl4State::Exit()
{
}


