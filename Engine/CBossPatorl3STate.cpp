#include "pch.h"
#include "CBossPatorl3STate.h"

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

CBossPatorl3STate::CBossPatorl3STate()
    :CState(MON_STATE::MBOSS2)
    , m_fXFlip(1.f)
    , m_fRXdir(-2.f * PI)
    , m_fYFlip(1.f)
    , m_fMTimeAcc(0.f)
    , m_iDestType(0)
    , m_fXdir(-2.f * PI)
    , m_fYdir(-3.f * PI / 2.f)
{
}

CBossPatorl3STate::~CBossPatorl3STate()
{
}

void CBossPatorl3STate::update()
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
        if (m_iDestType > 2)
        {
            m_iDestType = 0;
            m_fXFlip = 1.f;
            m_fRXdir = -2.f * PI;
            m_fYFlip = 1.f;
            m_fMTimeAcc = 0.f;
            m_iDestType = 0;
            m_fXdir = -2.f * PI;
            m_fYdir = -3.f * PI / 2.f;
            GetMonster()->SetSpeed(100.f);
            GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN1);
            ChangeAIState(GetAi(), MON_STATE::MBOSS3);
        }
        GetMonster()->SetDestPos(m_pDesPos[m_iDestType++]);
       
    }
    vMonPos += vDir * GetMonster()->GetInfo().fSpeed * fDT;

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

            if (m_fAdt > 0.1f)
            {

                
                CreateHurricaneMissile(Vec2(vMonPos.x - 100.f, vMonPos.y), vMonScale, m_fXdir, -3.f * PI / 2.f, MISSILE_TYPE::SMALL);
                CreateHurricaneMissile(Vec2(vMonPos.x + 100.f, vMonPos.y), vMonScale, -1 *m_fXdir, -3.f * PI / 2.f, MISSILE_TYPE::SMALL);
                m_fXdir += 2 * PI / 4.f * m_fXFlip;
                if (m_fXdir >= 2 * PI || m_fXdir <= -2 * PI)
                {
                    m_fXFlip *= -1.f;
                }

                m_fAdt = 0.f;

            }

            if (m_fMTimeAcc > 1.3f)
            {
                if (m_fSTimeAcc > 0.07f)
                {
                    CreateHurricaneMissile(Vec2(vMonPos.x - 100.f, vMonPos.y), vMonScale, -1 * m_fRXdir, 3.f * PI / 2.f, MISSILE_TYPE::SMALL);
                    CreateHurricaneMissile(Vec2(vMonPos.x + 100.f, vMonPos.y), vMonScale, m_fRXdir, 3.f * PI / 2.f, MISSILE_TYPE::SMALL);
                    m_fRXdir += 2 * PI / 4.f * m_fYFlip;
                    if (m_fRXdir >= 2 * PI || m_fRXdir <= -2 * PI)
                    {
                        m_fYFlip *= -1.f;
                    }
                    m_fSTimeAcc = 0.f;
                }
            }
           
            if (m_fMTimeAcc > 3.0f)
            {
                GetMonster()->SetMissiletype(MISSILE_PTRN::PTRN2);
                m_fMTimeAcc = 0.f;
            }

        }
    }
    break;
    case MISSILE_PTRN::PTRN2:
    {
        if (m_fAdt > 1.f)
        {

            if (1.f + fDT >= m_fAdt && m_fAdt > 1.f)
            {
                CreateStraightMissile(Vec2(vMonPos.x, vMonPos.y), vMonScale,400.f, MISSILE_TYPE::BSMALL);

            }
            if (1.1f + fDT >= m_fAdt && m_fAdt > 1.1f)
            {
                CreateStraightMissile(Vec2(vMonPos.x, vMonPos.y), vMonScale, 400.f, MISSILE_TYPE::BSMALL);
            }
            if (1.2f + fDT >= m_fAdt && m_fAdt > 1.2f)
            {
                CreateStraightMissile(Vec2(vMonPos.x, vMonPos.y), vMonScale, 400.f, MISSILE_TYPE::BSMALL);
            }
            if (1.3f + fDT >= m_fAdt && m_fAdt > 1.3f)
            {
                CreateStraightMissile(Vec2(vMonPos.x, vMonPos.y), vMonScale, 400.f, MISSILE_TYPE::BSMALL);
            }
            if (1.4f + fDT >= m_fAdt && m_fAdt > 1.4f)
            {
                CreateStraightMissile(Vec2(vMonPos.x, vMonPos.y), vMonScale, 400.f, MISSILE_TYPE::BSMALL);
            }
            if (1.5f + fDT >= m_fAdt && m_fAdt > 1.5f)
            {
                CreateStraightMissile(Vec2(vMonPos.x, vMonPos.y), vMonScale, 400.f, MISSILE_TYPE::BSMALL);

            }
            if (1.6f + fDT >= m_fAdt && m_fAdt > 1.6f)
            {
                CreateStraightMissile(Vec2(vMonPos.x, vMonPos.y), vMonScale, 400.f, MISSILE_TYPE::BSMALL);
            }
            if (1.7f + fDT >= m_fAdt && m_fAdt > 1.7f)
            {
                CreateStraightMissile(Vec2(vMonPos.x, vMonPos.y), vMonScale, 400.f, MISSILE_TYPE::BSMALL);
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

void CBossPatorl3STate::Enter()
{
    Vec2 vResolution = CCore::GetInst()->GetResolution();
    Vec2 vBackPos = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetPos();
    Vec2 vBackScale = CSceneMgr::GetInst()->GetCurScene()->GetBackground()->GetScale();
    RECT rBackrect = { (vBackPos.x - vBackScale.x / 2.f),(vBackPos.y - vBackScale.y / 2.f),(vBackPos.x + vBackScale.x / 2.f),(vBackPos.x + vBackScale.y / 2.f) };
    m_pDesPos[0] = Vec2(vBackPos.x, vBackPos.y - 400.f);
    m_pDesPos[1] = Vec2(vBackPos.x + 30.f, vBackPos.y + 200.f);
    m_pDesPos[2] = Vec2(vBackPos.x, vBackPos.y - 400.f);
}

void CBossPatorl3STate::Exit()
{
}


