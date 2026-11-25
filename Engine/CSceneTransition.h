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
    void Open();
    void OpenComplete() override;
    void Close();
    void CloseComplete() override;

    static constexpr float FadeDuration = 0.5f;

    CLoadingBackground m_background;
};
