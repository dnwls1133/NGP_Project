#pragma once

#include "CObject.h"

class CObject;
class CTexture;

/**
 * @brief 타이틀 Scene 내 재생되는 백그라운드 오브젝트에 대해 정의합니다.
 */
class CTitleBackground :
    public CObject
{
public:
    CTitleBackground();
    virtual ~CTitleBackground() override;

    virtual void update() override;
    virtual void render(HDC dc) override;

    void PlayIntroAnimation() noexcept; // 인트로 애니메이션 재생.
    void PlayTitleAnimation() noexcept; // 타이틀 애니메이션 재생.

    bool IsPlayIntroAnimation() const noexcept; // 인트로 애니메이션 재생 여부 확인.

    CLONE(CTitleBackground)

private:
    /**
     * @brief 인트로 시작 지점.
     */
    static constexpr size_t IDX_INTRO_START = 0;

    /**
     * @brief 인트로 종료 지점.
     */
    static constexpr size_t IDX_INTRO_END = 70;

    /**
     * @brief 타이틀 시작 지점.
     */
    static constexpr size_t IDX_TITLE_START = 0;

    /**
     * @brief 타이틀 종료 지점.
     */
    static constexpr size_t IDX_TITLE_END = 25;

    /**
     * @brief 텍스쳐.
     */
    CTexture* m_pTexture = nullptr;

    /**
     * @brief 현재 지점.
     */
    size_t m_currentIndex = 0;

    float m_dAnimDeltaTime = 0.0f;

    static constexpr float m_dAnimTime = 0.05f;

    bool m_bIsPlaying;
    bool m_bIsIntroState;

};
