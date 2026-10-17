#include "pch.h"
#include "CTraceState.h"

#include "MissileMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CPlayer.h"
#include "CMonster.h"
#include "CMissile.h"

#include "CTimeMgr.h"
void CreateMissile(Vec2 vMonPos, Vec2 vMonScale);
CTraceState::CTraceState()
    :CState(MON_STATE::TRACE)
{
}

CTraceState::~CTraceState()
{
}
void CTraceState::update()
{
    // 타겟팅 된 Player 를 쫒아간다.
    CPlayer* pPlayer = (CPlayer*)CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
    Vec2 vPlayerPos = pPlayer->GetPos();
    Vec2 vMonPos = GetMonster()->GetPos();
    Vec2 vMonScale = GetMonster()->GetScale();
    Vec2 vMonDir = vPlayerPos - vMonPos;
    vMonDir.Normalize();
    Vec2 vStraight = { -20,10 };
    vStraight.Normalize();
    vMonPos += vStraight * GetMonster()->GetInfo().fSpeed * 0.5f* fDT;
    m_fAdt += fDT;
    if (m_fAdt > 0.5f)
    {
        CreateMissile(vMonPos, vMonScale,3, MISSILE_TYPE::SMALL);
       // CreateMissile1(vMonPos, vMonScale);
        //CreateMissile2(vMonPos, vMonScale);
       // CreateMissile3(vMonPos, vMonScale);
        m_fAdt = 0.f;
      
    }
   
  
    GetMonster()->SetPos(vMonPos);
}

void CTraceState::Enter()
{
}

void CTraceState::Exit()
{
}


