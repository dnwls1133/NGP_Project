#include "pch.h"
#include "CExposion.h"

#include"CResMgr.h"
#include"CTimeMgr.h"


#include "CTexture.h"
#include "CCollider.h"
#include "CAnimator.h"
#include "CAnimation.h"


CExposion::CExposion()
    :m_fAcc(0.f)
{
    CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"Eenemy_explosion", L"texture\\Enemies\\Boss_explosion.png");
    CreaeteAnimator();
    GetAnimator()->CreateAnimation(L"Eenemy_explosion", m_pTex, Vec2(0.f, 0.f), Vec2(128.f, 128.f), Vec2(128.f, 0.f), 0.1f, 17);
    GetAnimator()->FindAnimation(L"Eenemy_explosion")->Save(L"Eenemy_explosion.anim");

    GetAnimator()->Play(L"Eenemy_explosion", true);
}

CExposion::~CExposion()
{
    
}

void CExposion::update()
{
    GetAnimator()->update();
}

void CExposion::render(HDC _dc)
{
    component_render(_dc);
}

