#pragma once

// 소켓 없이 바이트 조각을 직접 입력하는 TCP 프레이밍 단위 시험.
// 실제 TCP 송수신을 통한 통합 시험 추가 필요
bool RunTcpFramingTests();


// 실제 TCP 연결에서 Host→Client, Client→Host 순서로 프레이밍 시험.
// 호출 전 WSAStartup 필요.
bool RunTcpFramingNetworkTests(bool host, unsigned short port);
