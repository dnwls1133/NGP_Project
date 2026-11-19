#pragma once

#include <string>

namespace session_data_test
{
    enum class Mode
    {
        Data,
        Spawn,
        Lifecycle,
        Events,
        DuplicateEvents
    };

    bool Run(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort, Mode mode = Mode::Data);
    bool RunSnapshotReplayTest();
    bool Compare(const std::string& hostPath, const std::string& clientPath);
}
