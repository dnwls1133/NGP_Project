#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ProtocolTypes.h"

namespace protocol
{
    // 무상태 패킷 코덱:
    // - 입력 버퍼를 보관하지 않으며 스레드에 안전하다.
    // - 성공(CodecError::NONE)했을 때만 out을 교체하고, 실패 시 기존 out 값을 유지한다.
    // - Decode는 TCP 프레이밍이 완료된 단일 메시지 바이트열(frame)을 입력받는다.
    class CPacketCodec
    {
    public:
        static CodecError EncodeHeader(const PacketHeader& header, std::vector<std::uint8_t>& out);

        static CodecError DecodeHeader(std::span<const std::uint8_t> frame, PacketHeader& out);

        static CodecError Encode(const Message& message, std::vector<std::uint8_t>& out);

        static CodecError Decode(std::span<const std::uint8_t> frame, Message& out);

    };
}
