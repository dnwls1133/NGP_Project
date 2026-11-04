#pragma once

namespace scripted_peer
{
    enum class Behavior
    {
        Close,   // 연결 직후 종료
        Silent   // 연결 후 아무 메시지도 보내지 않음
    };

    // WinSock 초기화·종료는 main에서 담당한다.
    bool Run(bool asHost, Behavior behavior, unsigned short tcpPort);
}
