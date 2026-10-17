#include "pch.h"
#include "CStageCard.h"

#include "CCore.h"

#include "CTexture.h"
#include "CObject.h"
#include "CResMgr.h"
#include "CTimeMgr.h"

CStageCard::CStageCard(bool isDestroy_, const std::wstring& cardName_)
    : m_bIsDestroy(isDestroy_)
    , m_strCardName(cardName_)
{
    auto textureKey = std::format(L"{} No.0", m_strCardName);
    auto initialPath = std::format(L"texture\\Stage Cards\\{}\\slice_01.png", m_strCardName); 
    m_pTexture = CResMgr::GetInst()->LoadTexture(textureKey, initialPath);
    m_currentIndex = 0;
}

CStageCard::~CStageCard()
{

}

void CStageCard::update()
{
    // 업데이트 다 돌면 멈추기.
    if (m_currentIndex >= m_MAX_INDEX)
        return;

    m_dAnimDeltaTime += CTimeMgr::GetInst()->GetfDT();

    if (m_dAnimDeltaTime >= m_dAnimTime)
    {
        m_dAnimDeltaTime = 0.0f;

        if (++m_currentIndex >= m_MAX_INDEX)
        {
            m_currentIndex = m_MAX_INDEX;
            // return
        }

        const std::wstring textureKey = std::format(L"{} No.{}", m_strCardName, m_currentIndex);
        const std::wstring filePath = std::format(L"texture\\Stage Cards\\{}\\slice_{:02}.png", m_strCardName, m_currentIndex);
        m_pTexture = CResMgr::GetInst()->LoadTexture(textureKey, filePath);
    }
}

void CStageCard::render(HDC canvas_)
{
    if (!m_pTexture)
        return;

    if (m_bIsDestroy && m_currentIndex >= m_MAX_INDEX)
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
