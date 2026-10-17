#pragma once

#include "CBtnUI.h"

class CTexture;
class CSound;

class CMenuItem
    : public CBtnUI
{
    friend class CBtnUI;
public:
    CMenuItem(CTexture* const idleTex_, CTexture* const hoverTex_);

    virtual void MouseOn() override;
    virtual void MouseOff() override;
    virtual void MouseLbtnDown() override;
    virtual void MouseLbtnUP() override;
    // virtual void MouseLbtnClicked() override;

    virtual void render(HDC canvas_) override;

private:
    bool m_bHoverFlag = false;
    bool m_bClickedFlag = false;

    CTexture* m_pTexture;               // 현재 사용중인 텍스쳐.
    CTexture* const m_pIdleTexture;     // 일반 텍스쳐.
    CTexture* const m_pHoverTexture;    // 호버 텍스쳐.

    CSound* m_pHoverSound;
    CSound* m_pClickSound;
};

