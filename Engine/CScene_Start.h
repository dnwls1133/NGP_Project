#pragma once
#include "CScene.h"
class CScene_Start :
    public CScene
{
public:
    virtual void update();
    virtual void Enter();
    virtual void render(HDC _dc);
    virtual void Exit();
public:
    CScene_Start();
    ~CScene_Start();
};

