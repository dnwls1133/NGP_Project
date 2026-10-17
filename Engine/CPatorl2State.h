#pragma once
#include "CState.h"
class CPatorl2State :
    public CState
{
private:

public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();
public:
    CPatorl2State();
    ~CPatorl2State();
};

