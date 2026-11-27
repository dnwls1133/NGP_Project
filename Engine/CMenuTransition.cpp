#include "pch.h"
#include "CMenuTransition.h"

#include "CMenuItem.h"

#include <algorithm>

void CMenuTransition::AddMenuItem(CMenuItem& item)
{
    if (std::ranges::find(m_menuItems, &item) == m_menuItems.end())
    {
        m_menuItems.push_back(&item);
    }
}

void CMenuTransition::Update()
{
    if (IsPlaying())
    {
        CTransitionBase::Update();
        return;
    }

    if (GetCurrentState() != ETransitionState::Idle)
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
    if (GetCurrentState() == ETransitionState::CloseComplete)
    {
        return;
    }

    for (CMenuItem* item : m_menuItems)
    {
        item->render(_dc);
    }
}
