#pragma once
#include "CState.h"
class CPatorl4State :
    public CState
{
private:

public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();

public:
    CPatorl4State();
    ~CPatorl4State();
};

