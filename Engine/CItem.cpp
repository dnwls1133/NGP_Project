#include "pch.h"
#include "CItem.h"
#include "CPlayer.h"

#include "CTimeMgr.h"
#include "CResMgr.h"

#include "CSceneMgr.h"
#include "CScene.h"

#include "CTexture.h"
#include "CBackground.h"

#include "CCore.h"
#include "CCollider.h"
#include "CAnimator.h"
#include "CAnimation.h"

#include "CMonster.h"


CItem::CItem()
    :m_dAcc(0.f)
    , m_bHit(false)
    , m_fTheta(0.f)
    , m_vDir(Vec2(1.f, 1.f))
    , m_fVec(200.f)
{
    Vec2 vIpos = GetPos();
    Vec2 vPpos = CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0)->GetPos();
    double dx = vIpos.x;
    double dy = vPpos.y;
    double distance = sqrt(dx * dx + dy * dy);
    if (vIpos.x - vPpos.x < 0)
    {
        vPpos.x = -(vIpos.x - vPpos.x);
    }
    else if (vIpos.x - vPpos.x > 0)
    {
        vPpos.x = -(vIpos.x - vPpos.x);
    }
    vPpos.y = vIpos.y - vPpos.y;
    m_vDir = vPpos.Normalize();
   



    CreaeteCollider();
    GetCollider()->SetScale(Vec2(20.f, 20.f));
    CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"ItemTex0", L"texture\\Item.png");
    CreaeteAnimator();
    GetAnimator()->CreateAnimation(L"Item0", m_pTex, Vec2(0.f, 0.f), Vec2(20.f, 20.f), Vec2(20.f, 0.f), 0.05f, 15);
    GetAnimator()->FindAnimation(L"Item0")->Save(L"animation\\Item0.anim");
    GetAnimator()->Play(L"Item0", true);
}

CItem::~CItem()
{
}



void CItem::update()
{
    Vec2 vIpos = GetPos();
    Vec2 vPpos = CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0)->GetPos();
    Vec2 resolution = CCore::GetInst()->GetResolution();
    CBackground* Background = (CBackground*)CSceneMgr::GetInst()->GetCurScene()->GetBackground();
    Vec2 vBackPos = Background->GetPos();
    Vec2 vBackScale = Background->GetScale();



    if (vIpos.y > resolution.y-10.f || vIpos.y < 10.f)
    {
        m_vDir.y *= -1;
        
    }
    if (vIpos.x > vBackPos.x + vBackScale.x / 2 - 20.f || vIpos.x < vBackPos.x - vBackScale.x / 2 +20.f)
    {
        m_vDir.x *= -1;
    }


    vIpos.x += m_fVec * m_vDir.x * fDT;
    vIpos.y -= m_fVec * m_vDir.y * fDT;
    SetPos(vIpos);
    if (nullptr != GetAnimator())
    {
        GetAnimator()->update();
    }


}

void CItem::render(HDC _dc)
{
    component_render(_dc);
}

void CItem::OnCollisionEnter(CCollider* _pOther)
{
    CObject* pOtherObj = _pOther->GetObj();

    if (pOtherObj->GetName() == L"Player")
    {
        DeleteObject(this);
    }
}

void CItem::OnCollision(CCollider* _pOther)
{
}

void CItem::OnCollisionExit(CCollider* _pOther)
{
}


