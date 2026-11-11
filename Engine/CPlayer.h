#pragma once
#include "CObject.h"
#include "../shared/protocol/ProtocolTypes.h"
class CTexture;
class CSound;
class CPlayer :
    public CObject
{

private:
  //  CTexture*   m_pTex;
    double      dAcc;
    double      dStartAcc;
    int         m_iHp;
    bool        m_bHit;
    int         m_iAtk;
    int         m_iLvl;
    int         m_iBoomb;
    bool        m_clear;
    bool        m_dead;
    PlayerId    m_PlayerId = 0;

    CSound* m_pFireSE;
    CSound* m_pLvUpSE;
    CSound* m_pDeadSE;

public:
    virtual void update();
    void Simulate(std::uint8_t moveBits, std::uint8_t fireHeld, float dt);
    void SetPlayerId(PlayerId id) { m_PlayerId = id; }
    PlayerId GetPlayerId() { return m_PlayerId; }
    protocol::ObjectStateRecord ExportStateRecord(protocol::ObjectId objectId) const;

    virtual void render(HDC _dc);

    int getHP() { return m_iHp; }
    int getLv() { return m_iLvl; }


    void FullHP() { m_iHp = 3; }
    void setclear() { m_clear = true; }
    void setdead() { m_dead = true; }
    int GetPlayerAtk() { return m_iAtk; }

    bool isMujeok = false;
private:
    void CreateMissile(int type,float _fVec, MISSILE_TYPE _etype);

    virtual void OnCollisionEnter(CCollider* _pOther);

    CLONE(CPlayer);
public:
    CPlayer();
    ~CPlayer();
};



