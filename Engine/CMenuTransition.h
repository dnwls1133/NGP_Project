#pragma once

#include "CTransitionBase.h"
#include <vector>

class CMenuItem;

class CMenuTransition : public CTransitionBase
{
public:
    using MenuItemList = std::vector<CMenuItem*>;

    void AddMenuItem(CMenuItem& item);

    [[nodiscard]] MenuItemList& GetMenuItems() noexcept;
    [[nodiscard]] const MenuItemList& GetMenuItems() const noexcept;

    void Update() override;
    void Render(HDC _dc) override;

private:
    MenuItemList m_menuItems;
};

inline std::vector<CMenuItem*>& CMenuTransition::GetMenuItems() noexcept
{
    return m_menuItems;
}

inline const std::vector<CMenuItem*>& CMenuTransition::GetMenuItems() const noexcept
{
    return m_menuItems;
}
