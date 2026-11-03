#include "PacketCodec.h"

namespace protocol
{
    CodecError CPacketCodec::EncodeHeader(
        const PacketHeader& header,
        std::vector<std::uint8_t>& out)
    {
        (void)header;
        (void)out;
        return CodecError::UNSUPPORTED_MESSAGE;
    }

    CodecError CPacketCodec::DecodeHeader(
        std::span<const std::uint8_t> frame,
        PacketHeader& out)
    {
        (void)frame;
        (void)out;
        return CodecError::UNSUPPORTED_MESSAGE;
    }

    CodecError CPacketCodec::Encode(
        const Message& message,
        std::vector<std::uint8_t>& out)
    {
        (void)message;
        (void)out;
        return CodecError::UNSUPPORTED_MESSAGE;
    }

    CodecError CPacketCodec::Decode(
        std::span<const std::uint8_t> frame,
        Message& out)
    {
        (void)frame;
        (void)out;
        return CodecError::UNSUPPORTED_MESSAGE;
    }
}
