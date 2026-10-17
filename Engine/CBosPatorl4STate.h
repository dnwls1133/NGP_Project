#pragma once
#include "CState.h"
class CBosPatorl4STate :
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
    CBosPatorl4STate();
    ~CBosPatorl4STate();
};

