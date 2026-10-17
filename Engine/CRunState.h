#pragma once
#include "CState.h"
class CRunState :
    public CState
{
private:

public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();
public:
    CRunState();
    ~CRunState();
};

