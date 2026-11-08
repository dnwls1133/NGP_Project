#pragma once

#include <string>

namespace session_data_test
{
    bool Run(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);
    bool Compare(const std::string& hostPath, const std::string& clientPath);
}
