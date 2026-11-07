#include "pch.h"
#include "CScene_Lobby.h"

#include "CCore.h"
#include "CKeyMgr.h"
#include "CMenuItem.h"
#include "CPathMgr.h"
#include "CResMgr.h"
#include "CSoundMgr.h"
#include "CTexture.h"

#include "CNetworkMgr.h"
#include "CSessionMgr.h"

#include <iostream>

CScene_Lobby::CScene_Lobby()
    : m_pBackground(nullptr)
    , m_pTheme(nullptr)
    , m_hFont(nullptr)
{
    SetName(TEXT("Lobby Scene"));
}

void CScene_Lobby::Enter()
{
    inputRoomCode.clear();
    leavingLobby = false;

    auto* backButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Back Idle", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Back Hover", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_1.png")
    );
    backButton->SetPos(Vec2(940.0f, 770.0f));
    backButton->SetScale(Vec2(210.0f, 70.0f));
    backButton->SetClickedCallBack(
        [](DWORD_PTR context, DWORD_PTR)
        {
            auto* lobby = reinterpret_cast<CScene_Lobby*>(context);
            lobby->leavingLobby = true;

            CCore::GetInst()->GetSession().Stop();
            std::cout << "[LOBBY] Leaving room\n";
            ChangeScene(SCENE_TYPE::TITLE);
        },
        reinterpret_cast<DWORD_PTR>(this), 0
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

#ifdef _DEBUG
    hasSessionLog = false;
#endif

    auto& session = CCore::GetInst()->GetSession();

    if (session.IsHost())
    {
        const auto result = session.StartHost(40000, 40001);
        if (result != network::NetworkResult::Success)
            std::cerr << "Room Create Request Fail: " << static_cast<int>(result) << '\n';
    }

    if (!session.IsHost())
        std::cout << "[CLIENT] Enter room code, then press Enter\n";
}

void CScene_Lobby::update()
{
    if (leavingLobby)
        return;

    CScene::update();

    ResumeHostWaiting();
    LogSessionState();

    if (KEY_TAP(KEY::ESC))
    {
        leavingLobby = true;
        CCore::GetInst()->GetSession().Stop();
        std::cout << "[LOBBY] Back. Session Quit\n";
        ChangeScene(SCENE_TYPE::TITLE);
        return;
    }

    auto& session = CCore::GetInst()->GetSession();

    if (KEY_TAP(KEY::ENTER) && !session.IsHost())
    {
        const auto state = session.GetState();
        if (state == network::SessionState::Failed || state == network::SessionState::Rejected || state == network::SessionState::Disconnected)
            session.Stop();

        if (session.GetState() != network::SessionState::Idle)
            return;

        const std::string code(inputRoomCode.begin(), inputRoomCode.end());
        const auto result = session.JoinRoom(code, 40000, 40001);

        std::cout << "[CLIENT] Join by room code. result=" << static_cast<int>(result) << '\n';

        if (result == network::NetworkResult::InvalidArgument)
            std::cerr << "[CLIENT] Invalid room code\n";
    }

    if (KEY_TAP(KEY::R) &&
        session.GetState() == network::SessionState::UdpRegistered && !session.IsHost())
    {
        const bool ready = !session.IsClientReady();
        const auto result = session.RequestReady(ready);

        std::cout << "[CLIENT] READY requested=" << ready
            << ", result=" << static_cast<int>(result) << '\n';
    }

    if (KEY_TAP(KEY::S) &&
        session.IsHost() && session.GetState() == network::SessionState::UdpRegistered)
    {
        const auto result = session.RequestLoad();
        std::cout << "[HOST] LOAD requested. result=" << static_cast<int>(result) << '\n';
    }

    if (session.GetState() == network::SessionState::Loading)
    {
        // 실제 로딩 완료 API가 정해지기 전까지 시험용으로 완료 처리
        session.NotifyLocalLoaded();

        if (session.IsHost() && session.HasClientLoaded())
        {
            constexpr std::uint32_t TEST_START_TICK = 120;
            const auto result = session.RequestStart(TEST_START_TICK);

            std::cout << "[HOST] START requested. result=" << static_cast<int>(result) << '\n';
        }
    }
}

void CScene_Lobby::LogSessionState()
{
#ifdef _DEBUG
    const auto& session = CCore::GetInst()->GetSession();
    const auto state = session.GetState();
    const bool ready = session.IsClientReady();

    if (hasSessionLog && state == loggedState && ready == loggedReady)
        return;

    using network::SessionState;

    const char* description = "NONE";
    switch (state)
    {
    case SessionState::Idle:                description = "Session stopped"; break;
    case SessionState::Starting:            description = "Starting network"; break;
    case SessionState::Listening:           description = "Room created. Waiting for Client"; break;
    case SessionState::AwaitingJoin:        description = "TCP connected. Waiting for JOIN"; break;
    case SessionState::AwaitingAccept:      description = "JOIN queued. Waiting for approval"; break;
    case SessionState::AwaitingUdpRegister: description = "Join approved. Waiting for UDP registration"; break;
    case SessionState::AwaitingUdpAck:      description = "UDP registration queued. Waiting for ACK"; break;
    case SessionState::UdpRegistered:       description = "UDP registered. Lobby connection complete"; break;
    case SessionState::Loading:             description = "Loading game"; break;
    case SessionState::Started:             description = "START confirmed. Ready to enter gameplay"; break;
    case SessionState::Rejected:            description = "Join rejected"; break;
    case SessionState::Failed:              description = "Network failed"; break;
    case SessionState::Disconnected:        description = "Peer disconnected"; break;
    }

    const char* role = session.IsHost() ? "HOST" : "CLIENT";
    std::cout << '[' << role << "] " << description;

    if (state == SessionState::UdpRegistered)
    {
        std::cout << ", clientReady=" << ready;

        if (session.IsHost())
            std::cout << (ready ? ". Press S to start" : ". Waiting for Client ready");
        else
            std::cout << ". Press R to toggle ready";
    }

    if (state == SessionState::Failed || state == SessionState::Disconnected)
    {
        const auto& event = session.GetLastNetworkEvent();
        std::cout << ", socketError=" << event.socketError << ", codecError=" << static_cast<int>(event.codecError);
    }

    if (state == SessionState::Rejected)
        std::cout << ", reason=" << static_cast<int>(session.GetRejectReason());

    std::cout << '\n';

    if (state == SessionState::Listening && session.IsHost())
        std::cout << ", roomCode=" << session.GetRoomCode();

    hasSessionLog = true;
    loggedState = state;
    loggedReady = ready;
#endif
}

void CScene_Lobby::ResumeHostWaiting()
{
    auto& session = CCore::GetInst()->GetSession();

    if (!session.IsHost() || session.GetState() != network::SessionState::Disconnected)
        return;

    std::cout << "[HOST] Client left. Restarting room listener\n";

    session.Stop();

    const auto result = session.StartHost(session.GetTcpPort(), session.GetUdpPort());
    if (result != network::NetworkResult::Success)
        std::cerr << "[HOST] Room restart failed. result=" << static_cast<int>(result) << '\n';
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

    auto& session = CCore::GetInst()->GetSession();

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

        

        LPCTSTR message = session.IsHost()
            ? TEXT("함께할 유저에게 코드를 공유하세요!")
            : TEXT("함께할 유저의 코드를 입력해주세요");

        const std::string roomCode = session.GetRoomCode();
        const std::wstring hostCode(roomCode.begin(), roomCode.end());

        LPCTSTR address = session.IsHost()
            ? hostCode.c_str()
            : inputRoomCode.c_str();

        DrawText(_dc, message, -1, &messageRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        DrawText(_dc, address, -1, &addressRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(_dc, hOld);
    }

    if (session.GetState() == network::SessionState::UdpRegistered)
    {
        if (session.IsHost())
        {
            // Client 레디 상태 표시
            // session.IsClientReady()이면 시작 입력 활성화
        }
        else
        {
            // 레디 입력 활성화
            // session.RequestReady(true) 호출
        }
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

void CScene_Lobby::GetRoomCodeInput(WPARAM character)
{
    const auto& session = CCore::GetInst()->GetSession();

    if (leavingLobby || session.IsHost())
        return;

    if (character == VK_BACK)
    {
        if (!inputRoomCode.empty())
            inputRoomCode.pop_back();

        return;
    }

    if (character >= L'a' && character <= L'f')
        character -= L'a' - L'A';

    const bool hexDigit =
        (character >= L'0' && character <= L'9') ||
        (character >= L'A' && character <= L'F');

    const bool separator = character == L'-' && inputRoomCode.size() == 4;
    const bool hasSeparator = inputRoomCode.find(L'-') != std::wstring::npos;
    const std::size_t maxLength = hasSeparator || separator ? 9 : 8;

    if ((hexDigit || separator) && inputRoomCode.size() < maxLength)
        inputRoomCode.push_back(static_cast<wchar_t>(character));
}
