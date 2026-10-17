#include "pch.h"
#include "CBossPatorl2STate.h"

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

CBossPatorl2STate::CBossPatorl2STate()
    :CState(MON_STATE::MBOSS1)
   , m_fFlip(1.f)
    ,m_fMTimeAcc(0.f)
    ,m_iDestType(0)
    ,m_fXdir(-2* PI)
{
}

CBossPatorl2STate::~CBossPatorl2STate()
{
}

void CBossPatorl2STate::update()
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
        if (m_iDestType > 4)
        {
            m_iDestType = 0;
            m_fFlip = 1.f;
            m_fMTimeAcc = 0.f;
            m_iDestType = 0;
            m_fXdir = -2 * PI;
            GetMonster()->SetDestPos(Vec2(vBackPos.x, vBackPos.y - 400.f));
            GetMonster()->SetSpeed(300.f);
            ChangeAIState(GetAi(), MON_STATE::MBOSS2);
            GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN1);
        }
    }
    vMonPos += vDir * GetMonster()->GetInfo().fSpeed * 0.5f * fDT;

    m_fAdt += fDT;
    m_fSTimeAcc += fDT;
    m_fMTimeAcc += fDT;
    if (m_fSTimeAcc > 20.f)
    {
        m_fSTimeAcc = 0.f;
        //ChangeAIState(GetAi(), MON_STATE::RUN);
    }
    switch (GetMonster()->GetInfo().ePattern)
    {
    case MISSILE_PTRN::PTRN1:
    {
       
        if (m_fMTimeAcc > 1.f)
        {
            if (m_fAdt > 0.05f)
            {
                CreatePenMissile(vMonPos, vMonScale / 2.f, m_fXdir, MISSILE_TYPE::SMALL);
                CreatePenMissile(vMonPos, vMonScale / 2.f, -1 * m_fXdir, MISSILE_TYPE::SMALL);
                CreatePenMissile(vMonPos, vMonScale / 2.f,  m_fXdir/1.5f, MISSILE_TYPE::SMALL);
                CreatePenMissile(vMonPos, vMonScale / 2.f, -1* m_fXdir/1.5f, MISSILE_TYPE::SMALL);
                m_fXdir += 2 * PI / 6.f * m_fFlip;
                if (m_fXdir >= 2 * PI || m_fXdir <= -2 * PI)
                {
                    m_fFlip *= -1.f;
                }
                m_fAdt = 0.f;
                //CreateMissile4(vMonPos, vMonScale, MISSILE_TYPE::BSMALL);
               // m_fAdt = 0.f;
            }
            if (m_fMTimeAcc > 1.6f)
            {
                GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN2);
                m_fMTimeAcc = 0.f;
            }
           
        }
    }
    break;
    case MISSILE_PTRN::PTRN2:
    {
        if (m_fAdt > 2.f)
        {

            if (2.f + fDT >= m_fAdt && m_fAdt > 2.f)
            {
                CreateMissile5(vMonPos, vMonScale, MISSILE_TYPE::BSMALL);
            }
            if (2.5f + fDT >= m_fAdt && m_fAdt > 2.5f)
            {
                CreateMissile5(vMonPos, vMonScale, MISSILE_TYPE::BSMALL);
            }
            if (3.f + fDT >= m_fAdt && m_fAdt > 3.f)
            {
                CreateMissile5(vMonPos, vMonScale, MISSILE_TYPE::BSMALL);
            }
            if (3.5f + fDT >= m_fAdt && m_fAdt > 3.5f)
            {
                CreateMissile5(vMonPos, vMonScale, MISSILE_TYPE::BSMALL);
                m_fAdt = 0.f;
                m_fMTimeAcc = 0.f;
                GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN1);
            }
           
        }
       
    }
    break;
    }
    GetMonster()->SetPos(vMonPos);
}

void CBossPatorl2STate::Enter()
{
    Vec2 vResolution = CCore::GetInst()->GetResolution();
    Vec2 vBackPos = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetPos();
    Vec2 vBackScale = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetScale();
    RECT rBackrect = { (vBackPos.x - vBackScale.x / 2.f),(vBackPos.y - vBackScale.y / 2.f),(vBackPos.x + vBackScale.x / 2.f),(vBackPos.x + vBackScale.y / 2.f) };
    m_pDesPos[0] = Vec2(vBackPos.x, vBackPos.y - 100.f);
    m_pDesPos[1] = Vec2(vBackPos.x - 100.f, vBackPos.y - 150.f);
    m_pDesPos[2] = Vec2(vBackPos.x+100.f, vBackPos.y - 50.f);
    m_pDesPos[3] = Vec2(vBackPos.x - 40.f, vBackPos.y - 250.f);
    m_pDesPos[4] = Vec2(vBackPos.x + 30.f, vBackPos.y - 400.f);
}

void CBossPatorl2STate::Exit()
{
}


