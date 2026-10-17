#pragma once

#include "CObject.h"

class CTexture;

class CStageCard
    : public CObject
{
public:
    CStageCard(bool isDestroy_, const std::wstring& cardName_);
    virtual ~CStageCard() override;

    virtual void update() override;
    virtual void render(HDC canvas_) override;

    CLONE(CStageCard)
private:
    /**
     * @brief 텍스쳐.
     */
    CTexture* m_pTexture;

    /**
     * @brief 현재 지점.
     */
    size_t m_currentIndex = 0;

    /**
     * @brief 최대 지점.
     */
    static constexpr size_t m_MAX_INDEX = 15;

    /**
     * @brief 최소 지점.
     */
    static constexpr size_t m_MIN_INDEX = 0;

    /**
     * @brief 카드 이름.
     */
    std::wstring m_strCardName;

    /**
     * @brief 재생 후 파괴 여부.
     */
    bool m_bIsDestroy;

    float m_dAnimDeltaTime = 0.0f;

    static constexpr float m_dAnimTime = 0.05f;

};

