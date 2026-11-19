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



void CItem::InitAfterPlacement()
{
    Vec2 vIpos = GetPos();
    CScene* pCurScene = CSceneMgr::GetInst()->GetCurScene();
    CPlayer* pTarget = (pCurScene != nullptr) ? pCurScene->
        SelectNearestAlivePlayer(vIpos) : nullptr;
    if (pTarget == nullptr) {
        m_vDir = Vec2(0.f, -1.f);
        return;
    }

    Vec2 vPpos = pTarget->GetPos();
    vPpos.x -= vIpos.x;
    vPpos.y = vIpos.y - vPpos.y;
    m_vDir = vPpos.Normalize();

}

void CItem::ResolveSimultaneousPickup()
{
    if (m_bCollected || IsDead() || m_TouchPlayers.empty())
        return;

    CPlayer* winner = nullptr;
    float bestDistSq = 0.0f;
    const Vec2 itemPos = GetPos();
    for (CPlayer* candidate : m_TouchPlayers) {
        if (candidate == nullptr || candidate->IsDead() ||
            !candidate->IsAlivePlayer())
            continue;
        Vec2 diff = candidate->GetPos() - itemPos;
        float distSq = diff.x * diff.x + diff.y * diff.y;

        if (winner == nullptr
            || distSq < bestDistSq
            || (distSq == bestDistSq &&
                candidate->GetPlayerId() < winner->GetPlayerId())) {
            winner = candidate;
            bestDistSq = distSq;
        }
    }

    m_TouchPlayers.clear();

    if (winner != nullptr) {
        m_bCollected = true;
        SetCollideroff();
        winner->ApplyItemReward();
        DeleteObject(this);
    }
}

protocol::SpawnPayload CItem::ExportSpawnPayload(protocol::EventId eventId) const
{
    protocol::SpawnPayload spawn{};
    spawn.eventId = eventId;
    spawn.objectId = GetObjectId();
    spawn.kind = protocol::NetObjectKind::ITEM;
    spawn.subType = static_cast<std::uint8_t>(m_iType);
    spawn.ownerPlayerId = protocol::INVALID_PLAYER_SLOT;
    spawn.pos.x = static_cast<std::int32_t>(GetPos().x * 100.f);
    spawn.pos.y = static_cast<std::int32_t>(GetPos().y  * 100.f);
    spawn.vel.x = static_cast<std::int32_t>(m_vDir.x * m_fVec * 100.f);
    spawn.vel.y = static_cast<std::int32_t>(m_vDir.y * m_fVec * 100.f);
    spawn.hp = 1;
    return spawn;
}

void CItem::update()
{
    ResolveSimultaneousPickup(); // 이전 충돌 단계에서 모인 동시 접촉 후보가 있으면 단일 획득자 확정
    if (m_bCollected || IsDead())
        return;
    Vec2 vIpos = GetPos();
    
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
    if (m_bCollected || IsDead())
        return;
    CObject* pOtherObj = _pOther->GetObj();

    if (pOtherObj != nullptr && pOtherObj->GetName() == L"Player")
    {
        CPlayer* pPlayer = static_cast<CPlayer*>(pOtherObj);
        for (CPlayer* existing : m_TouchPlayers) {
            if (existing == pPlayer)
                return; // 이미 후보 목록에 있음
        }
        m_TouchPlayers.push_back(pPlayer);
    }
}

void CItem::OnCollision(CCollider* _pOther)
{
}

void CItem::OnCollisionExit(CCollider* _pOther)
{
}


