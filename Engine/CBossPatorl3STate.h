#pragma once
#include "CState.h"
class CBossPatorl3STate :
    public CState
{
private:
    Vec2 m_pDesPos[5];
    float m_fXdir;
    float m_fRXdir;
    float m_fYdir;
    float m_fXFlip;
    float m_fYFlip;
    float m_fMTimeAcc;
    int   m_iDestType;
public:
    virtual void update();
    virtual void Enter();
    virtual void Exit();

public:
    CBossPatorl3STate();
    ~CBossPatorl3STate();
};

