#pragma once

#include "CScene.h"
#include "CMenuTransition.h"
class CScene;

class CTitleLogo;
class CTitleBackground;
// class CTitleMessage;

class CSound;
class CTexture;

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
    void OpenMenu(CTitleMenu& menu);
    bool UpdateMenuOpen();

    class CTitleRevealTransition final : public CTransitionBase
    {
    public:
        void Start();
        void Render(HDC dc) override;

    private:
        CTexture* m_whiteTexture = nullptr;
        float m_alpha = 0.f;
    };

    CTitleRevealTransition m_titleReveal;
    CMenuTransition m_menuTransition;
    std::vector<Vec2> m_menuTargetPositions;
    float m_menuOpenElapsed = 0.f;

    EState m_CurrentState;

    CTitleBackground* m_pBackground;

    CSound* m_pTitleTheme;

    CTitleMenu* m_pFirstMenu = nullptr;
    CTitleMenu* m_pSecondMenu = nullptr;
    bool m_menuSwitchRequested = false;
};
