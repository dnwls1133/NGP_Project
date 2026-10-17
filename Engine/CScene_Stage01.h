#pragma once
#include "CScene.h"

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

