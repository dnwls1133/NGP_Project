#pragma once
#include "CState.h"
class CPatorl3State :
    public CState
{
private:
    float m_fXdir;
    float m_fFlip;
    float m_fMTimeAcc;
public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();

public:
    CPatorl3State();
    ~CPatorl3State();

};

