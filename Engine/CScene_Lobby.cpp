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

#include <cmath>
#include <iostream>

namespace
{
    void DrawLobbyLogo(HDC canvas, CTexture* logo, const RECT& bounds, int opticalOffsetX = 0)
    {
        assert(canvas);
        assert(logo);

        const int width = static_cast<int>(logo->Width());
        const int height = static_cast<int>(logo->Height());

        const int availableWidth = bounds.right - bounds.left;
        const int availableHeight = bounds.bottom - bounds.top;

        const float scaleX = static_cast<float>(availableWidth) / width;
        const float scaleY = static_cast<float>(availableHeight) / height;
        const float scale = scaleX < scaleY ? scaleX : scaleY;

        const int drawWidth = static_cast<int>(width * scale);
        const int drawHeight = static_cast<int>(height * scale);
        const int drawX = bounds.left + (availableWidth - drawWidth) / 2
            + static_cast<int>(std::lround(opticalOffsetX * scale));
        const int drawY = bounds.top + (availableHeight - drawHeight) / 2;

        TransparentBlt(canvas, drawX, drawY, drawWidth, drawHeight,
            logo->GetDC(), 0, 0, width, height, RGB(255, 0, 255));
    }
}

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
    requestedReady.reset();
    readyAfterJoin = false;


    // 배경 로드
    {
        m_pBackground = CResMgr::GetInst()->LoadTexture(
            L"Ranking Background",
            L"texture\\Sprite_Background_Rankings.png"
        );
        m_pPanel = CResMgr::GetInst()->LoadTexture(
            L"Lobby Panel",
            L"texture\\Panels\\Sprite_Panel_Lobby.png"
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
            -72,
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
    m_pRoomCodeLogo = session.IsHost()
        ? CResMgr::GetInst()->LoadTexture(L"ShareRoomCode", L"texture\\Sprite_Logo_ShareRoomCode.png")
        : CResMgr::GetInst()->LoadTexture(L"EnterRoomCode", L"texture\\Sprite_Logo_EnterRoomCode.png");
    m_pWaitPlayerLogo = CResMgr::GetInst()->LoadTexture(L"WaitPlayer", L"texture\\Sprite_Logo_WaitPlayer.png");
    m_pEnterPlayerLogo = CResMgr::GetInst()->LoadTexture(L"EnterPlayer", L"texture\\Sprite_Logo_EnterPlayer.png");
    m_pWaitDecisionLogo = CResMgr::GetInst()->LoadTexture(L"WaitDecision", L"texture\\Sprite_Logo_WaitDecision.png");

    if (session.IsHost())
    {
        const auto result = session.StartHost(40000, 40001);
        if (result != network::NetworkResult::Success)
            std::cerr << "Room Create Request Fail: " << static_cast<int>(result) << '\n';
    }

    if (!session.IsHost())
        std::cout << "[CLIENT] Enter room code, then press Enter\n";

    const Vec2 buttonSize(210.0f, 70.0f);
    const Vec2 actionPosition(985.0f, 684.0f);

    m_startButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Start Idle", L"texture\\Menu Items\\Start Game\\Sprite_Label_StartGame_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Start Hover", L"texture\\Menu Items\\Start Game\\Sprite_Label_StartGame_1.png")
    );
    m_startButton->SetPos(actionPosition);
    m_startButton->SetScale(buttonSize);
    m_startButton->SetClickedCallBack([](DWORD_PTR context, DWORD_PTR)
        {
            reinterpret_cast<CScene_Lobby*>(context)->SubmitHostStart();
        }, reinterpret_cast<DWORD_PTR>(this), 0
    );
    AddObject(m_startButton, GROUP_TYPE::UI);

    m_readyButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Ready Idle", L"texture\\Menu Items\\Ready Game\\Sprite_Label_ReadyGame_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Ready Hover", L"texture\\Menu Items\\Ready Game\\Sprite_Label_ReadyGame_1.png")
    );
    m_readyButton->SetPos(actionPosition);
    m_readyButton->SetScale(buttonSize);
    m_readyButton->SetClickedCallBack([](DWORD_PTR context, DWORD_PTR)
        {
            reinterpret_cast<CScene_Lobby*>(context)->SubmitClientAction();
        }, reinterpret_cast<DWORD_PTR>(this), 0);
    AddObject(m_readyButton, GROUP_TYPE::UI);

    m_backButton = new CMenuItem(
        CResMgr::GetInst()->LoadTexture(L"Back Idle", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_0.png"),
        CResMgr::GetInst()->LoadTexture(L"Back Hover", L"texture\\Menu Items\\Go Back\\Sprite_Label_GoBack_1.png")
    );
    m_backButton->SetPos(Vec2(983.0f, 784.0f));
    m_backButton->SetScale(buttonSize);
    m_backButton->SetClickedCallBack(
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
    AddObject(m_backButton, GROUP_TYPE::UI);

    UpdateActionButtons();
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

    if (readyAfterJoin && session.GetState() == network::SessionState::UdpRegistered)
    {
        readyAfterJoin = false;
        SubmitReady(true);
    }
    else if (KEY_TAP(KEY::ENTER) ||
        (KEY_TAP(KEY::R) && session.GetState() == network::SessionState::UdpRegistered))
        SubmitClientAction();

    if (KEY_TAP(KEY::S))
        SubmitHostStart();

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

    UpdateActionButtons();

    if (session.GetState() == network::SessionState::Started)
    {
        leavingLobby = true;
        ChangeScene(SCENE_TYPE::STAGE_01);
        return;
    }
}

void CScene_Lobby::SubmitClientAction()
{
    auto& session = CCore::GetInst()->GetSession();
    if (leavingLobby || session.IsHost())
        return;

    using network::SessionState;
    const auto state = session.GetState();
    if (state == SessionState::UdpRegistered)
    {
        SubmitReady(!requestedReady.value_or(session.IsClientReady()));
        return;
    }

    if (state == SessionState::Failed || state == SessionState::Rejected || state == SessionState::Disconnected)
        session.Stop();

    if (session.GetState() != SessionState::Idle)
        return;

    const std::string code(inputRoomCode.begin(), inputRoomCode.end());
    const auto result = session.JoinRoom(code, 40000, 40001);
    readyAfterJoin = result == network::NetworkResult::Success;
    UpdateActionButtons();

    std::cout << "[CLIENT] Join by room code. result=" << static_cast<int>(result) << '\n';

    if (result == network::NetworkResult::InvalidArgument)
        std::cerr << "[CLIENT] Invalid room code\n";
}

void CScene_Lobby::SubmitReady(bool ready)
{
    auto& session = CCore::GetInst()->GetSession();
    const auto result = session.RequestReady(ready);
    if (result == net::SendResult::Queued)
        requestedReady = ready;
    std::cout << "[CLIENT] READY requested=" << ready
        << ", result=" << static_cast<int>(result) << std::endl;
    UpdateActionButtons();
}

void CScene_Lobby::SubmitHostStart()
{
    auto& session = CCore::GetInst()->GetSession();
    if (leavingLobby || !session.IsHost() || session.GetState() != network::SessionState::UdpRegistered)
        return;

    if (!session.IsClientReady())
    {
        std::cout << "[HOST] Waiting for Client ready" << std::endl;
        return;
    }

    const auto result = session.RequestLoad();
    std::cout << "[HOST] LOAD requested. result=" << static_cast<int>(result) << std::endl;
    UpdateActionButtons();
}

void CScene_Lobby::UpdateActionButtons()
{
    const auto& session = CCore::GetInst()->GetSession();
    using network::SessionState;
    const auto state = session.GetState();
    if (state != SessionState::UdpRegistered)
        requestedReady.reset();

    const bool canJoin = state == SessionState::Idle || state == SessionState::Failed ||
        state == SessionState::Rejected || state == SessionState::Disconnected;
    if (canJoin)
        readyAfterJoin = false;

    const bool canReady = state == SessionState::UdpRegistered;

    m_startButton->SetActive(session.IsHost() && state == SessionState::UdpRegistered && session.IsClientReady());
    m_readyButton->SetActive(!session.IsHost() && (canJoin || canReady));
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
            std::cout << ". Press Enter to ready";
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
    const Vec2 resolution = CCore::GetInst()->GetResolution();

    // 배경 그리기
    if (m_pBackground)
    {
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

    constexpr int panelTextureHeight = 1080;
    constexpr int panelSourceTop = 83;
    constexpr int panelSourceBottom = 1057;
    constexpr int panelSourceHeight = panelSourceBottom - panelSourceTop;
    const int panelHeight = static_cast<int>(std::lround(resolution.y * panelSourceHeight / panelTextureHeight));
    const int panelTop = (static_cast<int>(resolution.y) - panelHeight) / 2;
    if (m_pPanel)
    {
        TransparentBlt(_dc, 0, panelTop, static_cast<int>(resolution.x), panelHeight,
            m_pPanel->GetDC(), 0, panelSourceTop, m_pPanel->Width(), panelSourceHeight, RGB(255, 0, 255));
    }

    auto& session = CCore::GetInst()->GetSession();

    // 메시지
    {
        SetBkMode(_dc, TRANSPARENT);
        SetTextColor(_dc, RGB(49, 19, 84));

        HFONT hOld = (HFONT)SelectObject(_dc, m_hFont);

        const LONG logoHalfHeight = static_cast<LONG>(resolution.y * 0.04f);
        RECT messageRect = {
            static_cast<LONG>(resolution.x * 0.1f),
            panelTop - logoHalfHeight,
            static_cast<LONG>(resolution.x * 0.9f),
            panelTop + logoHalfHeight
        };
        RECT addressRect = {
            static_cast<LONG>(resolution.x * 0.1f),
            static_cast<LONG>(resolution.y * 0.49f),
            static_cast<LONG>(resolution.x * 0.9f),
            static_cast<LONG>(resolution.y * 0.61f)
        };

        DrawLobbyLogo(_dc, m_pRoomCodeLogo, messageRect);

        const std::string roomCode = session.GetRoomCode();
        const std::wstring hostCode(roomCode.begin(), roomCode.end());

        LPCTSTR address = session.IsHost()
            ? hostCode.c_str()
            : inputRoomCode.c_str();

        DrawText(_dc, address, -1, &addressRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(_dc, hOld);
    }

    using network::SessionState;
    const auto state = session.GetState();
    constexpr int waitingLogoOpticalOffsetX = 22;
    const RECT statusRect = {
        static_cast<LONG>(resolution.x * 0.1f),
        static_cast<LONG>(resolution.y * 0.65f),
        static_cast<LONG>(resolution.x * 0.9f),
        static_cast<LONG>(resolution.y * 0.73f)
    };

    if (session.IsHost())
    {
        if (state == SessionState::UdpRegistered && session.IsClientReady())
        {
            DrawLobbyLogo(_dc, m_pEnterPlayerLogo, statusRect);
        }
        else if ((state == SessionState::Listening ||
                  state == SessionState::AwaitingJoin ||
                  state == SessionState::AwaitingUdpRegister) || !session.IsClientReady())
        {
            DrawLobbyLogo(_dc, m_pWaitPlayerLogo, statusRect, waitingLogoOpticalOffsetX);
        }
    }
    else if (state == SessionState::UdpRegistered && session.IsClientReady())
    {
        DrawLobbyLogo(_dc, m_pWaitDecisionLogo, statusRect, waitingLogoOpticalOffsetX);
    }

    /*if (state == SessionState::UdpRegistered)
    {
        RECT readyRect = statusRect;
        const HFONT oldFont = static_cast<HFONT>(SelectObject(_dc, m_hFont));
        const wchar_t* readyText = session.IsClientReady() ? L"준비 완료" : L"준비 안 됨";
        DrawText(_dc, readyText, -1, &readyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(_dc, oldFont);
    }*/
    /*
    if (session.IsHost())
    {
        if (state == SessionState::Loading)
            DrawLobbyLogo(_dc, m_pEnterPlayerLogo, statusRect);
        else if (state == SessionState::Listening || state == SessionState::AwaitingJoin ||
            state == SessionState::AwaitingUdpRegister)
            DrawLobbyLogo(_dc, m_pWaitPlayerLogo, statusRect, waitingLogoOpticalOffsetX);
    }
    else if (state == SessionState::Starting || state == SessionState::AwaitingAccept ||
        state == SessionState::AwaitingUdpAck)
    {
        DrawLobbyLogo(_dc, m_pWaitDecisionLogo, statusRect, waitingLogoOpticalOffsetX);
    }
    */

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

    /*const bool separator = character == L'-' && inputRoomCode.size() == 4;
    const bool hasSeparator = inputRoomCode.find(L'-') != std::wstring::npos;
    const std::size_t maxLength = hasSeparator || separator ? 9 : 8;*/

    if (hexDigit && inputRoomCode.size() < 4)
        inputRoomCode.push_back(static_cast<wchar_t>(character));
}
