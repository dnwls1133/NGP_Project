#pragma once

#include <string>

namespace network_mgr_test
{
    bool Run(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);
    bool RunListeningShutdown(unsigned short tcpPort, unsigned short udpPort);
    bool RunReceiveOverflow(unsigned short tcpPort, unsigned short udpPort);
    bool RunQueueTest(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);
    bool RunSnapshotBatchTest();
}
