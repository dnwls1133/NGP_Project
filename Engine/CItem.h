#pragma once
#include "CObject.h"
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
public:
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

