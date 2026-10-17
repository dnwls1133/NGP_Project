#pragma once

#include "CScene.h"

class CTexture;
class CSound;
class CMenuItem;

class CScene_Credits
    : public CScene
{
public:
    CScene_Credits();
    virtual ~CScene_Credits() override;

    virtual void Enter() override;
    virtual void render(HDC canvas_) override;
    virtual void Exit() override;

private:
    CTexture* m_pBackground;
    CTexture* m_pPanel;
    CSound* m_pTheme;
    CMenuItem* m_pBackButton;

};
