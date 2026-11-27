#include "pch.h"
#include "CTransitionBase.h"

#include <utility>

void CTransitionBase::Open(TransitionStep step)
{
    if (m_currentState != ETransitionState::Idle &&
        m_currentState != ETransitionState::CloseComplete)
    {
        return;
    }

    m_currentState = ETransitionState::Open;
    m_step = std::move(step);
    if (m_onOpen)
    {
        m_onOpen();
    }
    if (!m_step)
    {
        OpenComplete();
    }
}

void CTransitionBase::OpenComplete()
{
    if (m_currentState != ETransitionState::Open)
    {
        return;
    }

    m_currentState = ETransitionState::OpenComplete;
    m_step = {};
    if (m_onOpenComplete)
    {
        m_onOpenComplete();
    }
    m_currentState = ETransitionState::Idle;
}

void CTransitionBase::Close(TransitionStep step)
{
    if (m_currentState != ETransitionState::Idle)
    {
        return;
    }

    m_currentState = ETransitionState::Close;
    m_step = std::move(step);
    if (m_onClose)
    {
        m_onClose();
    }
    if (!m_step)
    {
        CloseComplete();
    }
}

void CTransitionBase::CloseComplete()
{
    if (m_currentState != ETransitionState::Close)
    {
        return;
    }

    m_currentState = ETransitionState::CloseComplete;
    m_step = {};
    if (m_onCloseComplete)
    {
        m_onCloseComplete();
    }
}

void CTransitionBase::Update()
{
    if (!m_step)
    {
        return;
    }

    const ETransitionState state = m_currentState;
    // A step may complete the transition and clear m_step while it is running.
    TransitionStep step = std::exchange(m_step, {});
    const bool complete = step();
    if (m_currentState != state || m_step)
    {
        return;
    }

    if (!complete)
    {
        m_step = std::move(step);
        return;
    }

    if (state == ETransitionState::Open)
    {
        OpenComplete();
    }
    else
    {
        CloseComplete();
    }
}
