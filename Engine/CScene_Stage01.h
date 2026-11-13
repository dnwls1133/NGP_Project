#pragma once
#include "CScene.h"
#include "../host/HostSimCore.h"
#include <optional>

class CSound;
class CStageHUD;
class CTexture;

class CScene_Stage01 :
    public CScene
{
private:
    vector<SpawnEvent> m_vEvents;
    bool               m_bClear;
    bool               m_bFail;
    float              m_fClearAcc;

    CStageHUD* m_pHud;

    CSound* m_pStageTheme;          // 스테이지 메인테마.
    CSound* m_pBossTheme;           // 보스 테마.
    CSound* m_pClearTheme;          // 스테이지 클리어 테마.
    CSound* m_pFailTheme;           // 스테이지 페일 테마.

    CTexture* m_pClearCard = nullptr;
    CTexture* m_pFailCard = nullptr;;

    host::HostSimEngine m_hostInput;
    double m_hostTickAccumulator = 0.0;

    struct RemotePlayerState
    {
        protocol::ObjectId objectId = 0;
        std::optional<std::uint32_t> lastSnapshotTick;
    };

    double m_inputSendAccumulator = 0.0;
    std::array<RemotePlayerState, protocol::MAX_PROTOCOL_PLAYERS> m_remotePlayers{};

    std::array<protocol::ObjectId, protocol::MAX_PROTOCOL_PLAYERS> m_playerObjectIds{};
    std::uint32_t m_hostTick = 0;
    bool m_playerSpawnsQueued = false;

public:
    virtual void update();
    virtual void Enter();
    virtual void render(HDC _dc);
    virtual void Exit();

    void Clear() { m_bClear = true; }
    void Fail() { m_bFail = true; }
    void InitEvents();
public:
    CScene_Stage01();
    ~CScene_Stage01();
};

