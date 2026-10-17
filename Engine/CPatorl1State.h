#pragma once
#include "CState.h"
class CPatorl1State :
    public CState
{
private:

public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();

public:
    CPatorl1State();
    ~CPatorl1State();
};

