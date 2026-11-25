#include "pch.h"
#include "CTitleTransition.h"

#include "CCore.h"
#include "CResMgr.h"
#include "CTexture.h"
#include "CTimeMgr.h"

#include <algorithm>
#include <format>

void CTitleTransition::Open()
{
    if (GetCurrentState() != ETransitionState::Idle &&
        GetCurrentState() != ETransitionState::CloseComplete)
    {
        return;
    }

    m_currentFrame = 0;
    m_frameElapsed = 0.f;
    m_alpha = 0.f;
    LoadOpeningFrame();

    CTransitionBase::Open([this]
    {
        if (!AdvanceFrame())
        {
            return false;
        }

        if (++m_currentFrame >= OpeningLastFrame)
        {
            return true;
        }

        LoadOpeningFrame();
        return false;
    });
}

void CTitleTransition::OpenComplete()
{
    if (GetCurrentState() != ETransitionState::Open)
    {
        return;
    }

    m_whiteTexture = CResMgr::GetInst()->CreateTexture(L"Title Reveal White", 1, 1);
    PatBlt(m_whiteTexture->GetDC(), 0, 0, 1, 1, WHITENESS);

    m_alpha = 1.f;
    m_currentFrame = 0;
    m_frameElapsed = 0.f;
    LoadTitleFrame();

    CTransitionBase::OpenComplete();
    Close();
}

void CTitleTransition::Close()
{
    CTransitionBase::Close([this]
    {
        const float delta = CTimeMgr::GetInst()->GetfDT() / FadeDuration;
        m_alpha = std::clamp(m_alpha - delta, 0.f, 1.f);
        return m_alpha == 0.f;
    });
}

void CTitleTransition::CloseComplete()
{
    if (GetCurrentState() != ETransitionState::Close)
    {
        return;
    }

    m_alpha = 0.f;
    CTransitionBase::CloseComplete();
}

void CTitleTransition::LoadOpeningFrame()
{
    const auto key = std::format(L"Introduction No.{}", m_currentFrame);
    const auto path = std::format(L"texture\\Opening\\Sprite_Background_Opening_{}.png", m_currentFrame);
    m_texture = CResMgr::GetInst()->LoadTexture(key, path);
}

void CTitleTransition::LoadTitleFrame()
{
    const auto key = std::format(L"Main Title No.{}", m_currentFrame);
    const auto path = std::format(L"texture\\Main Title\\Sprite_Background_MainTitle_{}.png", m_currentFrame);
    m_texture = CResMgr::GetInst()->LoadTexture(key, path);
}

bool CTitleTransition::AdvanceFrame()
{
    m_frameElapsed += CTimeMgr::GetInst()->GetfDT();
    if (m_frameElapsed < FrameDuration)
    {
        return false;
    }

    m_frameElapsed = 0.f;
    return true;
}

void CTitleTransition::Update()
{
    const auto state = GetCurrentState();
    if ((state == ETransitionState::Close || state == ETransitionState::CloseComplete) && AdvanceFrame())
    {
        m_currentFrame = (m_currentFrame + 1) % TitleFrameCount;
        LoadTitleFrame();
    }

    CTransitionBase::Update();
}

void CTitleTransition::Render(HDC dc)
{
    if (!m_texture)
    {
        return;
    }

    const POINT resolution = CCore::GetInst()->GetResolution();
    TransparentBlt(dc, 0, 0, resolution.x, resolution.y,
        m_texture->GetDC(), 0, 0, m_texture->Width(), m_texture->Height(), RGB(255, 0, 255));
}

void CTitleTransition::RenderOverlay(HDC dc)
{
    if (m_alpha <= 0.f)
    {
        return;
    }

    const POINT resolution = CCore::GetInst()->GetResolution();
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(m_alpha * 255.f);
    AlphaBlend(dc, 0, 0, resolution.x, resolution.y,
        m_whiteTexture->GetDC(), 0, 0, 1, 1, blend);
}
