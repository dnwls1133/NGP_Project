#include "pch.h"
#include "CIdleState.h"

#include "MissileMgr.h"
#include "CTimeMgr.h"

#include "CObject.h"
#include "CPlayer.h"
#include "CMonster.h"


#include "CSceneMgr.h"
#include "CScene.h"
CIdleState::CIdleState()
    :CState(MON_STATE::IDLE)
{
}

CIdleState::~CIdleState()
{
}


void CIdleState::update()
{
    // 가만히 있는다.
    CPlayer* pPlayer = (CPlayer*)CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
    Vec2 vBackPos = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetPos();
    Vec2 vBackScale = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetScale();
    // Player 의 위치 체크
    Vec2 vPlayerPos = pPlayer->GetPos();

    // 몬스터의 범위 안에 들어오면 추적 상태로 전환
    CMonster* pMonster = GetMonster();
    Vec2 vMonPos = pMonster->GetPos();
    Vec2 vMonScale = pMonster->GetScale();
    Vec2 vDestPos = GetMonster()->GetInfo().vDestPos;
    Vec2 vDir = vDestPos - vMonPos;
    vDir.Normalize();

   

    Vec2 vDiff = vDestPos - vMonPos;
    float fLen = vDiff.Length();

    // 플레이어가 몬스터의 인식범위 안으로 진입
    if (GetMonster()->GetInfo().eMType == MON_TYPE::NORMAL2)
    {
        if (GetMonster()->GetInfo().ePattern == MISSILE_PTRN::PTRN2)
        {
            m_fAdt += fDT;
            if (m_fAdt > 1.0f)
            {
                CreateMissile(vMonPos, vMonScale,3, MISSILE_TYPE::SMALL);
                m_fAdt = 0.f;
            }
        }
    }
    if (fLen < pMonster->GetInfo().fRecogRange )
    {
        switch (GetMonster()->GetInfo().eMType)
        {
        case MON_TYPE::NORMAL1:
        {
            ChangeAIState(GetAi(), MON_STATE::PATROL1);
        }
        break;
        case MON_TYPE::NORMAL2:
        {
            if (GetMonster()->GetInfo().ePattern == MISSILE_PTRN::PTRN1)
            {
                ChangeAIState(GetAi(), MON_STATE::PATROL2);
            }
            else if(GetMonster()->GetInfo().ePattern == MISSILE_PTRN::PTRN3)
            {
                CreateMissile(vMonPos, vMonScale,3, MISSILE_TYPE::SMALL);
                ChangeAIState(GetAi(), MON_STATE::RUN);
            }
            else
            {
                ChangeAIState(GetAi(), MON_STATE::RUN);
            }
        }
        break;
        case MON_TYPE::NORMAL3:
        {
            m_fSTimeAcc += fDT;
            if (m_fSTimeAcc >= 0.5f)
            {
                ChangeAIState(GetAi(), MON_STATE::RUN);
            }
        }
        break;
        case MON_TYPE::NORMAL4:
        {
            m_fSTimeAcc += fDT;
            if (m_fSTimeAcc >= 0.5f)
            {
                ChangeAIState(GetAi(), MON_STATE::RUN);
            }
        }
        break;
        case MON_TYPE::NORMAL5:
        {
            ChangeAIState(GetAi(), MON_STATE::RUN);
        }
        break;
        case MON_TYPE::EPIC:
        {
            ChangeAIState(GetAi(), MON_STATE::PATROL3);
        }
        break;
        case MON_TYPE::EPIC2:
        {
            ChangeAIState(GetAi(), MON_STATE::PATROL4);
        }
        break;
        case MON_TYPE::RARE:
        {
            GetMonster()->SetDestPos(Vec2(vBackPos.x + 100.f, vBackPos.y - 200.f));
            ChangeAIState(GetAi(), MON_STATE::MBOSS0);
        }
        break;
        case MON_TYPE::BOSS:
        {

        }
        break;
        }
        
        
    }
    vMonPos += vDir * GetMonster()->GetInfo().fSpeed * fDT;
    GetMonster()->SetPos(vMonPos);

}

void CIdleState::Enter()
{
}

void CIdleState::Exit()
{
}
