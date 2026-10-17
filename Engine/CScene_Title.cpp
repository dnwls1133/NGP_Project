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

#include "CTitleLogo.h"
#include "CTitleBackground.h"
#include "CRankMgr.h"
// include "CPanelUI.h"

CScene_Title::CScene_Title()
    : CScene()
{
    
}

CScene_Title::~CScene_Title()
{

}

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

    m_pBackground->PlayIntroAnimation();
    m_CurrentState = EState::Intro;
}

void CScene_Title::update()
{
    CScene::update();

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

            auto m_pStartButton = new CMenuItem(
                CResMgr::GetInst()->LoadTexture(L"StartGame_Idle", L"texture\\Menu Items\\Start Game\\Sprite_Label_StartGame_0.png"),
                CResMgr::GetInst()->LoadTexture(L"StartGame_Hover", L"texture\\Menu Items\\Start Game\\Sprite_Label_StartGame_1.png")
            );
            m_pStartButton->SetPos(Vec2(290.0f, 520.0f));
            m_pStartButton->SetScale(Vec2(210.0f, 70.0f));
            m_pStartButton->SetClickedCallBack(
                [](DWORD_PTR, DWORD_PTR) {
                    ChangeScene(SCENE_TYPE::STAGE_01);
                },
                (DWORD_PTR)0, (DWORD_PTR)0
            );
            AddObject(m_pStartButton, GROUP_TYPE::UI);

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
            AddObject(m_pManuelButton, GROUP_TYPE::UI);

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
            AddObject(m_pRankingButton, GROUP_TYPE::UI);

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
            AddObject(m_pExitButton, GROUP_TYPE::UI);

            return;
        }
    }
}

void CScene_Title::Exit()
{
    DeleteAll();
    CColliderMgr::GetInst()->Reset();
}
