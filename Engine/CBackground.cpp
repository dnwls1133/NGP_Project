#include "pch.h"
#include "CBackground.h"
#include "CResMgr.h"
#include "CTimeMgr.h"

#include "CCore.h"
#include "CTexture.h"

#include "CAnimator.h"
#include "CAnimation.h"

CBackground::CBackground()
	: m_pTex(nullptr)
	, m_dAcc(0.f)
	, m_strRelativePath{}
	, m_strKey{}
	, m_iAnimationidx(0)
{
	CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"Background0tex", L"texture\\Stage1\\Stage1_Sprite.png");
    CreaeteAnimator();
    GetAnimator()->CreateAnimation(L"Stage1_Background",m_pTex, Vec2(0.f, 0.f), Vec2(540.f, 960.f), Vec2(540.f, 0.f), 0.048f, 60);
    

      // Animation 저장해보기
    GetAnimator()->FindAnimation(L"Stage1_Background")->Save(L"animation\\Stage1_Background.anim");

    GetAnimator()->Play(L"Stage1_Background", true);
}
CBackground::~CBackground()
{

}

void CBackground::update()
{
	//m_dAcc += fDT;

	
	
}

void CBackground::render(HDC _dc)
{
	

    component_render(_dc);
	

	
}
