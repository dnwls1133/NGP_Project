#pragma once
#include "CObject.h"
class CPlayer;
class CItem :
    public CObject
{
private:
    bool      m_bHit;
    float     m_dAcc;
    float     m_fTheta; // 위아래 방향 개념
    float     m_fVec;
    int       m_iType;
    Vec2      m_vDir;
    bool      m_bCollected = false;
    vector<CPlayer*> m_TouchPlayers;
public:
    void InitAfterPlacement();
    void ApplySpawnState(const protocol::SpawnPayload& spawn);
    void ResolveSimultaneousPickup();

    protocol::SpawnPayload ExportSpawnPayload(protocol::EventId eventId) const override;
    virtual void update();
    virtual void render(HDC _dc);
public:
    virtual void OnCollisionEnter(CCollider* _pOther);
    virtual void OnCollision(CCollider* _pOther);
    virtual void OnCollisionExit(CCollider* _pOther);
    CLONE(CItem);

public:
    CItem();
    ~CItem();
};

