#include "pch.h"
#include "CScene_Credits.h"

#include "CCore.h"
#include "CTexture.h"
#include "CResMgr.h"
#include "CColliderMgr.h"
#include "CSound.h"
#include "CSoundMgr.h"
#include "CMenuItem.h"

CScene_Credits::CScene_Credits()
{
    m_pBackground = CResMgr::GetInst()->LoadTexture(
        L"Credits Background",
        L"texture\\Main Title\\Sprite_Background_MainTitle_0.png"
    );

    m_pPanel = CResMgr::GetInst()->LoadTexture(
        L"Credits Panel",
        L"texture\\Panels\\Sprite_Panel_Credits.png"
    );

    m_pTheme = CResMgr::GetInst()->LoadSound(
        L"Credits Theme",
        L"sound\\BGM\\BGM_CreditsTheme.mp3"
    );
}

CScene_Credits::~CScene_Credits()
{
    
}

void CScene_Credits::Enter()
{
    auto m_pBackButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Go To Title Idle", L"texture\\Menu Items\\Back To Title\\Sprite_Label_BackToTitle_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Go To Title Hover", L"texture\\Menu Items\\Back To Title\\Sprite_Label_BackToTitle_1.png")
    );
    m_pBackButton->SetPos(Vec2(940.0f, 770.0f));
    m_pBackButton->SetScale(Vec2(210.0f, 70.0f));
    m_pBackButton->SetClickedCallBack(
        [](DWORD_PTR, DWORD_PTR) {
            ChangeScene(SCENE_TYPE::TITLE);
        },
        (DWORD_PTR)0, (DWORD_PTR)0
    );
    AddObject(m_pBackButton, GROUP_TYPE::UI);

    CSoundMgr::GetInst()->PlayBGM(m_pTheme, false);
}

void CScene_Credits::render(HDC canvas_)
{
    {
        const Vec2 resolution = CCore::GetInst()->GetResolution();
        const Vec2 position = (Vec2(resolution.x / 2.0f, resolution.y / 2.0f));   // 중심 위치
        const Vec2 scale = (Vec2(resolution.x, resolution.y));    // 출력 크기 (픽셀 단위)

        const int width = m_pBackground->Width();
        const int height = m_pBackground->Height();

        const int drawWidth = static_cast<int>(scale.x);
        const int drawHeight = static_cast<int>(scale.y);

        const int drawX = static_cast<int>(position.x - drawWidth / 2.0f);
        const int drawY = static_cast<int>(position.y - drawHeight / 2.0f);

        // 투명색 지정 (예: RGB(255, 0, 255) - 보라색)
        COLORREF transparentColor = RGB(255, 0, 255);

        // 백그라운드 그리기
        TransparentBlt(
            canvas_,
            drawX, drawY,
            drawWidth, drawHeight,              // 출력 크기
            m_pBackground->GetDC(),
            0, 0,
            width, height,                      // 원본 크기
            transparentColor
        );
    }

    {
        const Vec2 resolution = CCore::GetInst()->GetResolution();
        const Vec2 position = (Vec2(resolution.x / 2.0f, resolution.y / 2.0f));   // 중심 위치
        const Vec2 scale = (Vec2(resolution.x, resolution.y));    // 출력 크기 (픽셀 단위)

        const int width = m_pPanel->Width();
        const int height = m_pPanel->Height();

        const int drawWidth = static_cast<int>(scale.x);
        const int drawHeight = static_cast<int>(scale.y);

        const int drawX = static_cast<int>(position.x - drawWidth / 2.0f);
        const int drawY = static_cast<int>(position.y - drawHeight / 2.0f);

        // 투명색 지정 (예: RGB(255, 0, 255) - 보라색)
        COLORREF transparentColor = RGB(255, 0, 255);

        // 패널 그리기
        TransparentBlt(
            canvas_,
            drawX, drawY,
            drawWidth, drawHeight,              // 출력 크기
            m_pPanel->GetDC(),
            0, 0,
            width, height,                      // 원본 크기
            transparentColor
        );
    }

    CScene::render(canvas_);
}

void CScene_Credits::Exit()
{
    CRankMgr::GetInst()->AddRanking();
    // CRankMgr::GetInst()->SaveRanking();

    DeleteAll();
    CColliderMgr::GetInst()->Reset();
}
