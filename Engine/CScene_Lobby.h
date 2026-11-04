#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>

#include "CScene.h"

class CTexture;
class CSound;

class CScene_Lobby : public CScene
{
public:
    CScene_Lobby();
    ~CScene_Lobby() override = default;

    virtual void Enter();
    virtual void update();
    virtual void render(HDC _dc);
    virtual void Exit();

    void SetHostAddress(const std::wstring& address) { m_hostAddress = address; }
    void GetIPAddress(WPARAM character);

private:
    CTexture* m_pBackground;
    CSound* m_pTheme;

    HFONT m_hFont;
    std::wstring m_hostAddress;
};
