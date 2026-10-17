#include "pch.h"
#include "CScene_Ranking.h"
#include "CCore.h"
#include "CRankMgr.h"
#include "CMenuItem.h";
#include "CTexture.h"
#include "CSound.h"
#include "CSoundMgr.h"
#include "CResMgr.h"
#include "CColliderMgr.h"

CScene_Ranking::CScene_Ranking()
{
    AddFontResourceEx(L"fonts\\BMDOHYEON_ttf.ttf", FR_PRIVATE, NULL);
    AddFontResourceEx(L"fonts\\BMDOHYEON_ttf.ttf", FR_PRIVATE, NULL);

    m_hScoreFont = CreateFont(
        -80,            // height (픽셀 기준, 숫자를 키우면 글자가 커짐)
        0,             // width (0이면 자동)
        0, 0,          // escapement, orientation
        FW_BOLD,       // 굵기
        FALSE, FALSE, FALSE, // italic, underline, strikeout
        HANGUL_CHARSET,// charset
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"배달의민족 도현"    // 글꼴 이름
    );

    m_hDateFont = CreateFont(
        -50,            // height (픽셀 기준, 숫자를 키우면 글자가 커짐)
        0,             // width (0이면 자동)
        0, 0,          // escapement, orientation
        FW_BOLD,       // 굵기
        FALSE, FALSE, FALSE, // italic, underline, strikeout
        HANGUL_CHARSET,// charset
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"배달의민족 도현"    // 글꼴 이름
    );

    //assert(m_hFont);
}

CScene_Ranking::~CScene_Ranking()
{
    RemoveFontResourceEx(L"fonts\\RetroGaming.ttf", FR_PRIVATE, NULL);
    DeleteObject(m_hScoreFont);
    DeleteObject(m_hDateFont);
}

void CScene_Ranking::Enter()
{
    m_pBackground = CResMgr::GetInst()->LoadTexture(
        L"Ranking Background",
        L"texture\\Sprite_Background_Rankings.png"
    );

    m_pPanel = CResMgr::GetInst()->LoadTexture(
       L"Ranking Panel",
       L"texture\\Panels\\Sprite_Panel_Rankings.png"
    );

    auto m_pBackButton = new CMenuItem(
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

void CScene_Ranking::render(HDC canvas_)
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

        {
            SetBkMode(canvas_, TRANSPARENT);
            SetTextColor(canvas_, RGB(255, 215, 0));

            HFONT hOld = (HFONT)SelectObject(canvas_, m_hScoreFont);

            std::wstring first = ToString(GetRanking(0).Score);
            TextOut(canvas_, 300, 250, first.c_str(), first.length());

            SetTextColor(canvas_, RGB(192, 192, 192));

            std::wstring second = ToString(GetRanking(1).Score);
            TextOut(canvas_, 300, 460, second.c_str(), second.length());

            SetTextColor(canvas_, RGB(205, 127, 50));

            std::wstring third = ToString(GetRanking(2).Score);
            TextOut(canvas_, 300, 700, third.c_str(), third.length());

            SelectObject(canvas_, hOld);
        }
        {
            SetBkMode(canvas_, TRANSPARENT);
            SetTextColor(canvas_, RGB(0, 0, 0));

            HFONT hOld = (HFONT)SelectObject(canvas_, m_hDateFont);

            std::wstring first = GetRanking(0).DateTime;
            TextOut(canvas_, 300, 350, first.c_str(), first.length());

            std::wstring second = GetRanking(1).DateTime.c_str();
            TextOut(canvas_, 300, 560, second.c_str(), second.length());

            std::wstring third = GetRanking(2).DateTime.c_str();
            TextOut(canvas_, 300, 800, third.c_str(), third.length());

            SelectObject(canvas_, hOld);
        }
    }

    CScene::render(canvas_);
}

void CScene_Ranking::Exit()
{
    DeleteAll();
    CColliderMgr::GetInst()->Reset();
}
