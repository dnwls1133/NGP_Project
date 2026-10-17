#include "pch.h"
#include "CMissile.h"
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

CMissile::CMissile()
	: m_dAcc(0.f)
    , m_bHit(false)
    ,m_fTheta(0.f)
	, m_vDir(Vec2(1.f,1.f))
	, m_pTex(nullptr)
	, m_fVec(0.f)

{
	
	m_vDir.Normalize();
	CreaeteCollider();
	GetCollider()->SetScale(Vec2(15.f, 15.f));
	

	
	

}

CMissile::~CMissile()
{
}

void CMissile::init(GROUP_TYPE _eType)
{
	switch (_eType)
	{
	case GROUP_TYPE::PROJ_PLAYER:
	{
		m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerMissileTex0", L"texture\\Missile00.bmp");
		CreaeteAnimator();


		/*GetAnimator()->CreateAnimation(L"PlayerMissile0", m_pTex, Vec2(0.f, 0.f), Vec2(50.f, 89.f), Vec2(50.f, 0.f), 0.5f, 4);
        GetAnimator()->FindAnimation(L"PlayerMissile0")->Save(L"animation\\PlayerMissile0.anim");*/

       /* m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerMissileTex1", L"texture\\Player_Missile_LV1.png");
        GetAnimator()->CreateAnimation(L"PlayerMissile1", m_pTex, Vec2(0.f, 0.f), Vec2(50.f, 89.f), Vec2(50.f, 0.f), 0.5f, 4);
        GetAnimator()->FindAnimation(L"PlayerMissile1")->Save(L"animation\\PlayerMissile1.anim");
        m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerMissileTex2", L"texture\\Player_Missile_LV2.png");
        GetAnimator()->CreateAnimation(L"PlayerMissile2", m_pTex, Vec2(0.f, 0.f), Vec2(50.f, 89.f), Vec2(50.f, 0.f), 0.5f, 4);
        GetAnimator()->FindAnimation(L"PlayerMissile2")->Save(L"animation\\PlayerMissile2.anim");
        m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerMissileTex3", L"texture\\Player_Missile_LV3.png");
        GetAnimator()->CreateAnimation(L"PlayerMissile3", m_pTex, Vec2(0.f, 0.f), Vec2(50.f, 89.f), Vec2(50.f, 0.f), 0.5f, 4);
        GetAnimator()->FindAnimation(L"PlayerMissile3")->Save(L"animation\\PlayerMissile3.anim");
        m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerMissileTex4", L"texture\\Player_Missile_LV4.png");
        GetAnimator()->CreateAnimation(L"PlayerMissile4", m_pTex, Vec2(0.f, 0.f), Vec2(50.f, 89.f), Vec2(50.f, 0.f), 0.5f, 4);
        GetAnimator()->FindAnimation(L"PlayerMissile4")->Save(L"animation\\PlayerMissile4.anim");
        m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerMissileTex5", L"texture\\Player_Missile_LV5.png");
        GetAnimator()->CreateAnimation(L"PlayerMissile5", m_pTex, Vec2(0.f, 0.f), Vec2(50.f, 89.f), Vec2(50.f, 0.f), 0.5f, 4);
        GetAnimator()->FindAnimation(L"PlayerMissile5")->Save(L"animation\\PlayerMissile5.anim");
        m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerMissileTex6", L"texture\\Player_Missile_LV6.png");
        GetAnimator()->CreateAnimation(L"PlayerMissile6", m_pTex, Vec2(0.f, 0.f), Vec2(50.f, 89.f), Vec2(50.f, 0.f), 0.5f, 4);
        GetAnimator()->FindAnimation(L"PlayerMissile6")->Save(L"animation\\PlayerMissile6.anim");*/


		//GetAnimator()->Play(L"PlayerMissile0", true);
	}
		
		break;
	case GROUP_TYPE::PROJ_MONSTER:
	{
		m_pTex = CResMgr::GetInst()->LoadTexture(L"MonsterMissileTex0", L"texture\\Monster_Middle_Bullet1.png");
		CreaeteAnimator();
		//GetAnimator()->CreateAnimation(L"MonsterMissile0", m_pTex, Vec2(0.f, 0.f), Vec2(32.f, 32.f), Vec2(32.f, 0.f), 0.5f, 4);
  //      m_pTex = CResMgr::GetInst()->LoadTexture(L"MonsterMissileTex1", L"texture\\Monster_Middle_Bullet2.png");
  //      GetAnimator()->CreateAnimation(L"MonsterMissile1", m_pTex, Vec2(0.f, 0.f), Vec2(80.f, 64.f), Vec2(80.f, 0.f), 0.05f, 6);
  //      m_pTex = CResMgr::GetInst()->LoadTexture(L"MonsterMissileTex2", L"texture\\Monster_Huge_Bullet1.png");
  //      GetAnimator()->CreateAnimation(L"MonsterMiddleMissile1", m_pTex, Vec2(0.f, 0.f), Vec2(80.f, 80.f), Vec2(80.f, 0.f), 0.1f, 4);
  //      m_pTex = CResMgr::GetInst()->LoadTexture(L"MonsterMissileTex3", L"texture\\Monster_Middle_Bullet3.png");
  //      GetAnimator()->CreateAnimation(L"MonsterMiddleMissile2", m_pTex, Vec2(0.f, 0.f), Vec2(64.f, 64.f), Vec2(64.f, 0.f), 0.1f, 4);
  //      //Animation 저장
  //      GetAnimator()->FindAnimation(L"MonsterMissile0")->Save(L"animation\\MonsterMissile0.anim");
  //      GetAnimator()->FindAnimation(L"MonsterMissile1")->Save(L"animation\\MonsterMissile1.anim");
  //      GetAnimator()->FindAnimation(L"MonsterMiddleMissile1")->Save(L"animation\\MonsterMiddleMissile1.anim");
  //      GetAnimator()->FindAnimation(L"MonsterMiddleMissile2")->Save(L"animation\\MonsterMiddleMissile2.anim");

        //GetAnimator()->Play(L"MonsterMissile0", true);
	}
	break;
	}

}
void CMissile::ChangemType(MISSILE_TYPE _eType)
{
    switch (_eType)
    {
    case MISSILE_TYPE::SMALL:
    {
        GetAnimator()->LoadAnimation(L"animation\\MonsterMissile0.anim");
        GetAnimator()->Play(L"MonsterMissile0", true);
        GetCollider()->SetScale(Vec2(15.f, 15.f));
    }
    break;
    case MISSILE_TYPE::SPIN:
    {
        GetAnimator()->LoadAnimation(L"animation\\MonsterMissile1.anim");
        GetAnimator()->Play(L"MonsterMissile1", true);
        GetCollider()->SetScale(Vec2(20.f, 20.f));
    }
    break;
    case MISSILE_TYPE::MIDDLE:
    {
        GetAnimator()->LoadAnimation(L"animation\\MonsterMiddleMissile1.anim");
        GetAnimator()->Play(L"MonsterMiddleMissile1", true);
        GetCollider()->SetScale(Vec2(40.f, 40.f));
    }
    break;
    case MISSILE_TYPE::BSMALL:
    {

        GetAnimator()->LoadAnimation(L"animation\\MonsterMiddleMissile2.anim");
        GetAnimator()->Play(L"MonsterMiddleMissile2", true);
        GetCollider()->SetScale(Vec2(20.f, 20.f));
    }
    break;
    case MISSILE_TYPE::LVL1:
    {
        GetAnimator()->LoadAnimation(L"animation\\PlayerMissile1.anim");
        GetAnimator()->Play(L"PlayerMissile1", true);
        GetCollider()->SetScale(Vec2(20.f, 20.f));
    }
    break;
    case MISSILE_TYPE::LVL2:
    {
        GetAnimator()->LoadAnimation(L"animation\\PlayerMissile2.anim");
        GetAnimator()->Play(L"PlayerMissile2", true);
        GetCollider()->SetScale(Vec2(20.f, 20.f));
    }
    break;
    case MISSILE_TYPE::LVL3:
    {
        GetAnimator()->LoadAnimation(L"animation\\PlayerMissile3.anim");
        GetAnimator()->Play(L"PlayerMissile3", true);
        GetCollider()->SetScale(Vec2(20.f, 20.f));
    }
    break;
    case MISSILE_TYPE::LVL4:
    {
        GetAnimator()->LoadAnimation(L"animation\\PlayerMissile4.anim");
        GetAnimator()->Play(L"PlayerMissile4", true);
        GetCollider()->SetScale(Vec2(20.f, 20.f));
    }
    break;
    case MISSILE_TYPE::LVL5:
    {
        GetAnimator()->LoadAnimation(L"animation\\PlayerMissile5.anim");
        GetAnimator()->Play(L"PlayerMissile5", true);
        GetCollider()->SetScale(Vec2(20.f, 20.f));
    }
    break;
    case MISSILE_TYPE::LVL6:
    {
        GetAnimator()->LoadAnimation(L"animation\\PlayerMissile6.anim");
        GetAnimator()->Play(L"PlayerMissile6", true);
        GetCollider()->SetScale(Vec2(20.f, 20.f));
    }
    break;
    }
}
void CMissile::update()
{
	Vec2 vPos = GetPos();
	Vec2 resolution = CCore::GetInst()->GetResolution();
    CBackground* Background = (CBackground*)CSceneMgr::GetInst()->GetCurScene()->GetBackground();
    Vec2 vBackPos = Background->GetPos();
    Vec2 vBackScale = Background->GetScale();

  
    if (m_bHit)
    {
        m_dAcc += fDT;
        if (1.f + fDT >= m_dAcc && m_dAcc > 1.f)
        {


            DeleteObject(this);
            return;
        }
    }
	else if (vPos.y > resolution.y || vPos.y < -50.f
        || vPos.x > vBackPos.x + vBackScale.x/2 + 50.f
        || vPos.x < vBackPos.x - vBackScale.x/2 - 50.f && m_bHit==false)
	{
		DeleteObject(this);
        return;
	}
	//vPos.x += 700.f * cos(m_fTheta) * fDT;
	//vPos.y -= 700.f * sin(m_fTheta) * fDT;

   
   
    if (GetName() == L"MsMissile")
    {

        switch (m_iType)
        {
        case 3:
        {
            if (m_dAcc >= 0)
            {
                m_dAcc += fDT;
            }

            if (m_dAcc > 0.7f)
            {
                m_fVec /= 2.f;
                m_dAcc = -1.f;
            }
        }
        break;
        case 2:
        {
            if (m_dAcc >= 0)
            {
                m_dAcc += fDT;
            }

            if (m_dAcc > 0.5f)
            {
                m_fVec *= 1.5f;
                m_dAcc = -1.f;
            }
        }
        break;
        }
        
    }
	vPos.x += m_fVec * m_vDir.x * fDT;
	vPos.y -= m_fVec * m_vDir.y * fDT;
	SetPos(vPos);
	if (nullptr != GetAnimator())
	{
		GetAnimator()->update();
	}
	
}

void CMissile::render(HDC _dc)
{
	//Vec2 vPos = GetPos();
	//Vec2 vScale = GetScale();

	//Ellipse(_dc, (int)vPos.x - vScale.x / 2.f
	//	, (int)vPos.y - vScale.y / 2.f
	//	, (int)vPos.x + vScale.x / 2.f
	//	, (int)vPos.y + vScale.y / 2.f);

	//switch (m_iType)
	//{
	//case 0:
	//{
	//	
	//	Vec2 Scale;
	//	Scale.x = (1 * m_pTex->Width()) / 3;
	//	Scale.y = (1 * m_pTex->Height())/3;
	//	GetCollider()->SetScale(Scale);
	//	
	//}
	//break;
	//case 1:
	//{
	//	m_pTex = CResMgr::GetInst()->LoadTexture(L"MissileTex0R", L"texture\\Missile0R.bmp");
	//	GetCollider()->SetScale(Vec2(20.f, 20.f));
	//	int iWidith = (int)m_pTex->Width();
	//	int iHeight = (int)m_pTex->Height();
	//	Vec2 vPos = GetPos();



	//	//BitBlt(_dc, int(vPos.x - (float)(iWidith / 2))
	//	//	, int(vPos.y - (float)(iHeight / 2))
	//	//	, iWidith, iHeight
	//	//	, m_pTex->GetDC()
	//	//	, 0, 0, SRCCOPY);

	//	TransparentBlt(_dc
	//		, int(vPos.x - (float)(iWidith / 2))
	//		, int(vPos.y - (float)(iHeight / 2))
	//		, iWidith, iHeight
	//		, m_pTex->GetDC()
	//		, 0, 0, iWidith, iHeight
	//		, RGB(255, 0, 255)); // 색상을 무시하고 나머지 복사해라
	//	break;
	//}
	//case 3:
	//{
	//	m_pTex = CResMgr::GetInst()->LoadTexture(L"Missileboom0Tex", L"texture\\Missileboom0.bmp");
	//	int iWidith = (int)m_pTex->Width();
	//	int iHeight = (int)m_pTex->Height();
	//	Vec2 vPos = GetPos();



	//	//BitBlt(_dc, int(vPos.x - (float)(iWidith / 2))
	//	//	, int(vPos.y - (float)(iHeight / 2))
	//	//	, iWidith, iHeight
	//	//	, m_pTex->GetDC()
	//	//	, 0, 0, SRCCOPY);

	//	TransparentBlt(_dc
	//		, int(vPos.x - (float)(iWidith / 2))
	//		, int(vPos.y - (float)(iHeight / 2))
	//		, iWidith, iHeight
	//		, m_pTex->GetDC()
	//		, 0, 0, iWidith, iHeight
	//		, RGB(255, 0, 255)); // 색상을 무시하고 나머지 복사해라
	//}
	//break;
	//}
	
	component_render(_dc);
}



void CMissile::OnCollisionEnter(CCollider* _pOther)
{
	CObject* pOtherObj = _pOther->GetObj();
    CMonster* pMonsObj = (CMonster*)_pOther->GetObj();

	if (GetName() != L"MsMissile" && pOtherObj->GetName() == L"Monster" && !pMonsObj->GetsignalDead() && !m_bHit)
	{
		m_pTex = CResMgr::GetInst()->LoadTexture(L"MissileTex1", L"texture\\Missile_explosion.png");
		CreaeteAnimator();
		GetAnimator()->CreateAnimation(L"Missile1", m_pTex, Vec2(0.f, 0.f), Vec2(33.88f, 78.f), Vec2(33.88f, 0.f), 0.05f, 17);
		GetAnimator()->Play(L"Missile1", false);
		CAnimation* pAnim = GetAnimator()->FindAnimation(L"Missile1");
		
		for (int i = 0; i < pAnim->GetMaxFrame(); ++i)
		{
			pAnim->GetFrame(i).vOffset = Vec2(0.f, -20.f);
		}
        m_bHit = true;
        SetCollideroff();
		m_fVec = 100.f;
	}
    
}

void CMissile::OnCollision(CCollider* _pOther)
{
	
}

void CMissile::OnCollisionExit(CCollider* _pOther)
{
 

       
   
}
