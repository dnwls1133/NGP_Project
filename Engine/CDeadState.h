#pragma once
#include "CState.h"
class CDeadState :
    public CState
{
private:
    bool m_bEntered = false;    // 사망 진입 1회 확정
    bool m_bExited = false;     // 사망 종료 1회 확정

public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();

public:
    CDeadState();
    ~CDeadState();
};

