#pragma once

#include <string>

namespace network_mgr_test
{
    enum class TestMode { Basic, SnapshotBatch };

    bool Run(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort, TestMode mode = TestMode::Basic);
    bool RunListeningShutdown(unsigned short tcpPort, unsigned short udpPort);
    bool RunActiveShutdown(unsigned short tcpPort, unsigned short udpPort);
    bool RunReceiveOverflow(unsigned short tcpPort, unsigned short udpPort);
    bool RunQueueTest(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);
    bool RunSessionApproval(bool host, const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);
    bool RunSessionBadTokenClient(const std::string& hostIp, unsigned short tcpPort, unsigned short udpPort);
    bool RunSnapshotBatchTest();
}
