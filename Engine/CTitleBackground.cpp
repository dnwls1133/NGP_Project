#include "pch.h"
#include "CTitleBackground.h"

#include "CCore.h"

#include "CTexture.h"
#include "CObject.h"
#include "CResMgr.h"
#include "CTimeMgr.h"

CTitleBackground::CTitleBackground()
{
    const std::wstring textureKey = L"Introduction No.0";
    const std::wstring filePath = L"texture\\Opening\\Sprite_Background_Opening_0.png";
    m_pTexture = CResMgr::GetInst()->LoadTexture(textureKey, filePath);
}

CTitleBackground::~CTitleBackground()
{

}

void CTitleBackground::update()
{
    if (!m_bIsPlaying)
        return;

    m_dAnimDeltaTime += CTimeMgr::GetInst()->GetfDT();

    if (m_dAnimDeltaTime >= m_dAnimTime)
    {
        m_dAnimDeltaTime = 0.0f;

        if (m_bIsIntroState)
        {
            if (++m_currentIndex >= IDX_INTRO_END)
            {
                m_bIsIntroState = false;
                m_currentIndex = IDX_INTRO_END;
                // return
            }

            const std::wstring textureKey = std::format(L"Introduction No.{}", m_currentIndex);
            const std::wstring filePath = std::format(L"texture\\Opening\\Sprite_Background_Opening_{}.png", m_currentIndex);
            m_pTexture = CResMgr::GetInst()->LoadTexture(textureKey, filePath);
        }
        else
        {
            if (++m_currentIndex >= IDX_TITLE_END)
            {
                m_currentIndex = IDX_TITLE_START;
            }

            const std::wstring textureKey = std::format(L"Main Title No.{}", m_currentIndex);
            const std::wstring filePath = std::format(L"texture\\Main Title\\Sprite_Background_MainTitle_{}.png", m_currentIndex);
            m_pTexture = CResMgr::GetInst()->LoadTexture(textureKey, filePath);
        }
    }
}

void CTitleBackground::render(HDC canvas_)
{
    if (!m_pTexture)
        return;

    const Vec2 position = GetPos();   // 중심 위치
    const Vec2 scale = GetScale();    // 출력 크기 (픽셀 단위)

    const int width = m_pTexture->Width();
    const int height = m_pTexture->Height();

    const int drawWidth = static_cast<int>(scale.x);
    const int drawHeight = static_cast<int>(scale.y);

    const int drawX = static_cast<int>(position.x - drawWidth / 2.0f);
    const int drawY = static_cast<int>(position.y - drawHeight / 2.0f);

    // 투명색 지정 (예: RGB(255, 0, 255) - 보라색)
    COLORREF transparentColor = RGB(255, 0, 255);

    TransparentBlt(
        canvas_,
        drawX, drawY,
        drawWidth, drawHeight,              // 출력 크기
        m_pTexture->GetDC(),
        0, 0,
        width, height,                      // 원본 크기
        transparentColor
    );
}

void CTitleBackground::PlayIntroAnimation() noexcept
{
    m_bIsIntroState = true;
    m_bIsPlaying = true;
    m_currentIndex = IDX_INTRO_START;
}

void CTitleBackground::PlayTitleAnimation() noexcept
{
    m_bIsIntroState = false;
    m_bIsPlaying = true;
    m_currentIndex = IDX_TITLE_START;
}

bool CTitleBackground::IsPlayIntroAnimation() const noexcept
{
    return m_bIsIntroState;
}
