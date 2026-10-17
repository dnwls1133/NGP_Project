#pragma once
#include "CObject.h"


struct tMonInfo
{
    float           fSpeed;         // 속도
    float           fHP;            // 체력
    float           fRecogRange;    // 인지 범위
    float           fAttRange;      // 공격 범위
    float           fAtt;           // 공격력
    Vec2            vDestPos;        // 시작 지점
    Vec2            vExitPos;        // 탈춤 지점
    MISSILE_PTRN    ePattern;        // 공격 패턴
    MON_TYPE        eMType;          // 몬스터 타입
};






class AI;
class CTexture;
class CSound;

class CMonster :
    public CObject
{
protected:
   
    tMonInfo    m_tInfo;
    double      dAccTime0;
    double      dAccTime1;
    AI*         m_pAI;
    bool        m_signaldead;
    bool        m_bHit;

    CSound* m_pHurtSE;
    CSound* m_pDeadSE;

public:
    virtual void update();
    virtual void render(HDC _dc);
public:
    virtual void CreateMissile(int type);
    

public:
    virtual void OnCollisionEnter(CCollider* _pOther);
    
public:
    void SetSpeed(float _f) { m_tInfo.fSpeed = _f;}
    void SetDestPos(Vec2 _vPos) { m_tInfo.vDestPos = _vPos; }
    float GetSpeed() { return m_tInfo.fSpeed; }
    void SetAI(AI* _AI);
    const tMonInfo& GetInfo() { return m_tInfo; }
    void SetAnim(MON_TYPE eType);
    bool GetsignalDead() { return m_signaldead; }

    void SetMissiletype(MISSILE_PTRN eType)
    {
        m_tInfo.ePattern = eType;
    }
private:
    void SetMonInfo(const tMonInfo& _info)
    {
        m_tInfo = _info;
    }
   

public:

    CLONE(CMonster);
public:
    CMonster();
    virtual ~CMonster();

    friend class CMonFactory;
   
};

