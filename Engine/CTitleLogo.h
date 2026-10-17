#pragma once
#include "CUI.h"

class CTexture;

class CTitleLogo
    : public CUI
{
public:
    CTitleLogo();
    virtual ~CTitleLogo() override;

    virtual void render(HDC canvas_) override;

    CLONE(CTitleLogo);

private:
    CTexture* m_pTexture;

};

