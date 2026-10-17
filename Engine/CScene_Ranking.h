#pragma once

#include "CScene.h"

class CTexture;
class CSound;
class CMenuItem;

class CScene_Ranking
    : public CScene
{
public:
    CScene_Ranking();
    virtual ~CScene_Ranking() override;

    virtual void Enter() override;
    virtual void render(HDC canvas_) override;
    virtual void Exit() override;

private:
    CTexture* m_pBackground;
    CTexture* m_pPanel;
    CSound* m_pTheme;

    HFONT m_hScoreFont;         // 점수용
    HFONT m_hDateFont;          // 날짜용
};

