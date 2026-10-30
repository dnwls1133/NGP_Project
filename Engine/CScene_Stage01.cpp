#include "pch.h"
#include "CScene_Stage01.h"
#include "CObject.h"

#include "CPlayer.h"
#include "CMonster.h"
#include "CBackground.h"

#include "CCore.h"

#include "CColliderMgr.h"
#include "CPathMgr.h"
#include "CKeyMgr.h"
#include "CSceneMgr.h"
#include "CTimeMgr.h"
#include "CResMgr.h"

#include "CTexture.h"
#include "CSound.h"
#include "CSoundMgr.h"
#include "CCamera.h"

#include "SelectGDI.h"

#include "AI.h"
#include "CState.h"
#include "CIdleState.h"
#include "CTraceState.h"

#include "CStageCard.h"
#include "CStageHUD.h"
#include "CAnimator.h"

CScene_Stage01::CScene_Stage01()
    :m_bClear(false)
    ,m_bFail(false)
    ,m_fClearAcc(0.f)
{
    m_pStageTheme = CResMgr::GetInst()->LoadSound(L"Stage Theme", L"sound\\BGM\\BGM_StageTheme.mp3");
    m_pBossTheme = CResMgr::GetInst()->LoadSound(L"Boss Theme", L"sound\\BGM\\BGM_BossTheme.mp3");
    m_pClearTheme = CResMgr::GetInst()->LoadSound(L"Stage Clear", L"sound\\BGM\\BGM_StageClear.mp3");
    m_pFailTheme = CResMgr::GetInst()->LoadSound(L"Stage Fail", L"sound\\BGM\\BGM_StageFail.mp3");
}

CScene_Stage01::~CScene_Stage01()
{
}

void CScene_Stage01::update()
{
   
   

    if (!m_bFail)
    {
        CScene::update();
        m_dAcc += fDT;
    }
    
    
  
    if (m_bClear)
    {
        if (m_fClearAcc == 0.f)
        {
            m_pHud->SetActive(false);
            CRankMgr::GetInst()->SetPlayer(nullptr);
            CSoundMgr::GetInst()->PlayBGM(m_pClearTheme, false);

            // const Vec2 resolution = CCore::GetInst()->GetResolution();
            // auto clearCard = new CStageCard(false, L"Stage Clear");
            // clearCard->SetName(L"Stage Clear Card");
            // clearCard->SetPos(Vec2(resolution.x / 2.0f, resolution.y / 2.0f));
            // clearCard->SetScale(Vec2(resolution.x, resolution.y));
            // AddObject(clearCard, GROUP_TYPE::UI);
            m_pClearCard = CResMgr::GetInst()->LoadTexture(L"Clear Card", L"texture\\Stage Cards\\StageClear\\slice_16.png");

            CPlayer* player = CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
            player->setclear();
            CCamera::GetInst()->FadeOut(3.f);
        }
        m_fClearAcc += fDT;
        if (m_fClearAcc > 3.f)
        {
           
            ChangeScene(SCENE_TYPE::CREDITS);
            m_fClearAcc = 0;
        }
    }
    else if (m_bFail)
    {
        if (m_fClearAcc == 0.f)
        {
            m_pHud->SetActive(false);
           
            CSoundMgr::GetInst()->PlayBGM(m_pFailTheme, false);

            // const Vec2 resolution = CCore::GetInst()->GetResolution();
            // CCamera::GetInst()->FadeOut(5.f);
            // todo stage clear 패널 띄우기
            // auto failCard = new CStageCard(false, L"StageFail");
            // failCard->SetName(L"Stage Fail Card");
            // failCard->SetPos(Vec2(resolution.x / 2.0f, resolution.y / 2.0f));
            // failCard->SetScale(Vec2(resolution.x, resolution.y));
            // AddObject(failCard, GROUP_TYPE::UI);
            m_pFailCard = CResMgr::GetInst()->LoadTexture(L"Fail Card", L"texture\\Stage Cards\\StageFail\\slice_16.png");
        }
        m_fClearAcc += fDT;
      
        if (KEY_TAP(KEY::SPACE))
        {
            CPlayer* player = CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
            player->FullHP();
            m_bFail = false;
            m_fClearAcc = 0.f;
            m_pFailCard = nullptr;
            if (m_dAcc >= 130.f)
            {
                CSoundMgr::GetInst()->PlayBGM(m_pBossTheme);
            }
            else
            {
                CSoundMgr::GetInst()->PlayBGM(m_pStageTheme);
            }
           
            m_pHud->SetActive(true);

            //CCamera::GetInst()->FadeIn(0.5f);
        }
        if (5.f + fDT >= m_fClearAcc && m_fClearAcc > 5.f)
        {
           
            CCamera::GetInst()->FadeOut(5.f);
        }
        if (m_fClearAcc >= 10.f)
        {
            CRankMgr::GetInst()->SetPlayer(nullptr);
            ChangeScene(SCENE_TYPE::TITLE);

            m_fClearAcc = 0.f;


        }
        
    }
    else
    {
        if (KEY_TAP(KEY::SPACE))
        {
            CPlayer* player = CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0);
            player->isMujeok = true;
        }

        for (auto& evt : m_vEvents)
        {
            if (!evt.triggered && m_dAcc >= evt.triggerTime)
            {
                static bool trigger = false;
                if (m_dAcc >= 130.f && !trigger)
                {
                    CSoundMgr::GetInst()->PlayBGM(m_pBossTheme);
                    trigger = true;
                }

                CMonster* pMon = CMonFactory::CreateMonster(evt.type, evt.mtype, evt.spawnPos, evt.targetPos, evt.exitPos);
                pMon->SetName(L"Monster");
                AddObject(pMon, GROUP_TYPE::MONSTER);
                evt.triggered = true;
            }
        }
    }
   


}
void CScene_Stage01::render(HDC _dc)
{
    CScene::render(_dc);
    CBackground* back = (CBackground*)GetBackground();
    Vec2 backinfo = back->GetScale();
    Vec2 backPos = back->GetPos();
    POINT resolution = CCore::GetInst()->GetResolution();
    SelectGDI gdi(_dc, BRUSH_TYPE::BLACK);
    Rectangle(_dc, 0, 0
        , (int)(backPos.x - backinfo.x / 2)
        , (int)(backPos.y + backinfo.y / 2));
    Rectangle(_dc
        , (int)(backPos.x + backinfo.x / 2)
        , 0
        , (int)resolution.x
        , (int)resolution.y);

    if (m_pClearCard)
    {
        const Vec2 resolution = CCore::GetInst()->GetResolution();

        const Vec2 position = Vec2(resolution.x / 2.0f, resolution.y / 2.0f);  // 중심 위치
        const Vec2 scale = Vec2(resolution.x, resolution.y);     // 출력될 크기

        const int width = static_cast<int>(m_pClearCard->Width());   // 원본 너비
        const int height = static_cast<int>(m_pClearCard->Height());  // 원본 높이

        TransparentBlt(
            _dc,
            static_cast<int>(position.x - width / 2),
            static_cast<int>(position.y - height / 2),
            width,
            height,
            m_pClearCard->GetDC(),
            0, 0,
            width, height,
            RGB(255, 0, 255)
        );
    }

    if (m_pFailCard)
    {
        const Vec2 resolution = CCore::GetInst()->GetResolution();

        const Vec2 position = Vec2(resolution.x / 2.0f, resolution.y / 2.0f);  // 중심 위치
        const Vec2 scale = Vec2(resolution.x, resolution.y);     // 출력될 크기

        const int width = static_cast<int>(m_pFailCard->Width());   // 원본 너비
        const int height = static_cast<int>(m_pFailCard->Height());  // 원본 높이

        TransparentBlt(
            _dc,
            static_cast<int>(position.x - width / 2),
            static_cast<int>(position.y - height / 2),
            width,
            height,
            m_pFailCard->GetDC(),
            0, 0,
            width, height,
            RGB(255, 0, 255)
        );
    }
}
void CScene_Stage01::Enter()
{
    // Warm the shared texture cache before combat, using the actual animation assets.
    // Only temporary playback objects are destroyed; CResMgr retains the textures.
    {
        CAnimator preload;
        preload.LoadAnimation(L"animation\\Boss1Enemy_Move.anim");
        preload.LoadAnimation(L"animation\\Boss1Enemy_Hit.anim");
        preload.LoadAnimation(L"animation\\Boss1Enemy_Dead.anim");
    }

    CRankMgr::GetInst()->CurrentRanking.Score = 0;

    m_pClearCard = nullptr;
    m_pFailCard = nullptr;

    Vec2 vResolution = CCore::GetInst()->GetResolution();

    // BackGround Object 추가
    CObject* pBackgroundObj = new CBackground;
    pBackgroundObj->SetPos(Vec2(vResolution.x / 2.f, vResolution.y / 2.f));
    pBackgroundObj->SetScale(Vec2(540.f, 960.f));
    pBackgroundObj->SetName(L"Background");
    AddObject(pBackgroundObj, GROUP_TYPE::BACKGROUND);

    RegisterBackground(pBackgroundObj);

    // Player Object 추가
    CPlayer* pObj = new CPlayer;
    pObj->SetPos(Vec2(vResolution.x /2.f, vResolution.y + 200.f));
    pObj->SetScale(Vec2(100.f, 100.f));
    pObj->SetName(L"Player");
    AddObject(pObj, GROUP_TYPE::PLAYER);

    RegisterPlayer(0, pObj);
    CRankMgr::GetInst()->SetPlayer(pObj);


    // Monster Object 추가
   
    InitEvents();
    

    // 타일 로딩
    //LoadTile(L"Tile\\Start.tile");


    // 충돌 지정
    // Player 그룹과 Monster 그룹간의 충돌체크
    CColliderMgr::GetInst()->CheckGroup(GROUP_TYPE::PLAYER, GROUP_TYPE::PROJ_MONSTER);
    CColliderMgr::GetInst()->CheckGroup(GROUP_TYPE::MONSTER, GROUP_TYPE::PROJ_PLAYER);
    CColliderMgr::GetInst()->CheckGroup(GROUP_TYPE::PLAYER, GROUP_TYPE::ITEM);
    CColliderMgr::GetInst()->CheckGroup(GROUP_TYPE::PLAYER, GROUP_TYPE::MONSTER);
    //Camera 지정

    CCamera::GetInst()->SetLookAt(vResolution / 2.f);

    CCamera::GetInst()->FadeOut(1.5f);
    CCamera::GetInst()->FadeIn(1.5f);
    m_dAcc = 0.f;
    //Camera 효과 지정
   // CCamera::GetInst()->Fadeout(5.f);

    CSoundMgr::GetInst()->PlayBGM(m_pStageTheme);

   

    m_pHud = new CStageHUD;
    m_pHud->SetActive(true);
    AddObject(m_pHud, GROUP_TYPE::UI);
}

void CScene_Stage01::Exit()
{
    DeleteAll();
    m_dAcc = 0;
    m_bClear = false;
    m_bFail = false;
    m_fClearAcc = 0.f;
    m_vEvents.clear();
    CColliderMgr::GetInst()->Reset();
}

void CScene_Stage01::InitEvents()
{
    Vec2 vResolution = CCore::GetInst()->GetResolution();
    Vec2 vBackPos = GetBackground()->GetPos();
    Vec2 vBackScale = GetBackground()->GetScale();
    RECT rBackrect = { (vBackPos.x - vBackScale.x / 2.f),(vBackPos.y - vBackScale.y / 2.f),(vBackPos.x + vBackScale.x / 2.f),(vBackPos.x + vBackScale.y / 2.f) };


    for (int i = 1; i < 80; ++i)
    {
        if (i % 2 == 0)
        {
            m_vEvents.push_back({ 5.0f * i,MON_TYPE::NORMAL3,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left + 200.f, -100.f),Vec2((float)rBackrect.left + 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.right + 400.f,vResolution.y) });
            m_vEvents.push_back({ (5.0f * i) + 0.5f,MON_TYPE::NORMAL4,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left + 150.f, -100.f),Vec2((float)rBackrect.left + 150.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 400.f,vResolution.y) });
            m_vEvents.push_back({ (5.0f * i) + 1.f,MON_TYPE::NORMAL4, MISSILE_PTRN::NONE,Vec2((float)rBackrect.left + 100.f, -100.f),Vec2((float)rBackrect.left + 100.f,(float)rBackrect.top + 100.f),Vec2((float)rBackrect.right + 400.f,vResolution.y) });

        }
        else
        {
            m_vEvents.push_back({ 5.0f * i,MON_TYPE::NORMAL3,MISSILE_PTRN::NONE, Vec2((float)rBackrect.right - 200.f , -100.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
            m_vEvents.push_back({ (5.0f * i) + 0.5f,MON_TYPE::NORMAL4,MISSILE_PTRN::NONE, Vec2((float)rBackrect.right - 150.f , -100.f),Vec2((float)rBackrect.right - 150.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
            m_vEvents.push_back({ (5.0f * i) + 1.f,MON_TYPE::NORMAL4, MISSILE_PTRN::NONE,Vec2((float)rBackrect.right - 100.f , -100.f),Vec2((float)rBackrect.right - 100.f,(float)rBackrect.top + 100.f),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
        }
        
    }
   
  
    // PTRN 2
    {
        for (int i = 0; i < 10; ++i)
        {
            if (i % 2 == 0)
            {
                m_vEvents.push_back({ 8.0f+0.2f*i,MON_TYPE::NORMAL2, MISSILE_PTRN::PTRN1,Vec2((float)rBackrect.left + 200.f , -100.f),Vec2((float)rBackrect.left + 200.f,(float)rBackrect.top + 500.f - 30.f*i),Vec2((float)rBackrect.right + 400.f,vResolution.y) });
            }
            else
            {
                m_vEvents.push_back({ 8.0f+0.2f*i,MON_TYPE::NORMAL2, MISSILE_PTRN::PTRN1,Vec2((float)rBackrect.left + 250.f , -100.f),Vec2((float)rBackrect.left + 150.f,(float)rBackrect.top + 500.f - 30.f*i),Vec2((float)rBackrect.right + 400.f,vResolution.y) });
            }
        }
        for (int i = 0; i < 10; ++i)
        {
            if (i % 2 == 0)
            {
                m_vEvents.push_back({ 13.0f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN1, Vec2((float)rBackrect.right - 200.f , -100.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 500.f - 30.f * i),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
            }
            else
            {
                m_vEvents.push_back({ 13.0f + 0.2f * i,MON_TYPE::NORMAL2, MISSILE_PTRN::PTRN1,Vec2((float)rBackrect.right - 250.f , -100.f),Vec2((float)rBackrect.right - 150.f,(float)rBackrect.top + 500.f - 30.f * i),Vec2((float)rBackrect.left - 400.f,vResolution.y) });
            }
        }
       
      
    }

    // PTRN 3
    {
        for (int i = 0; i < 10; ++i)
        {
            m_vEvents.push_back({ 15.0f + 0.4f*i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left - 250.f,(float)rBackrect.bottom / 2.f),Vec2(vBackPos.x,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 250.f,(float)rBackrect.bottom / 2.f) });

        }
        for (int i = 0; i < 10; ++i)
        {
            m_vEvents.push_back({ 18.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE,Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f),Vec2(vBackPos.x,(float)rBackrect.top + 250.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f) });

        }

    }


    // PTRN 3
    {
        m_vEvents.push_back({ 20.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN1, Vec2((float)rBackrect.left - 200.f,  (float)rBackrect.top + 250.f),Vec2((float)rBackrect.left + 200.f, (float)rBackrect.top + 200.f),Vec2((float)rBackrect.left + 100.f, -400.f) });
        m_vEvents.push_back({ 25.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN1,Vec2((float)rBackrect.right + 200.f, (float)rBackrect.top + 250.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.right - 100.f, -400.f) });

    }
  
    for (int i = 0; i < 4; ++i)
    {
        if (i % 2 == 0)
        {
            m_vEvents.push_back({ 35.0f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.right - 200.f , -100.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 300.f - 50.f * i),Vec2((float)rBackrect.right + 400.f,vResolution.y/2.f) });
        }
        else
        {
            m_vEvents.push_back({ 35.0f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.right - 250.f , -100.f),Vec2((float)rBackrect.right - 150.f,(float)rBackrect.top + 300.f - 50.f * i),Vec2((float)rBackrect.right + 400.f,vResolution.y/2.f) });
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        if (i % 2 == 0)
        {
            m_vEvents.push_back({ 35.5f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.left + 200.f , -100.f),Vec2((float)rBackrect.left + 200.f,(float)rBackrect.top + 300.f - 50.f * i),Vec2((float)rBackrect.left - 400.f,vResolution.y / 2.f) });
        }
        else
        {
            m_vEvents.push_back({ 36.0f + 0.2f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.left + 250.f , -100.f),Vec2((float)rBackrect.left + 150.f,(float)rBackrect.top + 300.f - 50.f * i),Vec2((float)rBackrect.left - 400.f,vResolution.y / 2.f) });
        }
    }



    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 35.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x + 200.f,(float)rBackrect.bottom/2.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 40.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE,Vec2((float)rBackrect.right + 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x - 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });
    }
    for (int i = 0; i < 4; ++i)
    {
        m_vEvents.push_back({ 45.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom/2.f),Vec2(vBackPos.x,(float)rBackrect.bottom / 2.f - 50.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom/2.f) });

    }
    for (int i = 0; i < 4; ++i)
    {
        m_vEvents.push_back({ 47.5f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f-100.f),Vec2(vBackPos.x,(float)rBackrect.bottom / 2.f - 150.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f - 100.f) });

    }
    for (int i = 0; i < 4; ++i)
    {
        m_vEvents.push_back({ 50.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f-300.f),Vec2(vBackPos.x,(float)rBackrect.bottom / 2.f - 350.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f - 300.f)  });

    }
    for (int i = 0; i < 4; ++i)
    {
        m_vEvents.push_back({ 52.5f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom / 2.f- 500.f),Vec2(vBackPos.x,(float)rBackrect.bottom / 2.f -550.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom / 2.f- 500.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 55.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x + 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 57.5f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE,Vec2((float)rBackrect.right + 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x - 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });
    }

    // PTRN 3
    {
        m_vEvents.push_back({ 60.0f,MON_TYPE::EPIC,MISSILE_PTRN::PTRN1, Vec2(vBackPos.x,  (float)rBackrect.top - 200.f),Vec2((float)vBackPos.x, (float)rBackrect.bottom/2.f-300.f),Vec2((float)vBackPos.x, -400.f) });
    }

    // PTRN 4
    {
        m_vEvents.push_back({ 80.0f,MON_TYPE::EPIC2,MISSILE_PTRN::PTRN1, Vec2((float)rBackrect.left + 150.f,  (float)rBackrect.top - 200.f),Vec2((float)rBackrect.left + 230.f, (float)rBackrect.bottom / 2.f - 250.f),Vec2((float)rBackrect.left + 100.f, -400.f) });
        m_vEvents.push_back({ 80.5f,MON_TYPE::EPIC2,MISSILE_PTRN::PTRN1, Vec2((float)rBackrect.right - 150.f,  (float)rBackrect.top - 200.f),Vec2((float)rBackrect.right - 180.f, (float)rBackrect.bottom / 2.f - 300.f),Vec2((float)rBackrect.right - 100.f, -400.f) });
    }

    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 90.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3, Vec2((float)rBackrect.left + 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.left + 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 95.5f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3,Vec2((float)rBackrect.right - 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.right - 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });
    }

    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 105.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3,Vec2((float)rBackrect.right - 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.right - 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });


    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 110.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3, Vec2((float)rBackrect.left + 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.left + 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });
    }
    m_vEvents.push_back({ 100.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.left + 100.f,  (float)rBackrect.top - 200.f),Vec2((float)rBackrect.left + 150.f, (float)rBackrect.top + 200.f),Vec2((float)rBackrect.right + 400.f, -400.f) });
    m_vEvents.push_back({ 105.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.right - 200.f, (float)rBackrect.top - 200.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.left - 400.f, -400.f) });
    m_vEvents.push_back({ 115.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN2, Vec2((float)rBackrect.left + 100.f,  (float)rBackrect.top - 200.f),Vec2((float)rBackrect.left + 150.f, (float)rBackrect.top + 200.f),Vec2((float)rBackrect.right + 400.f, -400.f) });
    m_vEvents.push_back({ 120.0f,MON_TYPE::NORMAL1,MISSILE_PTRN::PTRN2,Vec2((float)rBackrect.right - 200.f, (float)rBackrect.top - 200.f),Vec2((float)rBackrect.right - 200.f,(float)rBackrect.top + 200.f),Vec2((float)rBackrect.left - 400.f, -400.f) });
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 115.0f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE, Vec2((float)rBackrect.left - 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x + 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 117.5f + 0.4f * i,MON_TYPE::NORMAL5,MISSILE_PTRN::NONE,Vec2((float)rBackrect.right + 250.f, (float)rBackrect.top - 100.f),Vec2(vBackPos.x - 200.f,(float)rBackrect.bottom / 2.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });
    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 116.0f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3, Vec2((float)rBackrect.left + 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.left + 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.right + 250.f, (float)rBackrect.bottom + 100.f) });

    }
    for (int i = 0; i < 10; ++i)
    {
        m_vEvents.push_back({ 121.5f + 0.4f * i,MON_TYPE::NORMAL2,MISSILE_PTRN::PTRN3,Vec2((float)rBackrect.right - 300.f, (float)rBackrect.top - 100.f),Vec2((float)rBackrect.right - 100.f,(float)rBackrect.top + 150.f),Vec2((float)rBackrect.left - 250.f, (float)rBackrect.bottom + 100.f) });
    }









    m_vEvents.push_back({ 10.f,MON_TYPE::BOSS,MISSILE_PTRN::PTRN1,Vec2(vBackPos.x,  (float)rBackrect.top - 200.f),Vec2((float)vBackPos.x, (float)rBackrect.bottom / 2.f - 300.f),Vec2((float)vBackPos.x, -400.f) });

}


