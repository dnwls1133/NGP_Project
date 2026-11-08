#include "PongTest.h"

#include "../Engine/define.h"
#include "../Engine/CNetworkMgr.h"
#include "../Engine/CSessionMgr.h"
#include "PongSimulation.h"

#include <Windows.h>
#include <chrono>
#include <conio.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <algorithm>
#include <sstream>
#include <vector>


namespace pong_test
{
    using network::SessionState;
    using GameClock = std::chrono::steady_clock;

    struct GameRuntime
    {
        World world{};
        std::array<Input, 2> inputs{};

        bool initialized = false;
        bool hasRemoteInput = false;
        bool hasSnapshot = false;

        std::uint32_t nextInputSequence = 0;
        std::uint32_t ticksSinceSnapshot = 0;

        double simulationElapsed = 0.0;
        double inputElapsed = 0.0;

        GameClock::time_point previousFrame{};
        GameClock::time_point lastRemoteInput{};
    };

    const char* GetStateText(SessionState state)
    {
        switch (state)
        {
        case SessionState::Idle:                return "Idle";
        case SessionState::Starting:            return "Starting network";
        case SessionState::Listening:           return "Waiting for Client";
        case SessionState::AwaitingJoin:        return "Waiting for JOIN";
        case SessionState::AwaitingAccept:      return "Waiting for approval";
        case SessionState::AwaitingUdpRegister: return "Waiting for UDP registration";
        case SessionState::AwaitingUdpAck:      return "Waiting for UDP ACK";
        case SessionState::UdpRegistered:       return "Lobby connected";
        case SessionState::Loading:             return "Preparing game";
        case SessionState::Started:             return "Playing";
        case SessionState::Rejected:            return "Join rejected";
        case SessionState::Failed:              return "Network failed";
        case SessionState::Disconnected:        return "Peer disconnected";
        }

        return "Unknown";
    }

    void PresentConsoleFrame(const std::string& text)
    {
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_SCREEN_BUFFER_INFO info{};

        if (output == nullptr || output == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(output, &info))
            return;

        const SHORT width = info.srWindow.Right - info.srWindow.Left + 1;
        const SHORT height = info.srWindow.Bottom - info.srWindow.Top + 1;

        if (width <= 0 || height <= 0)
            return;

        CHAR_INFO blank{};
        blank.Char.AsciiChar = ' ';
        blank.Attributes = info.wAttributes;

        std::vector<CHAR_INFO> buffer(
            static_cast<std::size_t>(width) * height, blank);

        int x = 0;
        int y = 0;

        for (const char ch : text)
        {
            if (ch == '\r')
                continue;

            if (ch == '\n')
            {
                x = 0;
                ++y;

                if (y >= height)
                    break;

                continue;
            }

            if (x < width && y < height)
            {
                const auto index =
                    static_cast<std::size_t>(y) * width + x;

                buffer[index].Char.AsciiChar = ch;
            }

            ++x;
        }

        SMALL_RECT destination = info.srWindow;

        WriteConsoleOutputA(
            output,
            buffer.data(),
            COORD{ width, height },
            COORD{ 0, 0 },
            &destination);
    }

    void DrawGame(std::ostream& out, const World& world)
    {
        std::array<std::string, FIELD_HEIGHT> rows{};

        for (auto& row : rows)
            row.assign(FIELD_WIDTH, ' ');

        for (const auto& paddle : world.paddles)
        {
            const int x = std::clamp(
                static_cast<int>(paddle.position.x / POSITION_SCALE),
                0,
                FIELD_WIDTH - 1);

            const int top = std::clamp(
                static_cast<int>(paddle.position.y / POSITION_SCALE),
                0,
                FIELD_HEIGHT - PADDLE_HEIGHT);

            for (int offset = 0; offset < PADDLE_HEIGHT; ++offset)
                rows[top + offset][x] = '#';
        }

        const std::string border = "+" + std::string(FIELD_WIDTH, '-') + "+";

        out << "       HOST P0                  CLIENT P1\n";
        out << border << '\n';

        for (const auto& row : rows)
            out << '|' << row << "|\n";

        out << border << '\n';
    }

    void DrawScreen(const network::CSessionMgr& session, const std::string& notice, const World& world)
    {
        std::ostringstream frame;

        const auto state = session.GetState();

        frame << "=== PONG CUI ===\n";
        frame << "Role : " << (session.IsHost() ? "HOST" : "CLIENT") << '\n';
        frame << "State: " << GetStateText(state) << '\n';

        if (session.IsHost())
            frame << "Room code: " << session.GetRoomCode() << '\n';
        else
            frame << '\n';

        if (state == SessionState::Started)
        {
            DrawGame(frame, world);

            frame
                << "[W] Up / [S] Down\n"
                << "Host tick: " << world.hostTick << '\n'
                << "P0.y: " << world.paddles[0].position.y
                << "  P1.y: " << world.paddles[1].position.y
                << '\n';
        }
        else if (state == SessionState::UdpRegistered)
        {
            const bool ready = session.IsClientReady();

            frame << "Client ready: " << (ready ? "YES" : "NO") << '\n';

            if (session.IsHost())
            {
                frame << (ready
                    ? "[S] Start game\n"
                    : "Waiting for Client ready...\n");
            }
            else
            {
                frame << "[R] Toggle ready\n";
            }
        }

        if (state == SessionState::Failed ||
            state == SessionState::Disconnected)
        {
            const auto& event = session.GetLastNetworkEvent();

            frame
                << "socketError=" << event.socketError
                << ", codecError=" << static_cast<int>(event.codecError)
                << '\n';
        }

        if (state == SessionState::Rejected)
        {
            frame
                << "rejectReason="
                << static_cast<int>(session.GetRejectReason())
                << '\n';
        }

        if (!notice.empty())
            frame << notice << '\n';

        if (!session.IsHost() &&
            (state == SessionState::Failed ||
                state == SessionState::Rejected ||
                state == SessionState::Disconnected))
        {
            frame << "[ENTER] Enter another room code\n";
        }

        frame << "[ESC] Return to menu\n";

        PresentConsoleFrame(frame.str());
    }

    int ReadKey()
    {
        const int key = _getch();

        if (key == 0 || key == 224)
        {
            _getch();
            return 0;
        }

        return key;
    }

    bool ReadRoomCode(std::string& code)
    {
        code.clear();
        std::cout << "Room code ([ESC] Menu): " << std::flush;

        while (true)
        {
            int key = ReadKey();

            if (key == 27)
            {
                std::cout << '\n';
                return false;
            }

            if (key == '\r')
            {
                std::cout << '\n';
                return true;
            }

            if (key == '\b')
            {
                if (!code.empty())
                {
                    code.pop_back();
                    std::cout << "\b \b" << std::flush;
                }

                continue;
            }

            if (key >= 'a' && key <= 'f')
                key -= 'a' - 'A';

            const bool hexDigit =
                (key >= '0' && key <= '9') ||
                (key >= 'A' && key <= 'F');

            const bool separator = key == '-' && code.size() == 4;
            const bool hasSeparator = code.find('-') != std::string::npos;
            const std::size_t maxLength = hasSeparator || separator ? 9 : 8;

            if ((hexDigit || separator) && code.size() < maxLength)
            {
                code.push_back(static_cast<char>(key));
                std::cout << static_cast<char>(key) << std::flush;
            }
        }
    }

    bool JoinRoomWithPrompt(network::CSessionMgr& session, unsigned short tcpPort, unsigned short udpPort)
    {
        // 이전 접속의 스레드·소켓·세션 상태를 먼저 정리
        session.Stop();

        std::system("cls");
        std::cout << "=== JOIN ROOM ===\n\n";

        while (true)
        {
            std::string code;
            if (!ReadRoomCode(code))
                return false;

            const auto result = session.JoinRoom(code, tcpPort, udpPort);
            if (result == network::NetworkResult::Success)
                return true;

            if (result == network::NetworkResult::InvalidArgument)
                std::cout << "Invalid room code. Please try again.\n\n";
            else
                std::cout << "Join request failed. result="
                << static_cast<int>(result) << "\n\n";
        }
    }

    bool BeginGame(const network::CSessionMgr& session, GameRuntime& game)
    {
        const auto& start = session.GetStartInfo();

        // 현재 Pong 계산 함수는 60Hz 기준으로 작성되어 있다.
        if (start.simRateHz != SIMULATION_RATE_HZ ||
            start.inputRateHz != 30 ||
            start.stateRateHz != 20)
        {
            return false;
        }

        game = {};
        game.world = MakeInitialWorld();
        game.world.hostTick = start.startHostTick;
        game.previousFrame = GameClock::now();

        // Client는 시작 직후 첫 입력을 바로 등록한다.
        game.inputElapsed = 1.0 / start.inputRateHz;
        game.initialized = true;

        return true;
    }

    Input ReadMovementInput()
    {
        Input input{};

        // 키를 누르고 있는 동안 true.
        input.up = (GetAsyncKeyState('W') & 0x8000) != 0;
        input.down = (GetAsyncKeyState('S') & 0x8000) != 0;

        return input;
    }

    void ReceiveGameMessages(network::CSessionMgr& session, GameRuntime& game)
    {
        network::ReceivedMessage received{};

        while (session.TryReceiveGame(received))
        {
            if (session.IsHost())
            {
                Input input{};

                // 현재 2인 규격에서 Client 슬롯은 1.
                if (!DecodeInput(received.message, session.GetSessionId(), 1, input))
                    continue;

                if (game.hasRemoteInput &&
                    input.sequence <= game.inputs[1].sequence)
                {
                    continue;
                }

                game.inputs[1] = input;
                game.hasRemoteInput = true;
                game.lastRemoteInput = GameClock::now();
            }
            else
            {
                World snapshot{};

                if (!DecodeSnapshot(received.message, session.GetSessionId(), snapshot))
                    continue;

                if (snapshot.hostTick < session.GetStartInfo().startHostTick)
                    continue;

                if (game.hasSnapshot &&
                    snapshot.hostTick <= game.world.hostTick)
                {
                    continue;
                }

                game.world = snapshot;
                game.hasSnapshot = true;
            }
        }
    }

    bool SendClientInput(network::CSessionMgr& session, GameRuntime& game, std::string& notice)
    {
        Input input = ReadMovementInput();
        input.sequence = ++game.nextInputSequence;

        protocol::Message message{};

        if (!EncodeInput(input, session.GetSessionId(), session.GetLocalPlayerId(), message))
        {
            notice = "EncodeInput failed.";
            return false;
        }

        const auto result = session.EnqueueSend(net::Channel::Udp, message);

        if (result != net::SendResult::Queued &&
            result != net::SendResult::ReplacedPending)
        {
            notice = "Input enqueue failed. result=" +
                std::to_string(static_cast<int>(result));
            return false;
        }

        game.inputs[1] = input;
        return true;
    }

    bool SendHostSnapshot(network::CSessionMgr& session, const GameRuntime& game, std::string& notice)
    {
        net::SnapshotBatch batch{};

        if (!EncodeSnapshot(game.world, session.GetSessionId(), batch))
        {
            notice = "EncodeSnapshot failed.";
            return false;
        }

        const auto result = session.EnqueueSnapshotBatch(batch);

        if (result != net::SendResult::Queued &&
            result != net::SendResult::ReplacedPending)
        {
            notice = "Snapshot enqueue failed. result=" +
                std::to_string(static_cast<int>(result));
            return false;
        }

        return true;
    }

    bool UpdateGame(network::CSessionMgr& session, GameRuntime& game, std::string& notice)
    {
        if (!game.initialized && !BeginGame(session, game))
        {
            notice = "Unsupported game rates.";
            return false;
        }

        const auto now = GameClock::now();
        const double elapsed =
            std::chrono::duration<double>(now - game.previousFrame).count();

        game.previousFrame = now;

        ReceiveGameMessages(session, game);

        if (!session.IsHost())
        {
            constexpr double INPUT_INTERVAL = 1.0 / 30.0;
            game.inputElapsed += elapsed;

            if (game.inputElapsed >= INPUT_INTERVAL)
            {
                // 지연 후 과거 입력을 여러 개 몰아서 보내지 않는다.
                game.inputElapsed -= INPUT_INTERVAL;
                if (game.inputElapsed >= INPUT_INTERVAL)
                    game.inputElapsed = 0.0;

                if (!SendClientInput(session, game, notice))
                    return false;
            }

            return true;
        }

        game.inputs[0] = ReadMovementInput();

        // 오래된 입력의 순번은 유지하고 이동 상태만 중립으로 만든다.
        if (!game.hasRemoteInput ||
            GameClock::now() - game.lastRemoteInput >= std::chrono::milliseconds(300))
        {
            game.inputs[1].up = false;
            game.inputs[1].down = false;
        }

        constexpr double TICK_INTERVAL = 1.0 / SIMULATION_RATE_HZ;
        constexpr int MAX_TICKS_PER_UPDATE = 5;
        constexpr std::uint32_t TICKS_PER_SNAPSHOT = SIMULATION_RATE_HZ / 20;

        game.simulationElapsed += elapsed;

        int processedTicks = 0;

        while (game.simulationElapsed >= TICK_INTERVAL &&
            processedTicks < MAX_TICKS_PER_UPDATE)
        {
            SimulateTick(game.world, game.inputs);

            game.simulationElapsed -= TICK_INTERVAL;
            ++processedTicks;
            ++game.ticksSinceSnapshot;

            if (game.ticksSinceSnapshot >= TICKS_PER_SNAPSHOT)
            {
                if (!SendHostSnapshot(session, game, notice))
                    return false;

                game.ticksSinceSnapshot = 0;
            }
        }

        // 처리하지 못한 시간은 다음 Update에 남겨둔다.
        return true;
    }

    void RunRoom( network::CSessionMgr& session, unsigned short tcpPort, unsigned short udpPort)
    {
        const bool host = session.IsHost();

        bool firstFrame = true;
        bool previousReady = false;
        auto previousState = SessionState::Idle;
        std::string notice;
        GameRuntime game{};
        auto lastGameDraw = GameClock::now();

        while (true)
        {
            session.Update();
            bool redraw = false;

            while (_kbhit())
            {
                const int key = ReadKey();

                if (key == 27)
                    return;

                const auto state = session.GetState();

                const bool canRetry = !host && (state == SessionState::Failed || state == SessionState::Rejected || state == SessionState::Disconnected);

                if (key == '\r' && canRetry)
                {
                    if (!JoinRoomWithPrompt(session, tcpPort, udpPort))
                        return;

                    game = {};
                    lastGameDraw = GameClock::now();

                    firstFrame = true;
                    notice.clear();
                    redraw = true;
                    break;
                }

                if (state != SessionState::UdpRegistered)
                    continue;

                if (!host && (key == 'r' || key == 'R'))
                {
                    const auto result = session.RequestReady(!session.IsClientReady());

                    notice = result == net::SendResult::Queued
                        ? "Ready request queued. Waiting for ROOM_STATE."
                        : "Ready request failed.";

                    redraw = true;
                }

                if (host && (key == 's' || key == 'S'))
                {
                    const auto result = session.RequestLoad();

                    notice = result == net::SendResult::Queued
                        ? "LOAD queued. Preparing game."
                        : "Cannot start. Client must be ready.";

                    redraw = true;
                }
            }

            if (session.GetState() == SessionState::Loading)
            {
                const bool loaded = session.NotifyLocalLoaded();

                if (host && loaded && session.HasClientLoaded())
                {
                    constexpr std::uint32_t TEST_START_TICK = 120;
                    const auto result = session.RequestStart(TEST_START_TICK);

                    if (result != net::SendResult::Queued)
                    {
                        notice = "START request failed.";
                        redraw = true;
                    }
                }
            }

            if (session.GetState() == SessionState::Started)
            {
                if (!UpdateGame(session, game, notice))
                {
                    session.Stop();

                    std::cout
                        << "\n" << notice
                        << "\nPress any key to return to menu.\n";

                    ReadKey();
                    return;
                }

                // 게임 중에는 상태 변경이 없어도 주기적으로 화면을 갱신한다.
                const auto now = GameClock::now();

                if (now - lastGameDraw >= std::chrono::milliseconds(33))
                {
                    redraw = true;
                    lastGameDraw = now;
                }
            }

            const auto state = session.GetState();
            const bool ready = session.IsClientReady();

            if (firstFrame || state != previousState || ready != previousReady)
            {
                if (!redraw)
                    notice.clear();

                redraw = true;
            }

            if (redraw)
                DrawScreen(session, notice, game.world);

            firstFrame = false;
            previousState = state;
            previousReady = ready;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    bool Run(unsigned short tcpPort, unsigned short udpPort)
    {
        while (true)
        {
            std::system("cls");
            std::cout
                << "=== PONG CUI ===\n\n"
                << "[1] Create room\n"
                << "[2] Join room\n"
                << "[0] Exit\n\n"
                << "Select: " << std::flush;

            const int selection = ReadKey();

            if (selection == '0' || selection == 27)
                return true;

            if (selection != '1' && selection != '2')
                continue;

            network::CSessionMgr session(*network::CNetworkMgr::GetInst());

            if (selection == '1')
            {
                const auto result = session.StartHost(tcpPort, udpPort);

                if (result != network::NetworkResult::Success)
                {
                    std::cout <<
                        "\nCreate room failed. result=" << static_cast<int>(result) <<
                        "\nPress any key to return to menu.\n";

                    ReadKey();
                    continue;
                }
            }
            else if (!JoinRoomWithPrompt(session, tcpPort, udpPort))
            {
                continue;
            }

            RunRoom(session, tcpPort, udpPort);

            session.Stop();
        }
    }
}
