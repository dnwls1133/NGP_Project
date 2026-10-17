#pragma once

#include "CScene.h"
class CScene;

class CTitleLogo;
class CTitleBackground;
// class CTitleMessage;

class CSound;

class CBtnUI;

class CScene_Title :
    public CScene
{
public:
    CScene_Title();
    virtual ~CScene_Title() override;

    virtual void Enter() override;
    virtual void update() override;
    virtual void Exit() override;

private:
    // void mChangeToIntro();
    // void mChangeToTitle();

    CTitleBackground* m_pBackground;    // 배경

    CSound* m_pTitleTheme;

    enum class EState
    {
        None,
        Intro,
        Title,
    };

    EState m_CurrentState;
};
