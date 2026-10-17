#pragma once
#include "CObject.h"
class CPlayerDead :
    public CObject
{

public:
    float   m_fAcc;

public:
    virtual void update();
    virtual void render(HDC _dc);
private:
    CLONE(CPlayerDead);
public:
    CPlayerDead();
    ~CPlayerDead();
};

