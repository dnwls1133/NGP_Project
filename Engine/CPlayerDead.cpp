#include "pch.h"
#include "CPlayerDead.h"
#include"CResMgr.h"
#include"CTimeMgr.h"
#include "CSoundMgr.h"


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
    SetName(L"Player_Dead");

	GetAnimator()->Play(L"Player_Dead", false);
    auto* sound = CResMgr::GetInst()->LoadSound(L"Player Dead", L"sound\\SE\\SE_EnemyDead.mp3");
    CSoundMgr::GetInst()->PlaySE(sound);
}

CPlayerDead::~CPlayerDead()
{
}

protocol::SpawnPayload CPlayerDead::ExportSpawnPayload(protocol::EventId eventId) const
{
    auto spawn = CObject::ExportSpawnPayload(eventId);
    spawn.kind = protocol::NetObjectKind::PLAYER_DEATH_EFFECT;
    return spawn;
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

