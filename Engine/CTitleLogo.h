#pragma once
#include "CUI.h"

class CTexture;

class CTitleLogo
    : public CUI
{
public:
    CTitleLogo();
    virtual ~CTitleLogo() override;

    [[nodiscard]] float GetImageCenterX() const;

    virtual void render(HDC canvas_) override;

    CLONE(CTitleLogo);

private:
    int GetImageLeft(float positionX) const;

    CTexture* m_pTexture;

};

