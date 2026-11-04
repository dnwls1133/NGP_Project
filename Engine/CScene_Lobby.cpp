#include "pch.h"
#include "CScene_Lobby.h"

#include "CCore.h"
#include "CKeyMgr.h"
#include "CMenuItem.h"
#include "CPathMgr.h"
#include "CResMgr.h"
#include "CSoundMgr.h"
#include "CTexture.h"

TCHAR curAddressStr[256] = {};
int curAddressStrIndex = 0;
LOBBY_MODE lobbyMode = LOBBY_MODE::GUEST;

CScene_Lobby::CScene_Lobby() 
    : m_pBackground(nullptr)
    , m_pTheme(nullptr)
    , m_hFont(nullptr)
{
    SetName(TEXT("Lobby"));
}

void CScene_Lobby::Enter()
{
    curAddressStr[0] = TEXT('\0');
    curAddressStrIndex = 0;
    m_hostAddress.clear();

    auto* backButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Back Idle", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Back Hover", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_1.png")
    );
    backButton->SetPos(Vec2(940.0f, 770.0f));
    backButton->SetScale(Vec2(210.0f, 70.0f));
    backButton->SetClickedCallBack(
        [](DWORD_PTR, DWORD_PTR) {
            ChangeScene(SCENE_TYPE::TITLE);
        },
        (DWORD_PTR)0, (DWORD_PTR)0
    );
    AddObject(backButton, GROUP_TYPE::UI);

    // 배경 로드
    {
        m_pBackground = CResMgr::GetInst()->LoadTexture(
            L"Ranking Background",
            L"texture\\Sprite_Background_Rankings.png"
        );
    }
    // 배경 음악 로드
    {
        m_pTheme = CResMgr::GetInst()->LoadSound(L"Menu Theme", L"sound\\BGM\\BGM_MenuTheme.mp3");
        CSoundMgr::GetInst()->PlayBGM(m_pTheme, true);
    }
    // 폰트 로드
    {
        const std::wstring fontPath = std::wstring(CPathMgr::GetInst()->GetContentPath()) + L"font\\BMDOHYEON_ttf.ttf";
        AddFontResourceEx(fontPath.c_str(), FR_PRIVATE, NULL);

        m_hFont = CreateFont(
            -48,
            0,
            0,
            0,
            FW_BOLD,
            FALSE, FALSE, FALSE,
            HANGUL_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"배달의민족 도현"
        );
    }

    if (m_pTheme)
    {
        CSoundMgr::GetInst()->PlayBGM(m_pTheme, true);
    }
}

void CScene_Lobby::update()
{
    CScene::update();

    if (KEY_TAP(KEY::ESC))
    {
        ChangeScene(SCENE_TYPE::TITLE);
    }
}

void CScene_Lobby::GetIPAddress(WPARAM character)
{
    if (lobbyMode != LOBBY_MODE::GUEST)
    {
        return;
    }

    if (character == VK_BACK)
    {
        if (curAddressStrIndex > 0)
        {
            curAddressStr[--curAddressStrIndex] = TEXT('\0');
        }
    }
    else if ((character >= TEXT('0') && character <= TEXT('9')) || character == TEXT('.'))
    {
        if (curAddressStrIndex < 15)
        {
            curAddressStr[curAddressStrIndex++] = static_cast<TCHAR>(character);
            curAddressStr[curAddressStrIndex] = TEXT('\0');
        }
    }
}

void CScene_Lobby::render(HDC _dc)
{
    // 배경 그리기
    if (m_pBackground)
    {
        const Vec2 resolution = CCore::GetInst()->GetResolution();
        const Vec2 position = (Vec2(resolution.x / 2.0f, resolution.y / 2.0f));
        const Vec2 scale = (Vec2(resolution.x, resolution.y));
        const int width = m_pBackground->Width();
        const int height = m_pBackground->Height();
        const int drawWidth = static_cast<int>(scale.x);
        const int drawHeight = static_cast<int>(scale.y);
        const int drawX = static_cast<int>(position.x - drawWidth / 2.0f);
        const int drawY = static_cast<int>(position.y - drawHeight / 2.0f);
        COLORREF transparentColor = RGB(255, 0, 255);
        TransparentBlt(
            _dc,
            drawX, drawY,
            drawWidth, drawHeight,
            m_pBackground->GetDC(),
            0, 0,
            width, height,
            transparentColor
        );
    }

    // 메시지
    {
        SetBkMode(_dc, TRANSPARENT);
        SetTextColor(_dc, RGB(255, 215, 0));

        HFONT hOld = (HFONT)SelectObject(_dc, m_hFont);

        const Vec2 resolution = CCore::GetInst()->GetResolution();
        RECT messageRect = {
            static_cast<LONG>(resolution.x * 0.1f),
            static_cast<LONG>(resolution.y * 0.38f),
            static_cast<LONG>(resolution.x * 0.9f),
            static_cast<LONG>(resolution.y * 0.46f)
        };
        RECT addressRect = {
            static_cast<LONG>(resolution.x * 0.1f),
            static_cast<LONG>(resolution.y * 0.51f),
            static_cast<LONG>(resolution.x * 0.9f),
            static_cast<LONG>(resolution.y * 0.59f)
        };

        LPCTSTR message = lobbyMode == LOBBY_MODE::HOST
            ? TEXT("함께할 유저에게 주소를 공유하세요!")
            : TEXT("함께할 유저의 주소를 입력해주세요");
        LPCTSTR address = lobbyMode == LOBBY_MODE::HOST
            ? (m_hostAddress.empty() ? TEXT("???") : m_hostAddress.c_str())
            : curAddressStr;
        DrawText(_dc, message, -1, &messageRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        DrawText(_dc, address, -1, &addressRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(_dc, hOld);
    }

    CScene::render(_dc);
}

void CScene_Lobby::Exit()
{
    DeleteAll();

    // 폰트 해제
    if (m_hFont)
    {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
}
