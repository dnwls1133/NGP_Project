#include "pch.h"
#include "CRunState.h"

#include "MissileMgr.h"

#include "CObject.h"
#include "CPlayer.h"
#include "CMonster.h"

#include "CTimeMgr.h"

#include "CSceneMgr.h"
#include "CScene.h"
CRunState::CRunState()
    :CState(MON_STATE::RUN)
{
}

CRunState::~CRunState()
{
}

void CRunState::update()
{
    // 가만히 있는다.
   

    // Player 의 위치 체크
 

    // 몬스터의 범위 안에 들어오면 추적 상태로 전환
    CMonster* pMonster = GetMonster();
    Vec2 vMonPos = pMonster->GetPos();
    Vec2 vMonScale = pMonster->GetScale();
    Vec2 vDestPos = GetMonster()->GetInfo().vExitPos;
    Vec2 vDir = vDestPos - vMonPos;
    vDir.Normalize();



    Vec2 vDiff = vDestPos - vMonPos;
    float fLen = vDiff.Length();
    if (GetMonster()->GetInfo().ePattern == MISSILE_PTRN::PTRN2)
    {
        m_fAdt += fDT;
        if (m_fAdt > 1.0f)
        {
            CreateMissile(vMonPos, vMonScale,3, MISSILE_TYPE::SMALL);
            m_fAdt = 0.f;
        }
    }
    // 플레이어가 몬스터의 인식범위 안으로 진입
    
    vMonPos += vDir * GetMonster()->GetInfo().fSpeed * fDT;
    GetMonster()->SetPos(vMonPos);
}

void CRunState::Enter()
{
}

void CRunState::Exit()
{
}


