#pragma once

#include "CUI.h"

class CTitleMenu : public CUI
{
    CLONE(CTitleMenu)
public:
    CTitleMenu() : CUI(false) {}

    void render(HDC dc) override
    {
        if (IsActive())
        {
            render_child(dc);
        }
    }
};
