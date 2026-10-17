#include "pch.h"
#include "CBosPatorl4STate.h"

#include "time.h"
#include "MissileMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CPlayer.h"
#include "CMonster.h"
#include "CMissile.h"

#include "CCore.h"




#include "CAnimator.h"
#include "CAnimation.h"


#include "CTimeMgr.h"

CBosPatorl4STate::CBosPatorl4STate()
    :CState(MON_STATE::MBOSS3)
    , m_fFlip(1.f)
    , m_fMTimeAcc(0.f)
    , m_iDestType(0)
    , m_fXdir(-2 * PI)
{
}

CBosPatorl4STate::~CBosPatorl4STate()
{
}
void CBosPatorl4STate::update()
{
    Vec2 vResolution = CCore::GetInst()->GetResolution();
    Vec2 vBackPos = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetPos();
    Vec2 vBackScale = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetScale();
    RECT rBackrect = { (vBackPos.x - vBackScale.x / 2.f),(vBackPos.y - vBackScale.y / 2.f),(vBackPos.x + vBackScale.x / 2.f),(vBackPos.x + vBackScale.y / 2.f) };
    Vec2 vMonPos = GetMonster()->GetPos();
    Vec2 vMonScale = GetMonster()->GetScale();
    Vec2 vDestPos = GetMonster()->GetInfo().vDestPos;
    Vec2 vDir = vDestPos - vMonPos;
    vDir.Normalize();

    Vec2 vDiff = vDestPos - vMonPos;
    float fLen = vDiff.Length();
    if (fLen < GetMonster()->GetInfo().fRecogRange)
    {
        GetMonster()->SetDestPos(m_pDesPos[m_iDestType++]);
        if (m_iDestType > 3)
        {
            m_iDestType = 0;
           
        }
    }
    vMonPos += vDir * GetMonster()->GetInfo().fSpeed * fDT;

    m_fAdt += fDT;
    m_fMTimeAcc += fDT;
    switch (GetMonster()->GetInfo().ePattern)
    {
    case MISSILE_PTRN::PTRN1:
    {
        if (m_fAdt > 0.5f)
        {

            for (int i = 0; i < 8; ++i)
            {
                if (0.5f + (0.1f*i) + fDT >= m_fAdt && m_fAdt > 0.5f + (0.1f * i))
                {
                    CreateMissile1(vMonPos, vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile2(vMonPos, vMonScale, MISSILE_TYPE::SMALL);

                    CreateMissile1(Vec2(vMonPos.x + 20.f, vMonPos.y - 50.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile2(Vec2(vMonPos.x + 20.f, vMonPos.y - 50.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile1(Vec2(vMonPos.x + 40.f, vMonPos.y - 60.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile2(Vec2(vMonPos.x + 40.f, vMonPos.y - 60.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile1(Vec2(vMonPos.x + 60.f, vMonPos.y - 70.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile2(Vec2(vMonPos.x + 60.f, vMonPos.y - 70.f), vMonScale, MISSILE_TYPE::SMALL);


                    CreateMissile1(Vec2(vMonPos.x - 20.f, vMonPos.y - 50.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile2(Vec2(vMonPos.x - 20.f, vMonPos.y - 50.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile1(Vec2(vMonPos.x - 40.f, vMonPos.y - 60.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile2(Vec2(vMonPos.x - 40.f, vMonPos.y - 60.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile1(Vec2(vMonPos.x - 60.f, vMonPos.y - 70.f), vMonScale, MISSILE_TYPE::SMALL);
                    CreateMissile2(Vec2(vMonPos.x - 60.f, vMonPos.y - 70.f), vMonScale, MISSILE_TYPE::SMALL);

                }
            }
            if (1.5f < m_fAdt)
            {
                m_fAdt = 0.f;
            }

          
           
        }

        if (5.0f < m_fMTimeAcc)
        {

            m_fMTimeAcc = 0.f;
            m_fAdt = 0;
            GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN2);
        }
    }
    break;
    case MISSILE_PTRN::PTRN2:
    {
        if (m_fAdt > 0.5f)
        {
            if (0.5f + fDT >= m_fAdt && m_fAdt > 0.5f)
            {
                CreateMissile5(vMonPos, vMonScale, MISSILE_TYPE::MIDDLE);
            }
            if (1.f + fDT >= m_fAdt && m_fAdt > 1.f)
            {
                CreateMissile5(vMonPos, vMonScale, MISSILE_TYPE::MIDDLE);
            }
            if (1.5f + fDT >= m_fAdt && m_fAdt > 1.5f)
            {
                CreateMissile5(vMonPos, vMonScale, MISSILE_TYPE::MIDDLE);
            }
            if (2.f + fDT >= m_fAdt && m_fAdt > 2.f)
            {
                CreateMissile5(vMonPos, vMonScale, MISSILE_TYPE::MIDDLE);
                m_fFlip = 1.f;
                m_fMTimeAcc = 0.f;

                m_fXdir = -2 * PI;
                GetMonster()->SetSpeed(200.f);
                GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN1);
                ChangeAIState(GetAi(), MON_STATE::MBOSS1);
                m_fAdt = 0;
            }
        }
       
    }
    break;
    }
    GetMonster()->SetPos(vMonPos);
}

void CBosPatorl4STate::Enter()
{
    Vec2 vResolution = CCore::GetInst()->GetResolution();
    Vec2 vBackPos = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetPos();
    Vec2 vBackScale = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetScale();
    RECT rBackrect = { (vBackPos.x - vBackScale.x / 2.f),(vBackPos.y - vBackScale.y / 2.f),(vBackPos.x + vBackScale.x / 2.f),(vBackPos.x + vBackScale.y / 2.f) };
    m_pDesPos[0] = Vec2(vBackPos.x, vBackPos.y - 400.f);
    m_pDesPos[1] = Vec2(vBackPos.x + 200.f, vBackPos.y - 390.f);
    m_pDesPos[2] = Vec2(vBackPos.x, vBackPos.y - 350.f);
    m_pDesPos[3] = Vec2(vBackPos.x - 200.f, vBackPos.y - 390.f);
}

void CBosPatorl4STate::Exit()
{
}


