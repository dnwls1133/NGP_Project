#include "pch.h"
#include "CScene_Title.h"

#include "CCamera.h"
#include "CCore.h"
#include "CKeyMgr.h"
#include "CObject.h"
#include "CAnimator.h"
#include "CAnimation.h"

#include "CScene.h"
#include "CSceneMgr.h"

#include "CResMgr.h"
#include "CSound.h"
#include "CSoundMgr.h"
#include "CColliderMgr.h"

#include "CBtnUI.h"
#include "CMenuItem.h"
#include "CTitleMenu.hpp"

#include "CTitleLogo.h"
#include "CTitleBackground.h"
#include "CRankMgr.h"
// include "CPanelUI.h"

void CScene_Title::Enter()
{
    const Vec2 resolution = CCore::GetInst()->GetResolution();
    // CCamera::GetInst()->SetLookAt(vResolution / 2.f);

    m_pBackground = new CTitleBackground();
    m_pBackground->SetName(L"Background");
    m_pBackground->SetPos(Vec2(resolution.x / 2.0f, resolution.y / 2.0f));
    m_pBackground->SetScale(Vec2(resolution.x, resolution.y));

    AddObject(m_pBackground, GROUP_TYPE::BACKGROUND);

    // m_pLogo = new CTitleLogo();
    // m_pLogo->SetPos
   
    m_pTitleTheme = CResMgr::GetInst()->LoadSound(L"Title", L"sound\\BGM\\BGM_OpeningTheme.mp3");

    CRankMgr::GetInst()->LoadRanking();
    
    CSoundMgr::GetInst()->PlayBGM(m_pTitleTheme, false);

    CreateMenus();
    m_pFirstMenu->SetActive(false);

    m_pBackground->PlayIntroAnimation();
    m_CurrentState = EState::Intro;
}

void CScene_Title::update()
{
    CScene::update();

    if (m_CurrentState == EState::Title && m_pSecondMenu &&
        m_pSecondMenu->IsActive() && KEY_TAP(KEY::ESC))
    {
        m_pSecondMenu->SetActive(false);
        m_pFirstMenu->SetActive(true);
    }

    if (m_CurrentState == EState::Intro)
    {
        if (!m_pBackground->IsPlayIntroAnimation() || KEY_TAP(KEY::ENTER))
        {
            m_CurrentState = EState::Title;
            m_pBackground->PlayTitleAnimation();

            auto m_pLogo = new CTitleLogo();
            m_pLogo->SetName(L"Game Logo");
            m_pLogo->SetPos(Vec2(150.0f, 50.0f));
            m_pLogo->SetScale(Vec2(404.6f, 352.8f));
            AddObject(m_pLogo, GROUP_TYPE::UI);

            m_pFirstMenu->SetActive(true);
        }
    }
}

void CScene_Title::Exit()
{
    DeleteAll();
    m_pFirstMenu = nullptr;
    m_pSecondMenu = nullptr;
    CColliderMgr::GetInst()->Reset();
}

void CScene_Title::CreateMenus()
{
    m_pFirstMenu = new CTitleMenu();
    m_pFirstMenu->SetScale(CCore::GetInst()->GetResolution());

    auto m_pStartButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"StartGame_Idle", L"texture\\Menu Items\\Start Game\\Sprite_Label_StartGame_0.png"),
        CResMgr::GetInst()->LoadTexture(L"StartGame_Hover", L"texture\\Menu Items\\Start Game\\Sprite_Label_StartGame_1.png")
    );
    m_pStartButton->SetPos(Vec2(290.0f, 520.0f));
    m_pStartButton->SetScale(Vec2(210.0f, 70.0f));
    m_pStartButton->SetClickedCallBack(
        [](DWORD_PTR scene, DWORD_PTR) {
            auto* titleScene = reinterpret_cast<CScene_Title*>(scene);
            titleScene->m_pFirstMenu->SetActive(false);
            titleScene->m_pSecondMenu->SetActive(true);
        },
        reinterpret_cast<DWORD_PTR>(this), (DWORD_PTR)0
    );
    m_pFirstMenu->AddChild(m_pStartButton);

    auto m_pManuelButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Manuel_Idle", L"texture\\Menu Items\\Manual\\Sprite_Label_Manual_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Manuel_Hover", L"texture\\Menu Items\\Manual\\Sprite_Label_Manual_1.png")
    );
    m_pManuelButton->SetPos(Vec2(325.0f, 600.0f));
    m_pManuelButton->SetScale(Vec2(160.0f, 70.0f));
    m_pManuelButton->SetClickedCallBack(
        [](DWORD_PTR, DWORD_PTR) {
            ChangeScene(SCENE_TYPE::MANUAL);
        },
        (DWORD_PTR)0, (DWORD_PTR)0
    );
    m_pFirstMenu->AddChild(m_pManuelButton);

    auto m_pRankingButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Ranking_Idle", L"texture\\Menu Items\\Score Ranking\\Sprite_Label_GameRanking_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Ranking_Hover", L"texture\\Menu Items\\Score Ranking\\Sprite_Label_GameRanking_1.png")
    );
    m_pRankingButton->SetPos(Vec2(290.0f, 680.0f));
    m_pRankingButton->SetScale(Vec2(210.0f, 70.0f));
    m_pRankingButton->SetClickedCallBack(
        [](DWORD_PTR, DWORD_PTR) {
            ChangeScene(SCENE_TYPE::RANKING);
        },
        (DWORD_PTR)0, (DWORD_PTR)0
    );
    m_pFirstMenu->AddChild(m_pRankingButton);

    auto m_pExitButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"QuitGame_Idle", L"texture\\Menu Items\\Quit Game\\Sprite_Label_QuitGame_0.png"),
        CResMgr::GetInst()->LoadTexture(L"QuitGame_Hover", L"texture\\Menu Items\\Quit Game\\Sprite_Label_QuitGame_1.png")
    );
    m_pExitButton->SetPos(Vec2(290.0f, 760.0f));
    m_pExitButton->SetScale(Vec2(210.0f, 70.0f));
    m_pExitButton->SetClickedCallBack(
        [](DWORD_PTR, DWORD_PTR) {
            // ChangeScene(SCENE_TYPE::START);
            CRankMgr::GetInst()->SaveRanking();
            PostQuitMessage(0);
        },
        (DWORD_PTR)0, (DWORD_PTR)0
    );
    m_pFirstMenu->AddChild(m_pExitButton);
    AddObject(m_pFirstMenu, GROUP_TYPE::UI);

    m_pSecondMenu = new CTitleMenu();
    m_pSecondMenu->SetScale(CCore::GetInst()->GetResolution());

    auto openRoomButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"OpenRoom_Idle", L"texture\\Menu Items\\Open Room\\Sprite_Label_OpenRoom_0.png"),
        CResMgr::GetInst()->LoadTexture(L"OpenRoom_Hover", L"texture\\Menu Items\\Open Room\\Sprite_Label_OpenRoom_1.png"));
    openRoomButton->SetPos(Vec2(290.0f, 520.0f));
    openRoomButton->SetScale(Vec2(210.0f, 70.0f));
    m_pSecondMenu->AddChild(openRoomButton);

    auto enterRoomButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"EnterRoom_Idle", L"texture\\Menu Items\\Enter Room\\Sprite_Label_EnterRoom_0.png"),
        CResMgr::GetInst()->LoadTexture(L"EnterRoom_Hover", L"texture\\Menu Items\\Enter Room\\Sprite_Label_EnterRoom_1.png"));
    enterRoomButton->SetPos(Vec2(290.0f, 600.0f));
    enterRoomButton->SetScale(Vec2(210.0f, 70.0f));
    m_pSecondMenu->AddChild(enterRoomButton);

    auto backButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"GoBack_Idle", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_0.png"),
        CResMgr::GetInst()->LoadTexture(L"GoBack_Hover", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_1.png"));
    backButton->SetPos(Vec2(290.0f, 680.0f));
    backButton->SetScale(Vec2(210.0f, 70.0f));
    backButton->SetClickedCallBack(
        [](DWORD_PTR scene, DWORD_PTR) {
            auto* titleScene = reinterpret_cast<CScene_Title*>(scene);
            titleScene->m_pSecondMenu->SetActive(false);
            titleScene->m_pFirstMenu->SetActive(true);
        },
        reinterpret_cast<DWORD_PTR>(this), (DWORD_PTR)0
    );
    m_pSecondMenu->AddChild(backButton);
    m_pSecondMenu->SetActive(false);
    AddObject(m_pSecondMenu, GROUP_TYPE::UI);
}

