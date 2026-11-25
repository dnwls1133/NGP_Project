#pragma once

#include "CTransitionBase.h"
#include <vector>

class CMenuItem;

class CMenuTransition : public CTransitionBase
{
public:
    void AddMenuItem(CMenuItem& item);

    [[nodiscard]] std::vector<CMenuItem*>& GetMenuItems() noexcept;
    [[nodiscard]] const std::vector<CMenuItem*>& GetMenuItems() const noexcept;

    void Update() override;
    void Render(HDC _dc) override;

private:
    std::vector<CMenuItem*> m_menuItems;
};

inline std::vector<CMenuItem*>& CMenuTransition::GetMenuItems() noexcept
{
    return m_menuItems;
}

inline const std::vector<CMenuItem*>& CMenuTransition::GetMenuItems() const noexcept
{
    return m_menuItems;
}
