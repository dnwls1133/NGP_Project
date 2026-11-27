#pragma once

#include "CTransitionBase.h"

#include <cstddef>

class CTexture;

class CLoadingTransition final : public CTransitionBase
{
public:
    CLoadingTransition();

    void Open();
    void OpenComplete() override;
    void Close();
    void CloseComplete() override;

    void Update() override;
    void Render(HDC dc) override;

private:
    void AdvenceFrame();

    static constexpr std::size_t FrameCount = 11;
    static constexpr float FrameDuration = 0.05f;
    static constexpr float FadeDuration = 0.5f;

    CTexture* m_texture = nullptr;
    std::size_t m_currentFrame = 0;
    float m_frameElapsed = 0.f;
    float m_alpha = 0.f;
};
