#include "pch.h"
#include "CTitleLogo.h"

#include "CTexture.h"
#include "CResMgr.h"

CTitleLogo::CTitleLogo()
    : CUI(false)
{
    m_pTexture = CResMgr::GetInst()->LoadTexture(L"Logo_Title", L"texture\\Sprite_Logo_Title.png");
}

CTitleLogo::~CTitleLogo()
{

}

void CTitleLogo::render(HDC canvas_)
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
