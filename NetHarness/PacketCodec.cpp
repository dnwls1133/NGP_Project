#include "PacketCodec.h"

#include <bit>
#include <type_traits>
#include <utility>

namespace protocol
{
    namespace
    {
        // big-endian
        struct Writer
        {
            static constexpr bool reading = false;

            std::vector<std::uint8_t> bytes;
            bool ok = true;

            template<typename T>
            void Value(T& value)
            {
                if constexpr (std::is_enum_v<T>)
                {
                    auto raw = static_cast<std::underlying_type_t<T>>(value);

                    Value(raw);
                }
                else
                {
                    static_assert(std::is_integral_v<T>);

                    using U = std::make_unsigned_t<T>;
                    const auto raw = static_cast<U>(value);

                    for (int shift = static_cast<int>((sizeof(T) - 1) * 8); shift >= 0; shift -= 8)
                    {
                        bytes.push_back(static_cast<std::uint8_t>(raw >> shift));
                    }
                }
            }

            template<typename... T>
            void operator()(T&... values)
            {
                (Value(values), ...);
            }
        };

        // big-endian
        struct Reader
        {
            static constexpr bool reading = true;

            std::span<const std::uint8_t> bytes;
            std::size_t position = 0;
            bool ok = true;

            template<typename T>
            void Value(T& value)
            {
                if (!ok)
                    return;

                if constexpr (std::is_enum_v<T>)
                {
                    std::underlying_type_t<T> raw{};
                    Value(raw);

                    if (ok)
                        value = static_cast<T>(raw);
                }
                else
                {
                    static_assert(std::is_integral_v<T>);

                    if (bytes.size() - position < sizeof(T))
                    {
                        ok = false;
                        return;
                    }

                    std::uint64_t raw = 0;

                    for (std::size_t i = 0; i < sizeof(T); ++i)
                        raw = (raw << 8) | bytes[position++];

                    using U = std::make_unsigned_t<T>;
                    const U bits = static_cast<U>(raw);

                    if constexpr (std::is_signed_v<T>)
                        value = std::bit_cast<T>(bits);
                    else
                        value = bits;
                }
            }

            template<typename... T>
            void operator()(T&... values)
            {
                (Value(values), ...);
            }
        };

        template<typename A>
        void Fields(A& a, PacketHeader& h)
        {
            a(h.magic, h.version, h.type, h.flags, h.totalBytes, h.sessionId, h.roundId, h.sequence, h.hostTick, h.reserved);
        }

        template<typename A>
        void Fields(A&, JoinPayload&)
        {
        }

        template<typename A>
        void Fields(A& a, AcceptPayload& p)
        {
            a(p.assignedPlayerId, p.maxPlayers, p.reserved, p.udpToken);
        }

        template<typename A>
        void Fields(A& a, RejectPayload& p)
        {
            a(p.reason, p.reserved0, p.reserved1);
        }

        template<typename A>
        void Fields(A& a, ReadyPayload& p)
        {
            a(p.playerId, p.isReady, p.reserved);
        }

        template<typename A>
        void Fields(A& a, RoomStatePayload& p)
        {
            for (auto& slot : p.slots)
            {
                a(slot.playerId, slot.connected, slot.ready, slot.udpRegistered);
            }
        }

        template<typename A>
        void Fields(A& a, LoadPayload& p)
        {
            a(p.stageId, p.reserved);
        }

        template<typename A>
        void Fields(A& a, LoadedPayload& p)
        {
            a(p.playerId, p.reserved0, p.reserved1);
        }

        template<typename A>
        void Fields(A& a, StartPayload& p)
        {
            a(p.startHostTick, p.simRateHz, p.stateRateHz, p.inputRateHz, p.reserved);
        }

        template<typename A>
        void Fields(A& a, UdpRegisterPayload& p)
        {
            a(p.playerId, p.reserved0, p.reserved1, p.udpToken);
        }

        template<typename A>
        void Fields(A& a, UdpAckPayload& p)
        {
            a(p.playerId, p.registered, p.reserved);
        }

        template<typename A>
        void Fields(A& a, PlayerInputPayload& p)
        {
            a(p.playerId, p.moveBits, p.fireHeld, p.reserved, p.inputSequence);
        }

        template<typename A>
        void Fields(A& a, ObjectStateRecord& p)
        {
            a(p.objectId, p.kind, p.subType, p.alive, p.fsmState, p.pos.x, p.pos.y, p.vel.x, p.vel.y, p.hp, p.maxHp);
        }

        template<typename A>
        void Fields(A& a, StateSnapshotPayload& p)
        {
            std::uint16_t count = 0;

            if constexpr (!A::reading)
            {
                if (p.objects.size() > MAX_SNAPSHOT_OBJECTS_PER_PACKET)
                {
                    a.ok = false;
                    return;
                }

                count = static_cast<std::uint16_t>(p.objects.size());
            }

            a(p.snapshotTick, p.partIndex, p.partCount, count);

            if (!a.ok)
                return;

            if (count > MAX_SNAPSHOT_OBJECTS_PER_PACKET || p.partCount == 0 || p.partIndex >= p.partCount)
            {
                a.ok = false;
                return;
            }

            if constexpr (A::reading)
                p.objects.resize(count);

            for (auto& object : p.objects)
                Fields(a, object);
        }

        template<typename A>
        void Fields(A& a, SpawnPayload& p)
        {
            a(p.eventId, p.objectId, p.kind, p.subType, p.ownerPlayerId, p.patternType, p.pos.x, p.pos.y, p.vel.x, p.vel.y, p.hp, p.damage);
        }

        template<typename A>
        void Fields(A& a, DespawnPayload& p)
        {
            a(p.eventId, p.objectId, p.reason, p.reserved0, p.reserved1);
        }

        template<typename A>
        void Fields(A& a, CombatEventPayload& p)
        {
            a(p.eventId, p.kind, p.instigatorPlayerId, p.updatedPlayerLevel, p.reserved, p.sourceObjectId, p.targetObjectId, p.confirmedHp, p.scoreDelta, p.teamScore);
        }

        template<typename A>
        void Fields(A& a, ResultPayload& p)
        {
            a(p.eventId, p.outcome, p.reserved0, p.reserved1, p.finalTeamScore);
        }

        template<typename A>
        void Fields(A&, HeartbeatPayload&)
        {
        }

        template<typename A>
        void Fields(A& a, LeavePayload& p)
        {
            a(p.playerId, p.reason, p.reserved);
        }

        template<typename A>
        void Fields(A& a, EndPayload& p)
        {
            a(p.reason, p.reserved0, p.reserved1);
        }

        CodecError CheckHeader(const PacketHeader& header)
        {
            if (header.magic != MAGIC)
                return CodecError::BAD_MAGIC;

            if (header.version != VERSION)
                return CodecError::UNSUPPORTED_VERSION;

            const auto type = static_cast<std::uint16_t>(header.type);

            if (type < 1 || type > static_cast<std::uint16_t>(MessageType::END))
                return CodecError::UNKNOWN_TYPE;

            if (header.flags != 0)
                return CodecError::INVALID_FLAGS;

            if (header.reserved != 0)
                return CodecError::INVALID_RESERVED;

            if (header.totalBytes < HEADER_BYTES)
                return CodecError::INVALID_LENGTH;

            if (header.totalBytes > MaxMessageBytesFor(header.type))
                return CodecError::TOO_LARGE;

            if (header.roundId != 1)
                return CodecError::INVALID_FIELD;

            if (header.type == MessageType::JOIN && header.sessionId != 0)
                return CodecError::INVALID_FIELD;

            return CodecError::NONE;
        }

        template<std::size_t I = 0>
        bool SelectPayload( std::size_t index, MessagePayload& payload)
        {
            if constexpr (I == std::variant_size_v<MessagePayload>)
            {
                return false;
            }
            else
            {
                if (index == I)
                {
                    payload.emplace<I>();
                    return true;
                }

                return SelectPayload<I + 1>(index, payload);
            }
        }
    }

    CodecError CPacketCodec::EncodeHeader( const PacketHeader& header, std::vector<std::uint8_t>& out)
    {
        const auto error = CheckHeader(header);

        if (error != CodecError::NONE)
            return error;

        PacketHeader copy = header;
        Writer writer;

        Fields(writer, copy);

        out = std::move(writer.bytes);
        return CodecError::NONE;
    }

    CodecError CPacketCodec::DecodeHeader( std::span<const std::uint8_t> frame, PacketHeader& out)
    {
        if (frame.size() < HEADER_BYTES)
            return CodecError::TRUNCATED;

        PacketHeader header{};
        Reader reader{ frame.first(HEADER_BYTES) };

        Fields(reader, header);

        if (!reader.ok)
            return CodecError::TRUNCATED;

        const auto error = CheckHeader(header);

        if (error != CodecError::NONE)
            return error;

        out = header;
        return CodecError::NONE;
    }

    CodecError CPacketCodec::Encode( const Message& message, std::vector<std::uint8_t>& out)
    {
        const auto type = static_cast<std::uint16_t>(message.header.type);

        if (type < 1 || type > static_cast<std::uint16_t>(MessageType::END))
        {
            return CodecError::UNKNOWN_TYPE;
        }

        if (message.payload.index() != static_cast<std::size_t>(type - 1))
        {
            return CodecError::INVALID_FIELD;
        }

        Message copy = message;
        Writer body;

        std::visit(
            [&](auto& payload)
            {
                Fields(body, payload);
            },
            copy.payload);

        if (!body.ok)
            return CodecError::INVALID_FIELD;

        PacketHeader header = message.header;
        header.totalBytes = static_cast<std::uint32_t>( HEADER_BYTES + body.bytes.size());

        std::vector<std::uint8_t> encoded;

        const auto error = EncodeHeader(header, encoded);

        if (error != CodecError::NONE)
            return error;

        encoded.insert( encoded.end(), body.bytes.begin(), body.bytes.end());

        // 성공했을 때만 호출자의 출력값 교체。
        out = std::move(encoded);
        return CodecError::NONE;
    }

    CodecError CPacketCodec::Decode( std::span<const std::uint8_t> frame, Message& out)
    {
        PacketHeader header{};

        const auto error = DecodeHeader(frame, header);

        if (error != CodecError::NONE)
            return error;

        if (frame.size() != header.totalBytes)
            return CodecError::INVALID_LENGTH;

        Message message{};
        message.header = header;

        const auto index = static_cast<std::size_t>( static_cast<std::uint16_t>(header.type) - 1);

        if (!SelectPayload(index, message.payload))
            return CodecError::UNKNOWN_TYPE;

        Reader body{ frame.subspan(HEADER_BYTES) };

        std::visit(
            [&](auto& payload)
            {
                Fields(body, payload);
            },
            message.payload);

        if (!body.ok || body.position != body.bytes.size())
            return CodecError::INVALID_LENGTH;

        // 성공했을 때만 호출자의 출력값 교체。
        out = std::move(message);
        return CodecError::NONE;
    }
}
