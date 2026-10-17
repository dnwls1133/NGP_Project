#pragma once
#include "CObject.h"
class CCollider;
class CTexture;
class CMissile :
    public CObject
{
private:
    CTexture* m_pTex;
    bool      m_bHit;
    float     m_dAcc;
    float     m_fTheta; // 위아래 방향 개념
    float     m_fVec;
    int       m_iType;
    Vec2      m_vDir;
public:
    void SetDir(Vec2 _vDir)
    {
        m_vDir = _vDir;
        m_vDir.Normalize();
    }
    void SetType(int _iType) { m_iType = _iType; }
    void SetVec(int _iVec) { m_fVec = (float)_iVec; }
public:
    virtual void update();
    virtual void render(HDC _dc);
    void init(GROUP_TYPE _eType);
    void ChangemType(MISSILE_TYPE _eType);
public:
    virtual void OnCollisionEnter(CCollider* _pOther);
    virtual void OnCollision(CCollider* _pOther);
    virtual void OnCollisionExit(CCollider* _pOther);
    CLONE(CMissile);
public:
    CMissile();
    ~CMissile();



};


