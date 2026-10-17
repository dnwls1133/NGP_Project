#pragma once
#include "CState.h"
class CDeadState :
    public CState
{
private:

public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();

public:
    CDeadState();
    ~CDeadState();
};

