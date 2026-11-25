#pragma once

#include "CScene.h"
#include "CMenuTransition.h"
#include "CTitleTransition.h"
class CScene;

class CTitleLogo;
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
    void render(HDC dc) override;
    void Exit() override;

private:
    enum class EState : unsigned char
    {
        None,
        Intro,
        Title,
    };

    void CreateMenus();
    void ShowTitle();
    void OpenMenu(CTitleMenu& menu);
    bool UpdateMenuOpen();

    CTitleTransition m_titleTransition;
    CMenuTransition m_menuTransition;
    std::vector<Vec2> m_menuTargetPositions;
    float m_menuOpenElapsed = 0.f;

    EState m_CurrentState;

    CSound* m_pTitleTheme;

    CTitleMenu* m_pFirstMenu = nullptr;
    CTitleMenu* m_pSecondMenu = nullptr;
    bool m_menuSwitchRequested = false;
};
