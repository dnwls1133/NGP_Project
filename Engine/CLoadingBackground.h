#pragma once

#include <algorithm>

#include "CObject.h"

class CTexture;

class CLoadingBackground
{
public:
    CLoadingBackground();
    ~CLoadingBackground() = default;

    void Open();
    void Close()
    {
        m_currentIndex = 0;
        m_dAnimDeltaTime = 0;
        m_isActive = false;
    }

    void Update();
    void Render(HDC canvas);

    bool IsActive() const { return m_isActive; }
    float GetAlpha() const { return m_alpha; }
    void SetAlpha(float alpha) { m_alpha = std::clamp(alpha, 0.0f, 255.0f); }

private:
    static constexpr size_t IDX_START = 0;
    static constexpr size_t IDX_END = 11;

    static constexpr float ANIM_TIME = 0.05f;

    bool m_isActive = false;

    CTexture* m_pBackground = nullptr;
    float m_alpha = 0.0f;

    size_t m_currentIndex = 0;
    float m_dAnimDeltaTime = 0.0f;
};
