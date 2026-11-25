#include "pch.h"
#include "CMenuItem.h"

#include "CTexture.h"

#include "CSound.h"
#include "CSoundMgr.h"

#include "CCore.h"
#include "CResMgr.h"

#include <algorithm>

namespace
{
    constexpr float HoverScale = 1.05f;
    constexpr float ClickScale = 1.1f;
}

// CMenuItem(CTexture* const idleTex_, CTexture* const hoverTex_)
//     : m_pTexture(idleTex_)
//     , m_pIdleTexture(idleTex_)
//     , m_pHoverTexture(hoverTex_)
// {
//     // SetScale();
//     // m_pHoverSound = 
// }

CMenuItem::CMenuItem(CTexture* const idleTex_, CTexture* const hoverTex_, CTexture* cursorTexture)
    : m_pTexture(idleTex_)
    , m_pIdleTexture(idleTex_)
    , m_pHoverTexture(hoverTex_)
    , m_pCursorTexture(cursorTexture)
{
    m_pHoverSound = CResMgr::GetInst()->LoadSound(L"Button Hover", L"sound\\SE\\SE_MenuSelect.mp3");
    m_pClickSound = CResMgr::GetInst()->LoadSound(L"Button Hover", L"sound\\SE\\SE_MenuClick.mp3");
}

void CMenuItem::ResetMouseState()
{
    if (m_bClickedFlag)
    {
        MouseLbtnDown();
    }
    MouseOff();
    CUI::ResetMouseState();
}

void CMenuItem::MouseOn()
{
    if (m_bHoverFlag)
        return;

    if (m_pHoverTexture)
        m_pTexture = m_pHoverTexture;

    if (m_pHoverSound)
        CSoundMgr::GetInst()->PlaySE(m_pHoverSound);

    m_bHoverFlag = true;
}

void CMenuItem::MouseOff()
{
    if (!m_bHoverFlag)
        return;

    if (m_pIdleTexture)
        m_pTexture = m_pIdleTexture;

    m_bHoverFlag = false;
}

void CMenuItem::MouseLbtnUP()
{
    if (m_bClickedFlag)
        return;

    if (m_pHoverTexture)
        m_pTexture = m_pHoverTexture;

    if (m_pClickSound)
        CSoundMgr::GetInst()->PlaySE(m_pClickSound);

    m_bClickedFlag = true;
}

void CMenuItem::MouseLbtnDown()
{
    if (!m_bClickedFlag)
        return;

    if (m_pHoverTexture)
        m_pTexture = m_pHoverTexture;

    m_bClickedFlag = false;
}

void CMenuItem::AdjustBounds(Vec2& position, Vec2& size) const
{
    const float factor = (m_bHoverFlag ? HoverScale : 1.f) * (m_bClickedFlag ? ClickScale : 1.f);
    const Vec2 scaledSize(size.x * factor, size.y * factor);
    position.x += (size.x - scaledSize.x) * 0.5f;
    position.y += (size.y - scaledSize.y) * 0.5f;
    size = scaledSize;
}

void CMenuItem::render(HDC canvas_)
{
    if (!m_pTexture)
        return;

    Vec2 position = GetFinalPos();
    Vec2 size = GetScale();
    AdjustBounds(position, size);

    const int width = static_cast<int>(m_pTexture->Width());   // 원본 너비
    const int height = static_cast<int>(m_pTexture->Height());  // 원본 높이

    const int drawX = static_cast<int>(position.x);
    const int drawY = static_cast<int>(position.y);
    const int drawWidth = static_cast<int>(size.x);
    const int drawHeight = static_cast<int>(size.y);

    // 투명하게 처리할 색상 (예: RGB(255, 0, 255) → 마젠타)
    COLORREF transparentColor = RGB(255, 0, 255);

    // TransparentBlt 사용
    TransparentBlt(
        canvas_,             // 대상 DC
        drawX, drawY,        // 출력 위치 (좌상단)
        drawWidth, drawHeight, // 출력 크기 (확대/축소 적용)
        m_pTexture->GetDC(), // 원본 DC
        0, 0,                // 원본 좌상단
        width, height,       // 원본 크기
        transparentColor     // 투명 색상
    );

    if (!m_bHoverFlag)
        return;

    CTexture* cursor = m_pCursorTexture;
    if (!cursor)
        return;

    constexpr int cursorGap = 8;
    const int sourceWidth = static_cast<int>(cursor->Width());
    const int sourceHeight = static_cast<int>(cursor->Height());
    const int rightCursorX = drawX + drawWidth + cursorGap;
    const int screenWidth = static_cast<int>(CCore::GetInst()->GetResolution().x);
    const float cursorScale = m_bClickedFlag ? ClickScale : 1.f;
    const int scaledCursorWidth = static_cast<int>(sourceWidth * cursorScale);
    const int cursorWidth = (std::min)({scaledCursorWidth, drawX - cursorGap, screenWidth - rightCursorX});
    if (cursorWidth <= 0)
        return;

    const int cursorHeight = sourceHeight * cursorWidth / sourceWidth;
    const int cursorY = drawY + (drawHeight - cursorHeight) / 2;
    const int leftCursorX = drawX - cursorGap - cursorWidth;

    for (const int cursorX : {leftCursorX, rightCursorX})
    {
        TransparentBlt(canvas_, cursorX, cursorY, cursorWidth, cursorHeight,
            cursor->GetDC(), 0, 0, sourceWidth, sourceHeight, transparentColor);
    }
}
