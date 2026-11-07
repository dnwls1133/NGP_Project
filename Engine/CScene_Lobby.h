#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>

#include "CScene.h"
#include "CSessionMgr.h"

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

    void GetRoomCodeInput(WPARAM character);

private:
    CTexture* m_pBackground;
    CSound* m_pTheme;

    HFONT m_hFont;

    std::wstring inputRoomCode;
    bool leavingLobby = false;

    void LogSessionState();
    void ResumeHostWaiting();

#ifdef _DEBUG
    bool hasSessionLog = false;
    network::SessionState loggedState = network::SessionState::Idle;
    bool loggedReady = false;
#endif
};
