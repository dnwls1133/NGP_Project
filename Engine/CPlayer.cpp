#include "pch.h"
#include "CPlayer.h"


#include "CSceneMgr.h"
#include "CKeyMgr.h"
#include "CTimeMgr.h"
#include "CPathMgr.h"
#include "CResMgr.h"
#include "CSoundMgr.h"


#include "CScene.h"
#include "CScene_Stage01.h"
#include "CMissile.h"


#include "CBackground.h"

#include "CPlayerDead.h"

#include "CTexture.h"
#include "CSound.h"
#include "CCollider.h"
#include "CAnimator.h"
#include "CAnimation.h"

CPlayer::CPlayer()
	:dAcc(0.)
	,m_iHp(3)
    ,m_iAtk(2)
    ,dStartAcc(3.f)
    ,m_bHit(false)
    ,m_iLvl(1)
    ,m_iBoomb(1)
    ,m_clear(false)
    ,m_dead(false)
	
{
	//Texture 로딩하기
	//m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerTex", L"texture\\Player.bmp");
	CreaeteCollider();
	GetCollider()->SetScale(Vec2(10.f, 15.f));
	GetCollider()->SetOffsetPos(Vec2(0.f, 20.f));

    m_pFireSE = CResMgr::GetInst()->LoadSound(L"Player Fire", L"sound\\SE\\SE_FireDanmaku.mp3");
    m_pLvUpSE = CResMgr::GetInst()->LoadSound(L"Player Lv Up", L"sound\\SE\\SE_LevelUp.mp3");
    m_pDeadSE = CResMgr::GetInst()->LoadSound(L"Player Dead", L"sound\\SE\\SE_EnemyDead.mp3");

	CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerWalk", L"texture\\Player\\Player_Walk.png");
	CreaeteAnimator();

    GetAnimator()->LoadAnimation(L"animation\\player_walk_left.anim");

	/*GetAnimator()->CreateAnimation(L"Player_walk",m_pTex, Vec2(0.f, 0.f), Vec2(140.f, 140.f), Vec2(140.f, 0.f), 0.05f, 19);
    m_pTex = CResMgr::GetInst()->LoadTexture(L"PlayerInvincible", L"texture\\Player\\Player_Invincible.png");
    GetAnimator()->CreateAnimation(L"Player_Invincible", m_pTex, Vec2(0.f, 0.f), Vec2(140.f, 140.f), Vec2(140.f, 0.f), 0.05f, 20);

    Animation 저장해보기
    GetAnimator()->FindAnimation(L"Player_walk")->Save(L"animation\\player_walk_left.anim");
    GetAnimator()->FindAnimation(L"Player_Invincible")->Save(L"animation\\player_Invincible.anim");*/

    GetAnimator()->Play(L"Player_walk", true);
}

CPlayer::~CPlayer()
{
	
}
void CPlayer::update()
{
	Vec2 vPos = GetPos();
    CBackground* back = (CBackground*)CSceneMgr::GetInst()->GetCurScene()->GetBackground();
    Vec2 vBackpos = back->GetPos();
    Vec2 vBackScale = back->GetScale();
    if (!m_clear && !m_dead)
    {
        if (dStartAcc > 0.f)
        {
            dStartAcc -= fDT;
            vPos.y -= 100.f * fDT;
            SetPos(vPos);
        }
        else
        {
            if (m_bHit == true)
            {
                dAcc += fDT;
                if (dAcc < 1.5f)
                {
                    vPos.y -= 300.f * fDT;
                }
                else
                {
                    if (KEY_HOLD(KEY::UP))
                    {
                        if (vPos.y >= vBackpos.y - vBackScale.y / 2.f)
                        {
                            vPos.y -= 300.f * fDT;
                        }

                    }
                    if (KEY_HOLD(KEY::DOWN))
                    {
                        if (vPos.y <= vBackpos.y + vBackScale.y / 2.f)
                        {
                            vPos.y += 300.f * fDT;
                        }

                    }
                    if (KEY_HOLD(KEY::LEFT))
                    {
                        if (vPos.x >= vBackpos.x - vBackScale.x / 2.f)
                        {
                            vPos.x -= 300.f * fDT;
                        }

                    }
                    if (KEY_HOLD(KEY::RIGHT))
                    {
                        if (vPos.x <= vBackpos.x + vBackScale.x / 2.f)
                        {
                            vPos.x += 300.f * fDT;
                        }

                    }

                }
                if (dAcc > 2.f)
                {
                    dAcc = 0;
                    GetAnimator()->LoadAnimation(L"animation\\player_walk_left.anim");
                    GetAnimator()->Play(L"Player_walk", true);
                    SetCollideron();
                    m_bHit = false;
                }
                SetPos(vPos);

            }
            else
            {
                if (KEY_HOLD(KEY::UP))
                {
                    if (vPos.y >= vBackpos.y - vBackScale.y/2.f)
                    {
                        vPos.y -= 300.f * fDT;
                    }
                   
                }
                if (KEY_HOLD(KEY::DOWN))
                {
                    if (vPos.y <= vBackpos.y + vBackScale.y / 2.f)
                    {
                        vPos.y += 300.f * fDT;
                    }
                    
                }
                if (KEY_HOLD(KEY::LEFT))
                {
                    if (vPos.x >= vBackpos.x - vBackScale.x / 2.f)
                    {
                        vPos.x -= 300.f * fDT;
                    }
                    
                }
                if (KEY_HOLD(KEY::RIGHT))
                {
                    if (vPos.x <= vBackpos.x + vBackScale.x / 2.f)
                    {
                        vPos.x += 300.f * fDT;
                    }
                    
                }
                if (KEY_TAP(KEY::L))
                {
                    m_iLvl = 5;
                }
                if (KEY_HOLD((KEY::S)))
                {
                    dAcc += fDT;
                    switch (m_iLvl)
                    {
                    case 1:
                    {
                        if (dAcc > 0.15f)
                        {
                            dAcc = 0;
                            CreateMissile(0, 1500.f, MISSILE_TYPE::LVL1);
                            CSoundMgr::GetInst()->PlaySE(m_pFireSE);
                        }
                    }
                    break;
                    case 2:
                    {
                        if (dAcc > 0.15f)
                        {
                            dAcc = 0;
                            CreateMissile(0, 1500.f, MISSILE_TYPE::LVL1);
                            CreateMissile(1, 1500.f, MISSILE_TYPE::LVL1);
                            CreateMissile(2, 1500.f, MISSILE_TYPE::LVL1);
                            CreateMissile(3, 1500.f, MISSILE_TYPE::LVL4);
                            CreateMissile(4, 1500.f, MISSILE_TYPE::LVL4);
                            CSoundMgr::GetInst()->PlaySE(m_pFireSE);
                        }
                    }
                    break;
                    case 3:
                    {
                        if (dAcc > 0.15f)
                        {
                            dAcc = 0;
                            CreateMissile(0, 1700.f, MISSILE_TYPE::LVL2);
                            CreateMissile(0, 1700.f, MISSILE_TYPE::LVL2);
                            CreateMissile(1, 1700.f, MISSILE_TYPE::LVL5);
                            CreateMissile(2, 1700.f, MISSILE_TYPE::LVL5);
                            CreateMissile(3, 1700.f, MISSILE_TYPE::LVL5);
                            CreateMissile(4, 1700.f, MISSILE_TYPE::LVL5);
                            CSoundMgr::GetInst()->PlaySE(m_pFireSE);
                        }
                    }
                    break;
                    case 4:
                    {
                        if (dAcc > 0.15f)
                        {
                            dAcc = 0;
                            CreateMissile(0, 1700.f, MISSILE_TYPE::LVL3);
                            CreateMissile(1, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(2, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(3, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(4, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(5, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(6, 1700.f, MISSILE_TYPE::LVL6);
                            CSoundMgr::GetInst()->PlaySE(m_pFireSE);
                        }
                    }
                    break;
                    case 5:
                    {
                        if (dAcc > 0.15f)
                        {
                            dAcc = 0;

                            CreateMissile(1, 1700.f, MISSILE_TYPE::LVL3);
                            CreateMissile(2, 1700.f, MISSILE_TYPE::LVL3);
                            CreateMissile(0, 1700.f, MISSILE_TYPE::LVL3);
                            CreateMissile(3, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(4, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(5, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(6, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(7, 1700.f, MISSILE_TYPE::LVL6);
                            CreateMissile(8, 1700.f, MISSILE_TYPE::LVL6);
                            CSoundMgr::GetInst()->PlaySE(m_pFireSE);
                        }
                    }
                    break;
                    }

                   
                }
                SetPos(vPos);
            }
        }
    }
    else if(m_clear) {
        vPos.y -= 300.f * fDT;
        SetPos(vPos);
    }
   
  
	
	
	//GetAnimator()->update();
}

void CPlayer::render(HDC _dc)
{
	/*int iWidith = (int)m_pTex->Width();
	int iHeight = (int)m_pTex->Height();
	Vec2 vPos = GetPos();*/

	//BitBlt(_dc, int(vPos.x - (float)(iWidith / 2))
	//	, int(vPos.y - (float)(iHeight / 2))
	//	, iWidith, iHeight
	//	, m_pTex->GetDC()
	//	, 0, 0, SRCCOPY);

	//TransparentBlt(_dc
	//	, int(vPos.x - (float)(iWidith / 2))
	//	, int(vPos.y - (float)(iHeight / 2))
	//	, iWidith, iHeight
	//	, m_pTex->GetDC()
	//	, 0, 0, iWidith, iHeight
	//	, RGB(255, 0, 255)); // 색상을 무시하고 나머지 복사해라
	
	component_render(_dc);
   /* CTexture* pTex  = CResMgr::GetInst()->LoadTexture(L"M4Tex", L"texture\\Player_Walk_A.png");

    Vec2 vPos = GetPos();
    vPos = CCamera::GetInst()->GetRenderPos(vPos);

    float width = (float)pTex->Width();
    float height = (float)pTex->Height();

    BLENDFUNCTION bf = {};

    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.AlphaFormat = AC_SRC_ALPHA;
    bf.SourceConstantAlpha = 255;

    AlphaBlend(_dc
        , int(vPos.x - width / 2.f)
        , int(vPos.y - height / 2.f)
        , width, height
        , pTex->GetDC()
        , 0, 0, width, height
        , bf);*/
}

void CPlayer::CreateMissile(int type,  float _fVec,MISSILE_TYPE _eType)
{
	Vec2 vMissilePos = GetPos();
	vMissilePos.y -= GetScale().y / 2.f;
	switch (type)
	{
	case 1:
	{
		vMissilePos.x -= 20.f;
        vMissilePos.y += 20.f;
	}
		break;
	case 2:
	{
		vMissilePos.x += 20.f;
        vMissilePos.y += 20.f;
	}
		break;
    case 3:
    {
        vMissilePos.x -= 35.f;
        vMissilePos.y += 40.f;
    }
    break;
    case 4:
    {
        vMissilePos.x += 35.f;
        vMissilePos.y += 40.f;
    }
    break;
    case 5:
    {
        vMissilePos.x += 50.f;
        vMissilePos.y += 60.f;
    }
    break;
    case 6:
    {
        vMissilePos.x -= 50.f;
        vMissilePos.y += 60.f;
    }
    break;
    case 7:
    {
        vMissilePos.x += 65.f;
        vMissilePos.y += 80.f;
    }
    break;
    case 8:
    {
        vMissilePos.x -= 65.f;
        vMissilePos.y += 80.f;
    }
    break;
	}
	// Missile Object
	CMissile* pMissile = new CMissile;
	pMissile->init(GROUP_TYPE::PROJ_PLAYER);
	pMissile->SetPos(vMissilePos);
	pMissile->SetScale(Vec2(25.f, 25.f));
	pMissile->SetDir(Vec2(0.f,1.f));
    pMissile->ChangemType(_eType);
	pMissile->SetVec(_fVec);
	pMissile->SetType(0);
	pMissile->SetName(L"Missile_Player");

	CreateObject(pMissile,GROUP_TYPE::PROJ_PLAYER);
}



void CPlayer::OnCollisionEnter(CCollider* _pOther)
{
	CObject* pOtherObj = _pOther->GetObj();
  
	if (pOtherObj->GetName() == L"MsMissile" && m_bHit == false)
	{
        if (isMujeok)
            return;

        CSoundMgr::GetInst()->PlaySE(m_pDeadSE);
        if (m_iHp <= 1)
        {
            CScene_Stage01* curscene = (CScene_Stage01*)CSceneMgr::GetInst()->GetCurScene();
            curscene->Fail();
        }
        GetAnimator()->LoadAnimation(L"animation\\player_Invincible.anim");
        GetAnimator()->Play(L"Player_Invincible", true);
		CPlayerDead* pDead = new CPlayerDead;
		pDead->SetPos(GetPos());
		pDead->SetName(L"Player_Dead");
		CreateObject(pDead, GROUP_TYPE::DEAD_PLAYER);
		--m_iHp;
        m_iLvl = 1;
		m_bHit = true;
		Vec2 vPos = GetPos();
		vPos.y = 1200;
		SetPos(vPos);
		SetCollideroff();
        m_iBoomb = 1;
	}

    if (pOtherObj->GetName() == L"Item")
    {
        CSoundMgr::GetInst()->PlaySE(m_pLvUpSE);
        if (m_iLvl < 5)
        {
            m_iLvl += 1;
        }
        else
        {
            CRankMgr::GetInst()->CurrentRanking.Score += 100;
        }
    }
    if (pOtherObj->GetName() == L"Monster")
    {
        if (isMujeok)
            return;

        if (m_iLvl > 1)
        {
            m_iLvl -= 1;

        }
    }
}





