#include "pch.h"
#include "CItem.h"
#include <cmath>
#include "CPlayer.h"

#include "CTimeMgr.h"
#include "CResMgr.h"

#include "CSceneMgr.h"
#include "CScene.h"
#include "CScene_Stage01.h"

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
    , m_iType(0)
{
    CreaeteCollider();
    GetCollider()->SetScale(Vec2(20.f, 20.f));
    CTexture* m_pTex = CResMgr::GetInst()->LoadTexture(L"ItemTex0", L"texture\\Item.png");
    CreaeteAnimator();
    GetAnimator()->CreateAnimation(L"Item0", m_pTex, Vec2(0.f, 0.f), Vec2(20.f, 20.f), Vec2(20.f, 0.f), 0.05f, 15);
    GetAnimator()->Play(L"Item0", true);
}

CItem::~CItem()
{
}

void CItem::InitAfterPlacement()
{
    const Vec2 vIpos = GetPos();
    const CScene* pCurScene = CSceneMgr::GetInst()->GetCurScene();
    const CPlayer* pTarget = pCurScene->SelectNearestAlivePlayer(vIpos);
    if (pTarget == nullptr)
    {
        m_vDir = Vec2(0.f, -1.f);
        return;
    }

    Vec2 vPpos = pTarget->GetPos();
    vPpos.x -= vIpos.x;
    vPpos.y = vIpos.y - vPpos.y;
    m_vDir = vPpos.Normalize();
}

void CItem::ApplySpawnState(const protocol::SpawnPayload& spawn)
{
    SetObjectId(spawn.objectId);
    SetName(L"Item");
    SetPos(Vec2(spawn.pos.x / 100.f, spawn.pos.y / 100.f));
    m_iType = spawn.subType;

    Vec2 velocity(spawn.vel.x / 100.f, spawn.vel.y / 100.f);
    m_fVec = velocity.Length();
    m_vDir = m_fVec > 0.f ? Vec2(velocity.x / m_fVec, velocity.y / m_fVec) : Vec2(0.f, 0.f);
}

void CItem::ResolveSimultaneousPickup()
{
    if (!CCore::GetInst()->GetSession().IsHost())
    {
        return;
    }

    if (m_bCollected || IsDead() || m_TouchPlayers.empty())
    {
        return;
    }

    CPlayer* winner = nullptr;
    float bestDistSq = 0.0f;
    const Vec2 itemPos = GetPos();
    for (CPlayer* candidate : m_TouchPlayers)
    {
        assert(candidate != nullptr);

        if (candidate->IsDead() || !candidate->IsAlivePlayer())
        {
            continue;
        }
        Vec2 diff = candidate->GetPos() - itemPos;
        float distSq = diff.x * diff.x + diff.y * diff.y;

        if (winner == nullptr ||
            distSq < bestDistSq ||
            (distSq == bestDistSq && candidate->GetPlayerId() < winner->GetPlayerId()))
        {
            winner = candidate;
            bestDistSq = distSq;
        }
    }

    m_TouchPlayers.clear();

    if (winner != nullptr)
    {
        auto* scene = CSceneMgr::GetInst()->GetCurScene();
        auto* stage = dynamic_cast<CScene_Stage01*>(scene);
        assert(stage != nullptr);

        if (!stage->ConfirmItemPickup(GetObjectId(), winner->GetPlayerId()))
        {
            return;
        }

        stage->QueueDespawn(GetObjectId(), protocol::DespawnReason::COLLECTED);
        m_bCollected = true;
        SetCollideroff();
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
    ResolveSimultaneousPickup();
    if (m_bCollected || IsDead())
    {
        return;
    }

    Vec2 vIpos = GetPos();
    Vec2 resolution = CCore::GetInst()->GetResolution();
    CBackground* Background = (CBackground*)CSceneMgr::GetInst()->GetCurScene()->GetBackground();
    Vec2 vBackPos = Background->GetPos();
    Vec2 vBackScale = Background->GetScale();

    const float minY = 10.f;
    const float maxY = resolution.y - 10.f;
    const float minX = vBackPos.x - vBackScale.x / 2.f + 20.f;
    const float maxX = vBackPos.x + vBackScale.x / 2.f - 20.f;

    if (vIpos.y > maxY)
        m_vDir.y = std::fabs(m_vDir.y);      // y -= dir.y -> 위로(안쪽)
    else if (vIpos.y < minY)
        m_vDir.y = -std::fabs(m_vDir.y);     // y -= dir.y -> 아래로(안쪽)

    if (vIpos.x > maxX)
        m_vDir.x = -std::fabs(m_vDir.x);     // x += dir.x -> 왼쪽으로(안쪽)
    else if (vIpos.x < minX)
        m_vDir.x = std::fabs(m_vDir.x);      // x += dir.x -> 오른쪽으로(안쪽)


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


