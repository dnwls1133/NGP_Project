#pragma once
#include "CObject.h"
class CExposion :
    public CObject
{
public:
    float   m_fAcc;

public:
    virtual void update();
    virtual void render(HDC _dc);

private:
    CLONE(CExposion);
public:
    CExposion();
    ~CExposion();
};

