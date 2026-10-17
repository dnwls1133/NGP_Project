#pragma once

class AI;
class CMonster;

class CState
{
private:
    AI*       m_pAI;
    MON_STATE m_eState;

public:
    float     m_fAdt;
    float     m_fSTimeAcc;
public:
    AI* GetAi() { return m_pAI; }
    MON_STATE GetType() { return m_eState; }
    CMonster* GetMonster();
public:

    virtual void Enter() = 0;
    virtual void Exit() = 0;
    virtual void update() = 0;
public:
    CState(MON_STATE _eState);
    virtual ~CState();

    friend class AI;
};

