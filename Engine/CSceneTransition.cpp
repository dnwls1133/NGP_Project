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
    m_background.Open();
    Open([this] { return Fade(1.f, 255.f); });
}

void CSceneTransition::Update()
{
    const bool wasOpening = GetCurrentState() == ETransitionState::Open;
    CTransitionBase::Update();

    if (wasOpening && GetCurrentState() == ETransitionState::Idle)
    {
        Close([this] { return Fade(-1.f, 0.f); });
    }

    if (GetCurrentState() == ETransitionState::CloseComplete && m_background.IsActive())
    {
        m_background.Close();
    }

    m_background.Update();
}

void CSceneTransition::Render(HDC dc)
{
    m_background.Render(dc);
}

bool CSceneTransition::Fade(float direction, float targetAlpha)
{
    constexpr float fadeSpeed = 510.f;
    const float alphaDelta = CTimeMgr::GetInst()->GetfDT() * fadeSpeed;
    m_background.SetAlpha(m_background.GetAlpha() + direction * alphaDelta);
    return m_background.GetAlpha() == targetAlpha;
}
