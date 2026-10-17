#include "pch.h"
#include "CScene_Manual.h"
#include "CCore.h"
#include "CRankMgr.h"
#include "CMenuItem.h";
#include "CTexture.h"
#include "CSound.h"
#include "CSoundMgr.h"
#include "CResMgr.h"
#include "CColliderMgr.h"

CScene_Manual::CScene_Manual()
{
   
}

CScene_Manual::~CScene_Manual()
{

}

void CScene_Manual::Enter()
{
    m_pBackground = CResMgr::GetInst()->LoadTexture(
        L"Manuel Background",
        L"texture\\Sprite_Background_Manual.png"
    );

    m_pPanel = CResMgr::GetInst()->LoadTexture(
        L"Manuel Panel",
        L"texture\\Panels\\Sprite_Panel_Manual.png"
    );

    m_pBackButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Back Idle", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Back Hover", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_1.png")
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

    m_pTheme = CResMgr::GetInst()->LoadSound(L"Menu Theme", L"sound\\BGM\\BGM_MenuTheme.mp3");
    CSoundMgr::GetInst()->PlayBGM(m_pTheme, true);
}

void CScene_Manual::render(HDC canvas_)
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

void CScene_Manual::Exit()
{
    DeleteAll();
    CColliderMgr::GetInst()->Reset();
}
