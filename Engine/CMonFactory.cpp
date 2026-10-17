#include "pch.h"
#include "CMonFactory.h"

#include "CMonster.h"
#include "AI.h"

#include "CIdleState.h"
#include "CTraceState.h"
#include "CPatorl1State.h"
#include "CPatorl2State.h"
#include "CPatorl3State.h"
#include "CPatorl4State.h"
#include "CBossPatorl1STate.h"
#include "CBossPatorl2STate.h"
#include "CBossPatorl3STate.h"
#include "CBosPatorl4STate.h"
#include "CDeadState.h"
#include "CRunState.h"

#include "CCollider.h"

CMonFactory::CMonFactory()
{
}

CMonFactory::~CMonFactory()
{
}
CMonster* CMonFactory::CreateMonster(MON_TYPE _eType, MISSILE_PTRN _eMType,Vec2 _vPos,Vec2 _vDPos,Vec2 _vEPos)
{

    CMonster* pMon = nullptr;
    switch (_eType)
    {
    case MON_TYPE::NORMAL1:
    {
        pMon = new CMonster;
        pMon->SetAnim(MON_TYPE::NORMAL1);
        pMon->SetName(L"Monster");
        pMon->SetPos(_vPos);
        pMon->SetScale(Vec2(100.f, 100.f));
        pMon->GetCollider()->SetScale(Vec2(50.f,25.f));

        tMonInfo info = {};
        info.fAtt = 1.f;
        info.fAttRange = 10.f;
        info.fHP = 250.f;
        info.fRecogRange = 3.f;
        info.fSpeed = 100.f;
        info.vDestPos = _vDPos;
        info.ePattern = _eMType;
        info.eMType = MON_TYPE::NORMAL1;
        info.vExitPos = _vEPos;
        pMon->SetMonInfo(info);

        AI* pAI = new AI;
        pAI->AddState(new CIdleState);
        pAI->AddState(new CPatorl1State);
        pAI->AddState(new CDeadState);
        pAI->AddState(new CRunState);
        pAI->SetCurState(MON_STATE::IDLE);
        pMon->SetAI(pAI);
    }
        break;
    case MON_TYPE::NORMAL2:
    {
        pMon = new CMonster;
        pMon->SetAnim(MON_TYPE::NORMAL2);
        pMon->SetName(L"Monster");
        pMon->SetPos(_vPos);
        pMon->SetScale(Vec2(50.f, 50.f));
        pMon->GetCollider()->SetScale(Vec2(50.f, 25.f));

        tMonInfo info = {};
        info.fAtt = 1.f;
        info.fAttRange = 10.f;
        info.fHP = 150.f;
        info.fRecogRange = 3.f;
        info.fSpeed = 200.f;
        info.vDestPos = _vDPos;
        info.ePattern = _eMType;
        info.eMType = MON_TYPE::NORMAL2;
        info.vExitPos = _vEPos;
        pMon->SetMonInfo(info);

        AI* pAI = new AI;
        pAI->AddState(new CIdleState);
        pAI->AddState(new CPatorl2State);
        pAI->AddState(new CDeadState);
        pAI->AddState(new CRunState);
        pAI->SetCurState(MON_STATE::IDLE);
        pMon->SetAI(pAI);
    }
    break;
    case MON_TYPE::NORMAL3:
    {
        pMon = new CMonster;
        pMon->SetAnim(MON_TYPE::NORMAL3);
        pMon->SetName(L"Monster");
        pMon->SetPos(_vPos);


        pMon->GetCollider()->SetScale(Vec2(25.f, 20.f));
        tMonInfo info = {};
        info.fAtt = 1.f;
        info.fAttRange = 10.f;
        info.fHP = 20.f;
        info.fRecogRange = 3.f;
        info.fSpeed = 200.f;
        info.vDestPos = _vDPos;
        info.ePattern = _eMType;
        info.eMType = MON_TYPE::NORMAL3;
        info.vExitPos = _vEPos;
        pMon->SetMonInfo(info);

        AI* pAI = new AI;
        pAI->AddState(new CIdleState);
        pAI->AddState(new CPatorl1State);
        pAI->AddState(new CDeadState);
        pAI->AddState(new CRunState);
        pAI->SetCurState(MON_STATE::IDLE);
        pMon->SetAI(pAI);
    }
    break;
    case MON_TYPE::NORMAL4:
    {
        pMon = new CMonster;
        pMon->SetAnim(MON_TYPE::NORMAL4);
        pMon->SetName(L"Monster");
        pMon->SetPos(_vPos);
        pMon->GetCollider()->SetScale(Vec2(25.f, 20.f));


        tMonInfo info = {};
        info.fAtt = 1.f;
        info.fAttRange = 10.f;
        info.fHP = 50.f;
        info.fRecogRange = 3.f;
        info.fSpeed = 200.f;
        info.vDestPos = _vDPos;
        info.ePattern = _eMType;
        info.eMType = MON_TYPE::NORMAL4;
        info.vExitPos = _vEPos;
        pMon->SetMonInfo(info);

        AI* pAI = new AI;
        pAI->AddState(new CIdleState);
        pAI->AddState(new CPatorl1State);
        pAI->AddState(new CDeadState);
        pAI->AddState(new CRunState);
        pAI->SetCurState(MON_STATE::IDLE);
        pMon->SetAI(pAI);
    }
    break;
    case MON_TYPE::NORMAL5:
    {
        pMon = new CMonster;
        pMon->SetAnim(MON_TYPE::NORMAL4);
        pMon->SetName(L"Monster");
        pMon->SetPos(_vPos);
        pMon->GetCollider()->SetScale(Vec2(25.f, 20.f));


        tMonInfo info = {};
        info.fAtt = 1.f;
        info.fAttRange = 10.f;
        info.fHP = 50.f;
        info.fRecogRange = 3.f;
        info.fSpeed = 300.f;
        info.vDestPos = _vDPos;
        info.ePattern = _eMType;
        info.eMType = MON_TYPE::NORMAL5;
        info.vExitPos = _vEPos;
        pMon->SetMonInfo(info);

        AI* pAI = new AI;
        pAI->AddState(new CIdleState);
        pAI->AddState(new CPatorl1State);
        pAI->AddState(new CDeadState);
        pAI->AddState(new CRunState);
        pAI->SetCurState(MON_STATE::IDLE);
        pMon->SetAI(pAI);
    }
    break;
    case MON_TYPE::RARE:
    {
        pMon = new CMonster;
        pMon->SetAnim(MON_TYPE::RARE);
        pMon->SetName(L"Monster");
        pMon->SetPos(_vPos);
        pMon->SetScale(Vec2(150.f, 150.f));
        pMon->GetCollider()->SetScale(Vec2(150.f, 100.f));


        tMonInfo info = {};
        info.fAtt = 1.f;
        info.fAttRange = 10.f;
        info.fHP = 8000.f;
        info.fRecogRange = 3.f;
        info.fSpeed = 100.f;
        info.vDestPos = _vDPos;
        info.ePattern = _eMType;
        info.eMType = MON_TYPE::RARE;
        info.vExitPos = _vEPos;
        pMon->SetMonInfo(info);

        AI* pAI = new AI;
        pAI->AddState(new CIdleState);
        pAI->AddState(new CBossPatorl1STate);
        pAI->AddState(new CBossPatorl2STate);
        pAI->AddState(new CBossPatorl3STate);
        pAI->AddState(new CBosPatorl4STate);
        pAI->AddState(new CDeadState);
        pAI->AddState(new CRunState);
        pAI->SetCurState(MON_STATE::IDLE);
        pMon->SetAI(pAI);
    }
        break;
    case MON_TYPE::EPIC:
    {
        pMon = new CMonster;
        pMon->SetAnim(MON_TYPE::EPIC);
        pMon->SetName(L"Monster");
        pMon->SetPos(_vPos);
        pMon->SetScale(Vec2(100.f, 100.f));
        pMon->GetCollider()->SetScale(Vec2(100.f, 50.f));


        tMonInfo info = {};
        info.fAtt = 1.f;
        info.fAttRange = 10.f;
        info.fHP = 1000.f;
        info.fRecogRange = 3.f;
        info.fSpeed = 100.f;
        info.vDestPos = _vDPos;
        info.ePattern = _eMType;
        info.eMType = MON_TYPE::EPIC;
        info.vExitPos = _vEPos;
        pMon->SetMonInfo(info);

        AI* pAI = new AI;
        pAI->AddState(new CIdleState);
        pAI->AddState(new CPatorl3State);
        pAI->AddState(new CDeadState);
        pAI->AddState(new CRunState);
        pAI->SetCurState(MON_STATE::IDLE);
        pMon->SetAI(pAI);
    }
        break;
    case MON_TYPE::EPIC2:
    {
        pMon = new CMonster;
        pMon->SetAnim(MON_TYPE::EPIC2);
        pMon->SetName(L"Monster");
        pMon->SetPos(_vPos);
        pMon->SetScale(Vec2(100.f, 100.f));
        pMon->GetCollider()->SetScale(Vec2(100.f, 50.f));


        tMonInfo info = {};
        info.fAtt = 1.f;
        info.fAttRange = 10.f;
        info.fHP = 1000.f;
        info.fRecogRange = 3.f;
        info.fSpeed = 100.f;
        info.vDestPos = _vDPos;
        info.ePattern = _eMType;
        info.eMType = MON_TYPE::EPIC2;
        info.vExitPos = _vEPos;
        pMon->SetMonInfo(info);

        AI* pAI = new AI;
        pAI->AddState(new CIdleState);
        pAI->AddState(new CPatorl4State);
        pAI->AddState(new CDeadState);
        pAI->AddState(new CRunState);
        pAI->SetCurState(MON_STATE::IDLE);
        pMon->SetAI(pAI);
    }
    break;
    case MON_TYPE::BOSS:
    {
        pMon->SetName(L"Monster");
    }
        break;
    default:
        break;
    }

    assert(pMon);
    return pMon;
}
