#include "pch.h"
#include "CSceneTransition.h"

#include "CTimeMgr.h"

#include <utility>

void CSceneTransition::Start(TransitionCallback switchScene)
{
    if (m_background.IsActive())
    {
        return;
    }

    OnOpenComplete() = std::move(switchScene);
    Open();
}

void CSceneTransition::Open()
{
    m_background.Open();
    CTransitionBase::Open([this]
    {
        const float delta = CTimeMgr::GetInst()->GetfDT() / FadeDuration;
        m_background.SetAlpha(m_background.GetAlpha() + delta);
        return m_background.GetAlpha() >= 1.f;
    });
}

void CSceneTransition::OpenComplete()
{
    if (GetCurrentState() != ETransitionState::Open)
    {
        return;
    }

    m_background.SetAlpha(1.f);
    CTransitionBase::OpenComplete();
    Close();
}

void CSceneTransition::Close()
{
    CTransitionBase::Close([this]
    {
        const float delta = CTimeMgr::GetInst()->GetfDT() / FadeDuration;
        m_background.SetAlpha(m_background.GetAlpha() - delta);
        return m_background.GetAlpha() <= 0.f;
    });
}

void CSceneTransition::CloseComplete()
{
    if (GetCurrentState() != ETransitionState::Close)
    {
        return;
    }

    m_background.SetAlpha(0.f);
    m_background.Close();
    CTransitionBase::CloseComplete();
}

void CSceneTransition::Update()
{
    CTransitionBase::Update();
    m_background.Update();
}

void CSceneTransition::Render(HDC dc)
{
    m_background.Render(dc);
}
