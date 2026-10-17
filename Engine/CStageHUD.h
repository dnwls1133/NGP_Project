#pragma once

#include "CUI.h"

class CStageHUD
    : public CUI
{
    CLONE(CStageHUD)
public:
    CStageHUD();
    virtual ~CStageHUD() override;

    virtual void update();
    virtual void render(HDC _dc);

    inline bool IsActive() const
    {
        return m_bIsActive;
    }

    inline void SetActive(bool isActive_)
    {
        m_bIsActive = isActive_;
    }

private:
    HFONT m_hLabelFont;
    HFONT m_hValueFont;

    CTexture* m_pHeartFull;     // 꽉 찬 하트.
    CTexture* m_pHeartEmpty;    // 빈 하트.

    bool m_bIsActive;

};

