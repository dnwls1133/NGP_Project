#pragma once
#include "CObject.h"
class CPlayerDead :
    public CObject
{

public:
    float   m_fAcc;

public:
    virtual void update();
    virtual void render(HDC _dc);
    protocol::SpawnPayload ExportSpawnPayload(protocol::EventId eventId) const override;
private:
    CLONE(CPlayerDead);
public:
    CPlayerDead();
    ~CPlayerDead();
};

