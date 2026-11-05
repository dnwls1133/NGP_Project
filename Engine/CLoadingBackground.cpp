#include "pch.h"
#include "CLoadingBackground.h"

#include "CCore.h"
#include "CResMgr.h"
#include "CTimeMgr.h"
#include "CTexture.h"

CLoadingBackground::CLoadingBackground()
{
    LPCTSTR textureKey = TEXT("Loading Background No.0");
    LPCTSTR filePath = TEXT("texture\\Loading\\a0000.jpg");
    m_pBackground = CResMgr::GetInst()->LoadTexture(textureKey, filePath);
}

void CLoadingBackground::Open()
{
    m_currentIndex = IDX_START;
    m_dAnimDeltaTime = 0.0f;
    m_alpha = 0.0f;
    m_pBackground = CResMgr::GetInst()->LoadTexture(L"Loading Background No.0", L"texture\\Loading\\a0000.jpg");
    m_isActive = true;
}

void CLoadingBackground::Update()
{
    if (!m_isActive || !m_pBackground)
        return;

    m_dAnimDeltaTime += CTimeMgr::GetInst()->GetfDT();

    if (m_dAnimDeltaTime >= ANIM_TIME)
    {
        m_dAnimDeltaTime -= ANIM_TIME;
        if (++m_currentIndex >= IDX_END)
        {
            m_currentIndex = IDX_START;
        }

        const std::wstring textureKey = std::format(L"Loading Background No.{}", m_currentIndex);
        const std::wstring filePath = std::format(L"texture\\Loading\\a{:04}.jpg", m_currentIndex);
        m_pBackground = CResMgr::GetInst()->LoadTexture(textureKey, filePath);
    }
}

void CLoadingBackground::Render(HDC canvas_)
{
    if (!m_isActive || !m_pBackground)
        return;

    const POINT resolution = CCore::GetInst()->GetResolution();

    const int width = m_pBackground->Width();
    const int height = m_pBackground->Height();

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(m_alpha);

    AlphaBlend(
        canvas_,
        0, 0,
        resolution.x, resolution.y,
        m_pBackground->GetDC(),
        0, 0,
        width, height,                      // 원본 크기
        blend
    );
}

