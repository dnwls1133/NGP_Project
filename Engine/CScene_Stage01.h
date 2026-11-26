#pragma once
#include "CScene.h"
#include "../host/HostSimCore.h"
#include <deque>


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

    CTexture* m_fadeTexture = nullptr;
    float m_entryFadeAlpha = 1.f;
    bool m_waitingForEntryFade = true;

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
    };

    double m_inputSendAccumulator = 0.0;
    std::array<RemotePlayerState, protocol::MAX_PROTOCOL_PLAYERS> m_remotePlayers{};

    std::array<protocol::ObjectId, protocol::MAX_PROTOCOL_PLAYERS> m_playerObjectIds{};
    std::uint32_t m_hostTick = 0;
    protocol::EventId m_nextSpawnEventId = 2;
    protocol::ObjectId m_lastQueuedSpawnObjectId = 0;
    std::deque<protocol::SpawnPayload> m_pendingSpawns;

    bool m_playerSpawnsQueued = false;
    bool m_bBossBgmPlayed = false;
    bool m_bBossDefeated = false;
    void EvaluateHostMatchOutcome();
    std::deque<protocol::Message> m_pendingItemEvents;

    bool FlushItemEvents();

protected:
    void OnObjectAdded(CObject* _pObj, GROUP_TYPE _eType) override;

public:
    virtual void update();
    virtual void Enter();
    virtual void render(HDC _dc);
    virtual void Exit();

    void Clear() { m_bClear = true; }
    void Fail() { m_bFail = true; }
    void MarkBossDefeated() { m_bBossDefeated = true; }
    void InitEvents();
    bool ConfirmItemPickup(protocol::ObjectId itemId, protocol::PlayerSlot slot);
    void ReportMonsterCombat(
        protocol::ObjectId monsterId,
        protocol::ObjectId missileId,
        protocol::PlayerSlot ownerSlot,
        std::int32_t confirmedHp,
        bool died,
        std::int32_t scoreDelta,
        std::uint32_t teamScore); 
    void QueueDespawn(protocol::ObjectId objectId, protocol::DespawnReason reason);
public:
    CScene_Stage01();
    ~CScene_Stage01();
};

