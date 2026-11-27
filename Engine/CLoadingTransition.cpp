#include "pch.h"
#include "CLoadingTransition.h"

#include "CCore.h"
#include "CResMgr.h"
#include "CTexture.h"
#include "CTimeMgr.h"

#include <algorithm>
#include <format>

CLoadingTransition::CLoadingTransition()
{
    AdvenceFrame();
}

void CLoadingTransition::Open()
{
    if (GetCurrentState() != ETransitionState::Idle &&
        GetCurrentState() != ETransitionState::CloseComplete)
    {
        return;
    }

    m_currentFrame = 0;
    m_frameElapsed = 0.f;
    m_alpha = 0.f;
    AdvenceFrame();

    CTransitionBase::Open([this]
    {
        const float delta = CTimeMgr::GetInst()->GetfDT() / FadeDuration;
        m_alpha = std::clamp(m_alpha + delta, 0.f, 1.f);
        return m_alpha >= 1.f;
    });
}

void CLoadingTransition::OpenComplete()
{
    if (GetCurrentState() != ETransitionState::Open)
    {
        return;
    }

    m_alpha = 1.f;
    CTransitionBase::OpenComplete();
    Close();
}

void CLoadingTransition::Close()
{
    CTransitionBase::Close([this]
    {
        const float delta = CTimeMgr::GetInst()->GetfDT() / FadeDuration;
        m_alpha = std::clamp(m_alpha - delta, 0.f, 1.f);
        return m_alpha <= 0.f;
    });
}

void CLoadingTransition::CloseComplete()
{
    if (GetCurrentState() != ETransitionState::Close)
    {
        return;
    }

    m_alpha = 0.f;
    CTransitionBase::CloseComplete();
}

void CLoadingTransition::AdvenceFrame()
{
    const auto key = std::format(L"Loading Background No.{}", m_currentFrame);
    const auto path = std::format(L"texture\\Loading\\a{:04}.jpg", m_currentFrame);
    m_texture = CResMgr::GetInst()->LoadTexture(key, path);
}

void CLoadingTransition::Update()
{
    CTransitionBase::Update();

    if (!IsPlaying() || !m_texture)
    {
        return;
    }

    m_frameElapsed += CTimeMgr::GetInst()->GetfDT();
    if (m_frameElapsed >= FrameDuration)
    {
        m_frameElapsed -= FrameDuration;
        m_currentFrame = (m_currentFrame + 1) % FrameCount;
        AdvenceFrame();
    }
}

void CLoadingTransition::Render(HDC dc)
{
    if (m_alpha <= 0.f || !m_texture)
    {
        return;
    }

    const POINT resolution = CCore::GetInst()->GetResolution();
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(m_alpha * 255.f);
    AlphaBlend(dc, 0, 0, resolution.x, resolution.y,
        m_texture->GetDC(), 0, 0, m_texture->Width(), m_texture->Height(), blend);
}
