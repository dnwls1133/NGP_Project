#pragma once
#include "CState.h"
class CBossPatorl2STate :
    public CState
{
private:
    Vec2 m_pDesPos[5];
    float m_fXdir;
    float m_fFlip;
    float m_fMTimeAcc;
    int   m_iDestType;
public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();

public:
    CBossPatorl2STate();
    ~CBossPatorl2STate();
};

