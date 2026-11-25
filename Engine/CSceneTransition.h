#pragma once

#include "CTransitionBase.h"
#include "CLoadingBackground.h"

class CSceneTransition : public CTransitionBase
{
public:
    void Start(TransitionCallback switchScene);
    void Update() override;
    void Render(HDC dc) override;

private:
    bool Fade(float direction, float targetAlpha);

    CLoadingBackground m_background;
};
