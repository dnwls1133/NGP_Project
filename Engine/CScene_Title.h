#pragma once

#include "CScene.h"
class CScene;

class CTitleLogo;
class CTitleBackground;
// class CTitleMessage;

class CSound;

class CBtnUI;
class CMenuItem;
class CTitleMenu;

class CScene_Title :
    public CScene
{
public:
    CScene_Title() = default;
    ~CScene_Title() override = default;

    void Enter() override;
    void update() override;
    void Exit() override;

private:
    enum class EState : unsigned char
    {
        None,
        Intro,
        Title,
    };

    void CreateMenus();

    EState m_CurrentState;

    CTitleBackground* m_pBackground;

    CSound* m_pTitleTheme;

    CTitleMenu* m_pFirstMenu = nullptr;
    CTitleMenu* m_pSecondMenu = nullptr;
};
