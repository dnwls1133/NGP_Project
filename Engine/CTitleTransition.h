#pragma once

#include "CTransitionBase.h"

#include <cstddef>

class CTexture;

class CTitleTransition final : public CTransitionBase
{
public:
    void Open();
    void OpenComplete() override;
    void Close();
    void CloseComplete() override;

    void Update() override;
    void Render(HDC dc) override;
    void RenderOverlay(HDC dc);

private:
    void LoadOpeningFrame();
    void LoadTitleFrame();
    bool AdvanceFrame();

    static constexpr std::size_t OpeningLastFrame = 70;
    static constexpr std::size_t TitleFrameCount = 25;
    static constexpr float FrameDuration = 0.05f;
    static constexpr float FadeDuration = 1.f;

    CTexture* m_texture = nullptr;
    CTexture* m_whiteTexture = nullptr;
    std::size_t m_currentFrame = 0;
    float m_frameElapsed = 0.f;
    float m_alpha = 0.f;
};
