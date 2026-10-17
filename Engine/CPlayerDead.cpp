#include "pch.h"
#include "CPlayerDead.h"
#include"CResMgr.h"
#include"CTimeMgr.h"


#include "CTexture.h"
#include "CCollider.h"
#include "CAnimator.h"
#include "CAnimation.h"
CPlayerDead::CPlayerDead()
	:m_fAcc(0.f)
{

	CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"Player_Dead", L"texture\\Player\\Player_Dead.png");
	CreaeteAnimator();
	GetAnimator()->CreateAnimation(L"Player_Dead", m_pTex, Vec2(0.f, 0.f), Vec2(140.f, 140.f), Vec2(140.f, 0.f), 0.05f, 20);
    GetAnimator()->FindAnimation(L"Player_Dead")->Save(L"Player_Dead.anim");

	GetAnimator()->Play(L"Player_Dead", false);
}

CPlayerDead::~CPlayerDead()
{
}

void CPlayerDead::update()
{
	m_fAcc += fDT;
	if (m_fAcc > 1.f)
	{
		DeleteObject(this);
	}
	GetAnimator()->update();
}

void CPlayerDead::render(HDC _dc)
{
	component_render(_dc);
}

