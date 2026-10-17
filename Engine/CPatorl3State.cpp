#include "pch.h"
#include "CPatorl3State.h"

#include "MissileMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CPlayer.h"
#include "CMonster.h"
#include "CMissile.h"

#include "CAnimator.h"
#include "CAnimation.h"


#include "CTimeMgr.h"

CPatorl3State::CPatorl3State()
    :CState(MON_STATE::PATROL3)
    , m_fXdir(-2*PI)
    , m_fFlip(1.f)
    , m_fMTimeAcc(0.f)
{
    
}

CPatorl3State::~CPatorl3State()
{
}
void CPatorl3State::update()
{
    // 타겟팅 된 Player 를 쫒아간다.
   
    
    Vec2 vMonPos = GetMonster()->GetPos();
    Vec2 vMonScale = GetMonster()->GetScale();
   
    Vec2 vStraight = { 1,-10 };
    vStraight.Normalize();
    if (vMonPos.y <= GetMonster()->GetInfo().vDestPos.y - 50.f)
    {
        vStraight.y *= -1;
    }
    vMonPos += vStraight * GetMonster()->GetInfo().fSpeed * 0.1f * fDT;

    m_fAdt += fDT;
    m_fSTimeAcc += fDT;
    m_fMTimeAcc += fDT;
    if (m_fSTimeAcc >= 20.f)
    {
        m_fSTimeAcc = 0.f;
        ChangeAIState(GetAi(), MON_STATE::RUN);
    }
    switch (GetMonster()->GetInfo().ePattern)
    {
    case MISSILE_PTRN::PTRN1:
    {
       
        if (m_fMTimeAcc > 3.f)
        {
            if (m_fAdt > 0.1f)
            {

                
                CreateMissile3(vMonPos, vMonScale, m_fXdir, MISSILE_TYPE::SMALL);
                m_fXdir += 2 * PI / 4.f * m_fFlip;
                if (m_fXdir >= 2 * PI || m_fXdir <= -2 * PI)
                {
                    m_fFlip *= -1.f;
                }
                m_fAdt = 0.f;
            }
            if (m_fMTimeAcc > 3.8f)
            {
                GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN2);
                m_fMTimeAcc = 0.f;
            }
        }
        
    }
    break;
    case MISSILE_PTRN::PTRN2:
    {
        if (2 * fDT >= m_fMTimeAcc && m_fMTimeAcc > fDT)
        {
            CreateMissile(vMonPos, vMonScale,1,MISSILE_TYPE::BSMALL);

        }
        if (m_fMTimeAcc > 2.f)
        {
            CreateMissile4(vMonPos, vMonScale, 3,MISSILE_TYPE::SPIN);
            GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN1);
            m_fMTimeAcc = 0.f;
        }
    }
    break;
    case MISSILE_PTRN::PTRN3:
    {

    }
    break;
    }
    


    GetMonster()->SetPos(vMonPos);
}

void CPatorl3State::Enter()
{
}

void CPatorl3State::Exit()
{
}


