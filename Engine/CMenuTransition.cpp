#include "pch.h"
#include "CMenuTransition.h"

#include "CMenuItem.h"

#include <algorithm>
#include <utility>

void CMenuTransition::AddMenuItem(CMenuItem& item)
{
    if (std::ranges::find(m_menuItems, &item) == m_menuItems.end())
    {
        m_menuItems.push_back(&item);
    }
}

void CMenuTransition::Open(TransitionStep step)
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

void CMenuTransition::OpenComplete()
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

void CMenuTransition::Close(TransitionStep step)
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

void CMenuTransition::CloseComplete()
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

void CMenuTransition::Update()
{
    if (IsPlaying())
    {
        const ETransitionState state = m_currentState;
        if (m_step && m_step())
        {
            if (state == ETransitionState::Open)
            {
                OpenComplete();
            }
            else
            {
                CloseComplete();
            }
        }
        return;
    }

    if (m_currentState != ETransitionState::Idle)
    {
        return;
    }

    for (CMenuItem* item : m_menuItems)
    {
        item->update();
    }
}

void CMenuTransition::Render(HDC _dc)
{
    if (m_currentState == ETransitionState::CloseComplete)
    {
        return;
    }

    for (CMenuItem* item : m_menuItems)
    {
        item->render(_dc);
    }
}
