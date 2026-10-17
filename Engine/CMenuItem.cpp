#include "pch.h"
#include "CMenuItem.h"

#include "CTexture.h"

#include "CSound.h"
#include "CSoundMgr.h"

#include "CResMgr.h"

// CMenuItem(CTexture* const idleTex_, CTexture* const hoverTex_)
//     : m_pTexture(idleTex_)
//     , m_pIdleTexture(idleTex_)
//     , m_pHoverTexture(hoverTex_)
// {
//     // SetScale();
//     // m_pHoverSound = 
// }

CMenuItem::CMenuItem(CTexture* const idleTex_, CTexture* const hoverTex_)
    : m_pTexture(idleTex_)
    , m_pIdleTexture(idleTex_)
    , m_pHoverTexture(hoverTex_)
{
    m_pHoverSound = CResMgr::GetInst()->LoadSound(L"Button Hover", L"sound\\SE\\SE_MenuSelect.mp3");
    m_pClickSound = CResMgr::GetInst()->LoadSound(L"Button Hover", L"sound\\SE\\SE_MenuClick.mp3");
}

void CMenuItem::MouseOn()
{
    if (m_bHoverFlag)
        return;

    if (m_pHoverTexture)
        m_pTexture = m_pHoverTexture;

    const float curScaleX = GetScale().x * 1.05f;
    const float curScaleY = GetScale().y * 1.05f;

    SetScale(Vec2(curScaleX, curScaleY));

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

    const float curScaleX = GetScale().x / 1.05f;
    const float curScaleY = GetScale().y / 1.05f;
    SetScale(Vec2(curScaleX, curScaleY));

    m_bHoverFlag = false;
}

void CMenuItem::MouseLbtnUP()
{
    if (m_bClickedFlag)
        return;

    if (m_pHoverTexture)
        m_pTexture = m_pHoverTexture;

    const float curScaleX = GetScale().x * 1.1f;
    const float curScaleY = GetScale().y * 1.1f;
    SetScale(Vec2(curScaleX, curScaleY));

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

    const float curScaleX = GetScale().x / 1.1f;
    const float curScaleY = GetScale().y / 1.1f;
    SetScale(Vec2(curScaleX, curScaleY));

    m_bClickedFlag = false;
}

void CMenuItem::render(HDC canvas_)
{
    if (!m_pTexture)
        return;

    const Vec2 position = GetFinalPos();  // 중심 위치
    const Vec2 scale = GetScale();     // 출력될 크기

    const int width = static_cast<int>(m_pTexture->Width());   // 원본 너비
    const int height = static_cast<int>(m_pTexture->Height());  // 원본 높이

    const int drawX = static_cast<int>(position.x - (scale.x - width) / 2.0f);
    const int drawY = static_cast<int>(position.y - (scale.y - height) / 2.0f);

    // 투명하게 처리할 색상 (예: RGB(255, 0, 255) → 마젠타)
    COLORREF transparentColor = RGB(255, 0, 255);

    // TransparentBlt 사용
    TransparentBlt(
        canvas_,             // 대상 DC
        drawX, drawY,        // 출력 위치 (좌상단)
        scale.x, scale.y,    // 출력 크기 (확대/축소 적용)
        m_pTexture->GetDC(), // 원본 DC
        0, 0,                // 원본 좌상단
        width, height,       // 원본 크기
        transparentColor     // 투명 색상
    );

}
