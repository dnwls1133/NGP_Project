#pragma once

#include <cstdint>

bool RunHost(unsigned short tcpPort, unsigned short udpPort, std::uint32_t repeats = 10, bool rejectJoin = false);
