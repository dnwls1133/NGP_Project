#include "PacketCodec.h"

#include <type_traits>
#include <utility>

namespace protocol
{
    namespace
    {
        // =====================================================================
        // [1-A] ByteWriter: C++ 구조체의 숫자들을 네트워크 표준인 Big-Endian(상위 바이트부터)
        //     순서로 쪼개서 std::vector<uint8_t> 배열에 차곡차곡 쌓아주는 도구입니다.
        //     (Winsock의 htons / htonl 역할 + 버퍼에 push_back 하는 역할을 동시에 수행)
        // =====================================================================
        struct ByteWriter
        {
            std::vector<std::uint8_t> bytes;

            // 1바이트 부호 없는 정수(uint8_t)를 그대로 1바이트 추가합니다.
            void WriteU8(std::uint8_t value)
            {
                bytes.push_back(value);
            }

            // 2바이트 정수(uint16_t)를 상위 바이트 -> 하위 바이트 순서(Big-Endian)로 기록합니다.
            // (윈속 htons와 동일한 바이트 순서를 만듭니다)
            void WriteU16(std::uint16_t value)
            {
                bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>(value & 0xFFu));
            }

            // 4바이트 부호 없는 정수(uint32_t)를 최상위 바이트부터 4바이트(Big-Endian)로 기록합니다.
            // 예: 0x01020304 -> [0x01, 0x02, 0x03, 0x04] (윈속 htonl과 동일)
            void WriteU32(std::uint32_t value)
            {
                bytes.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
                bytes.push_back(static_cast<std::uint8_t>(value & 0xFFu));
            }

            // 4바이트 부호 있는 정수(int32_t: 좌표, 속도, HP, 점수 증감량 등)를
            // 2의 보수 비트 손실 없이 uint32_t로 변환하여 Big-Endian 4바이트로 기록합니다.
            void WriteI32(std::int32_t value)
            {
                WriteU32(static_cast<std::uint32_t>(value));
            }

            // 8바이트 부호 없는 정수(uint64_t: UdpToken 등)를 Big-Endian 8바이트로 기록합니다.
            void WriteU64(std::uint64_t value)
            {
                for (int shift = 56; shift >= 0; shift -= 8)
                {
                    bytes.push_back(
                        static_cast<std::uint8_t>((value >> shift) & 0xFFu));
                }
            }

            // 2D 고정소수점 좌표/속도(FixedVec2: x, y 각각 int32_t 4바이트씩 총 8바이트)를 기록합니다.
            void WriteVec2(const FixedVec2& v)
            {
                WriteI32(v.x);
                WriteI32(v.y);
            }
        };

        // =====================================================================
        // [1-B] ByteReader (11/03 추가): 수신한 Big-Endian 바이트 배열에서
        //       앞에서부터 순서대로 숫자를 꺼내 원래 C++ 정수로 조립하는 언박싱 도구입니다.
        //       (Winsock의 ntohs / ntohl 역할 + 버퍼에서 pop_front 하는 역할을 동시에 수행)
        // =====================================================================
        struct ByteReader
        {
            std::span<const std::uint8_t> bytes;
            std::size_t offset = 0; // 현재 읽고 있는 위치

            // 아직 읽지 않고 남아 있는 바이트 수를 반환한다.
            std::size_t Remaining() const noexcept
            {
                return bytes.size() - offset;
            }

            // 1바이트 부호 없는 정수를 꺼내고 offset을 1칸 이동한다.
            std::uint8_t ReadU8() noexcept
            {
                return bytes[offset++];
            }

            // 2바이트 Big-Endian 바이트를 합쳐서 uint16_t로 복원한다. (ntohs와 동일)
            std::uint16_t ReadU16() noexcept
            {
                const std::uint16_t hi = static_cast<std::uint16_t>(bytes[offset++]);
                const std::uint16_t lo = static_cast<std::uint16_t>(bytes[offset++]);
                return static_cast<std::uint16_t>((hi << 8) | lo);
            }

            // 4바이트 Big-Endian 바이트를 합쳐서 uint32_t로 복원한다. (ntohl과 동일)
            std::uint32_t ReadU32() noexcept
            {
                const std::uint32_t b0 = static_cast<std::uint32_t>(bytes[offset++]);
                const std::uint32_t b1 = static_cast<std::uint32_t>(bytes[offset++]);
                const std::uint32_t b2 = static_cast<std::uint32_t>(bytes[offset++]);
                const std::uint32_t b3 = static_cast<std::uint32_t>(bytes[offset++]);
                return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
            }

            // 4바이트 Big-Endian 바이트를 부호 있는 int32_t로 복원한다.
            std::int32_t ReadI32() noexcept
            {
                return static_cast<std::int32_t>(ReadU32());
            }

            // 8바이트 Big-Endian 바이트를 합쳐서 uint64_t로 복원한다.
            std::uint64_t ReadU64() noexcept
            {
                std::uint64_t value = 0;
                for (int i = 0; i < 8; ++i) {
                    value = (value << 8) | static_cast<std::uint64_t>(bytes[offset++]);
                }
                return value;
            }

            // 2D 고정소수점 좌표/속도(x,y 각각 4바이트 씩 총 8바이트)를 복원한다.
            FixedVec2 ReadVec2() noexcept
            {
                FixedVec2 v{};
                v.x = ReadI32();
                v.y = ReadI32();
                return v;
            }



        };

        // =====================================================================
        // [2] 필드 범위 검증 도우미 함수들 (IsValid*)
        //     인코딩하기 전에 구조체 안에 잘못된 값(범위 밖 번호, 쓰레기값 등)이
        //     들어있으면 네트워크로 내보내지 않고 즉시 에러를 반환하기 위한 함수들입니다.
        // =====================================================================

        // 메시지 종류(MessageType) 번호가 1(JOIN) ~ 19(END) 사이인지 확인합니다.
        constexpr bool IsValidMessageTypeValue(std::uint16_t rawType) noexcept
        {
            return rawType >= static_cast<std::uint16_t>(MessageType::JOIN)
                && rawType <= static_cast<std::uint16_t>(MessageType::END);
        }

        // 플레이어 슬롯 번호가 0(호스트 P0) 또는 1(참가자 P1)인지 확인합니다.
        constexpr bool IsValidPlayerSlot(PlayerSlot slot) noexcept
        {
            return slot < MAX_PROTOCOL_PLAYERS;
        }

        // 플레이어 슬롯이 0, 1이거나 '해당 없음(INVALID_PLAYER_SLOT = 0xFF)'인지 확인합니다.
        // (예: 몬스터 스폰 시 ownerPlayerId는 플레이어가 아니므로 0xFF를 사용)
        constexpr bool IsValidOptionalPlayerSlot(PlayerSlot slot) noexcept
        {
            return slot < MAX_PROTOCOL_PLAYERS || slot == INVALID_PLAYER_SLOT;
        }

        // 0 또는 1만 가져야 하는 불리언(bool 역할) 1바이트 필드가 0이나 1인지 확인합니다.
        constexpr bool IsValidBoolU8(std::uint8_t value) noexcept
        {
            return value == 0 || value == 1;
        }

        // 동기화 객체 종류(NetObjectKind)가 1(PLAYER) ~ 5(ITEM) 범위 안인지 확인합니다.
        constexpr bool IsValidNetObjectKind(NetObjectKind kind) noexcept
        {
            const auto raw = static_cast<std::uint8_t>(kind);
            return raw >= static_cast<std::uint8_t>(NetObjectKind::PLAYER)
                && raw <= static_cast<std::uint8_t>(NetObjectKind::ITEM);
        }

        // 접속 거부 사유(RejectReason)가 1(ROOM_FULL) ~ 4(INTERNAL_ERROR) 범위 안인지 확인합니다.
        constexpr bool IsValidRejectReason(RejectReason reason) noexcept
        {
            const auto raw = static_cast<std::uint8_t>(reason);
            return raw >= static_cast<std::uint8_t>(RejectReason::ROOM_FULL)
                && raw <= static_cast<std::uint8_t>(RejectReason::INTERNAL_ERROR);
        }

        // 연결 종료 사유(DisconnectReason)가 0(NORMAL) ~ 5(PROTOCOL_ERROR) 범위 안인지 확인합니다.
        constexpr bool IsValidDisconnectReason(DisconnectReason reason) noexcept
        {
            const auto raw = static_cast<std::uint8_t>(reason);
            return raw <= static_cast<std::uint8_t>(DisconnectReason::PROTOCOL_ERROR);
        }

        // 객체 제거 사유(DespawnReason)가 1(DESTROYED) ~ 4(STAGE_END) 범위 안인지 확인합니다.
        constexpr bool IsValidDespawnReason(DespawnReason reason) noexcept
        {
            const auto raw = static_cast<std::uint8_t>(reason);
            return raw >= static_cast<std::uint8_t>(DespawnReason::DESTROYED)
                && raw <= static_cast<std::uint8_t>(DespawnReason::STAGE_END);
        }

        // 전투 이벤트 종류(CombatEventKind)가 1(DAMAGE_APPLIED) ~ 4(SCORE_CHANGED)인지 확인합니다.
        constexpr bool IsValidCombatEventKind(CombatEventKind kind) noexcept
        {
            const auto raw = static_cast<std::uint8_t>(kind);
            return raw >= static_cast<std::uint8_t>(CombatEventKind::DAMAGE_APPLIED)
                && raw <= static_cast<std::uint8_t>(CombatEventKind::SCORE_CHANGED);
        }

        // 게임 승패 결과(MatchOutcome)가 1(VICTORY) 또는 2(DEFEAT)인지 확인합니다.
        constexpr bool IsValidMatchOutcome(MatchOutcome outcome) noexcept
        {
            return outcome == MatchOutcome::VICTORY
                || outcome == MatchOutcome::DEFEAT;
        }

        // =====================================================================
        // [3] 메시지 종류별 규격 길이(totalBytes) 및 공통 헤더(32바이트) 유효성 검사
        //     PROTOCOL_V1.md 4절에 정의된 판정 우선순위 순서대로 헤더의 각 필드를 검사합니다.
        // =====================================================================

        // 해당 메시지 종류(type)가 가질 수 있는 정상 전체 바이트 길이(totalBytes)인지 확인합니다.

        constexpr bool IsValidTotalBytesForType(MessageType type, std::uint32_t
            totalBytes) noexcept
        {
            switch (type)
            {
            case MessageType::JOIN:
            case MessageType::HEARTBEAT:
                return totalBytes == 32;

            case MessageType::REJECT:
            case MessageType::READY:
            case MessageType::LOAD:
            case MessageType::LOADED:
            case MessageType::UDP_ACK:
            case MessageType::LEAVE:
            case MessageType::END:
                return totalBytes == 36;

            case MessageType::ROOM_STATE:
            case MessageType::PLAYER_INPUT:
                return totalBytes == 40;

            case MessageType::ACCEPT:
            case MessageType::START:
            case MessageType::UDP_REGISTER:
            case MessageType::DESPAWN:
            case MessageType::RESULT:
                return totalBytes == 44;

            case MessageType::COMBAT_EVENT:
                return totalBytes == 60;

            case MessageType::SPAWN:
                return totalBytes == 68;

            case MessageType::STATE_SNAPSHOT:
            {
                // 스냅샷은 최소 40B(헤더 32B + 스냅샷 헤더 8B) 이상이고,
                // (totalBytes - 40)이 객체 레코드 크기(32B)로 나누어떨어져야 합니다.
                constexpr std::uint32_t minSnapshotBytes =
                    static_cast<std::uint32_t>(HEADER_BYTES + SNAPSHOT_HEADER_BYTES);
                if (totalBytes < minSnapshotBytes)
                    return false;
                const std::uint32_t objectBytes = totalBytes - minSnapshotBytes;
                return (objectBytes % OBJECT_STATE_RECORD_BYTES) == 0
                    && (objectBytes / OBJECT_STATE_RECORD_BYTES)
                    <= MAX_SNAPSHOT_OBJECTS_PER_PACKET;
            }

            default:
                return false;
            }
        }

        // (11/05 추가) 메시지 종류에 따른 헤더 sessionId 구조 규칙을 검사합니다.
        // - JOIN: 세션 발급 전 요청이므로 반드시 sessionId == 0이어야 합니다.
        // - REJECT: 발급 전 거절(0) 또는 세션 중 거절 모두 허용 가능합니다.
        // - 그 외 17종 메시지: 승인 이후 메시지이므로 sessionId != 0이어야 합니다.
        // (실제 활성 세션 번호와 일치하는지는 CSessionManager에서 검사합니다)
        constexpr bool IsValidSessionIdForType(MessageType type
            , std::uint32_t sessionId) noexcept
        {
            if (type == MessageType::JOIN)
                return sessionId == 0;
            if (type == MessageType::REJECT)
                return true; // 0 또는 활성 세션 번호 모두 허용
            return sessionId != 0;
        }

        CodecError ValidateHeaderFields(const PacketHeader& header) noexcept
        {
            // 1순위: 매직 넘버가 0x5653("VS")인지 확인
            if (header.magic != MAGIC)
                return CodecError::BAD_MAGIC;

            // 2순위: 프로토콜 버전이 1인지 확인
            if (header.version != VERSION)
                return CodecError::UNSUPPORTED_VERSION;

            // 3순위: 메시지 번호가 1~19 사이인지 확인
            const auto rawType = static_cast<std::uint16_t>(header.type);
            if (!IsValidMessageTypeValue(rawType))
                return CodecError::UNKNOWN_TYPE;

            // 4순위: v1에서 쓰지 않는 flags가 0인지 확인
            if (header.flags != FLAGS_NONE)
                return CodecError::INVALID_FLAGS;

            // 5순위: 예약 필드(reserved)가 0인지 확인
            if (header.reserved != RESERVED_VALUE)
                return CodecError::INVALID_RESERVED;

            // 6순위: 전체 길이가 최소 헤더 크기(32바이트) 이상인지 확인
            if (header.totalBytes < HEADER_BYTES)
                return CodecError::INVALID_LENGTH;

            // 7순위: UDP(1200B) 또는 TCP(65536B) 최대 크기를 넘지 않는지 확인
            if (header.totalBytes > MaxMessageBytesFor(header.type))
                return CodecError::TOO_LARGE;

            // 8순위 (11/04 추가): 메시지 종류별 고정/가변 규격 길이와 totalBytes가 일치하는지 확인
            if (!IsValidTotalBytesForType(header.type, header.totalBytes))
                return CodecError::INVALID_LENGTH;  

            // 9순위: 단일 스테이지(Stage01) 규칙상 roundId가 1인지 확인
            if (header.roundId != 1)
                return CodecError::INVALID_FIELD;

            // 10순위 (11/05 보강): JOIN은 sessionId==0, 승인 후 메시지는 sessionId!=0 구조 규칙 확인
            if (!IsValidSessionIdForType(header.type, header.sessionId))
                return CodecError::INVALID_FIELD;


            return CodecError::NONE;
        }

        // =====================================================================
        // [4] 19종 메시지 본문(Payload)별 검증 및 직렬화 함수 (EncodePayload 오버로딩)
        //     std::visit을 통해 현재 Message.payload에 들어있는 타입에 맞는 함수가 자동 호출됩니다.
        // =====================================================================

        // 1. JOIN: 참가 요청 (본문 없음, 0바이트)
        CodecError EncodePayload(const JoinPayload&, ByteWriter&)
        {
            return CodecError::NONE;
        }

        // 2. ACCEPT: 참가 수락 (슬롯 번호, 최대 인원, 예약 필드, 8바이트 UDP 토큰 = 총 12바이트)
        CodecError EncodePayload(const AcceptPayload& p, ByteWriter& w)
        {
            if (!IsValidPlayerSlot(p.assignedPlayerId)
                || p.maxPlayers != MAX_PROTOCOL_PLAYERS
                || p.reserved != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(p.assignedPlayerId);
            w.WriteU8(p.maxPlayers);
            w.WriteU16(p.reserved);
            w.WriteU64(p.udpToken);
            return CodecError::NONE;
        }

        // 3. REJECT: 참가 거부 (거부 사유 1B + 예약 3B = 총 4바이트)
        CodecError EncodePayload(const RejectPayload& p, ByteWriter& w)
        {
            if (!IsValidRejectReason(p.reason)
                || p.reserved0 != 0
                || p.reserved1 != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(static_cast<std::uint8_t>(p.reason));
            w.WriteU8(p.reserved0);
            w.WriteU16(p.reserved1);
            return CodecError::NONE;
        }

        // 4. READY: 준비 상태 변경 (플레이어ID 1B + 준비여부 1B + 예약 2B = 총 4바이트)
        CodecError EncodePayload(const ReadyPayload& p, ByteWriter& w)
        {
            if (!IsValidPlayerSlot(p.playerId)
                || !IsValidBoolU8(p.isReady)
                || p.reserved != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(p.playerId);
            w.WriteU8(p.isReady);
            w.WriteU16(p.reserved);
            return CodecError::NONE;
        }

        // 5. ROOM_STATE: 방 슬롯 2개의 상태 (슬롯당 4B * 2명 = 총 8바이트)
        CodecError EncodePayload(const RoomStatePayload& p, ByteWriter& w)
        {
            for (std::size_t i = 0; i < p.slots.size(); ++i)
            {
                const auto& slot = p.slots[i];
                if (slot.playerId != static_cast<PlayerSlot>(i)
                    || !IsValidBoolU8(slot.connected)
                    || !IsValidBoolU8(slot.ready)
                    || !IsValidBoolU8(slot.udpRegistered))
                {
                    return CodecError::INVALID_FIELD;
                }
                w.WriteU8(slot.playerId);
                w.WriteU8(slot.connected);
                w.WriteU8(slot.ready);
                w.WriteU8(slot.udpRegistered);
            }
            return CodecError::NONE;
        }

        // 6. LOAD: 스테이지 로딩 지시 (스테이지 번호 2B + 예약 2B = 총 4바이트)
        CodecError EncodePayload(const LoadPayload& p, ByteWriter& w)
        {
            if (p.stageId != 1 || p.reserved != 0)
                return CodecError::INVALID_FIELD;

            w.WriteU16(p.stageId);
            w.WriteU16(p.reserved);
            return CodecError::NONE;
        }

        // 7. LOADED: 스테이지 및 리소스 로딩 완료 보고 (플레이어ID 1B + 예약 3B = 총 4바이트)
        CodecError EncodePayload(const LoadedPayload& p, ByteWriter& w)
        {
            if (!IsValidPlayerSlot(p.playerId)
                || p.reserved0 != 0
                || p.reserved1 != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(p.playerId);
            w.WriteU8(p.reserved0);
            w.WriteU16(p.reserved1);
            return CodecError::NONE;
        }

        // 8. START: 게임 동시 시작 지시 (시작 틱 4B + 시뮬/스냅샷/입력 주기 6B + 예약 2B = 총 12바이트)
        CodecError EncodePayload(const StartPayload& p, ByteWriter& w)
        {
            if (p.simRateHz == 0
                || p.stateRateHz == 0
                || p.inputRateHz == 0
                || p.reserved != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU32(p.startHostTick);
            w.WriteU16(p.simRateHz);
            w.WriteU16(p.stateRateHz);
            w.WriteU16(p.inputRateHz);
            w.WriteU16(p.reserved);
            return CodecError::NONE;
        }

        // 9. UDP_REGISTER: 참가자 UDP 주소 등록 요청 (슬롯 1B + 예약 3B + 토큰 8B = 총 12바이트)
        CodecError EncodePayload(const UdpRegisterPayload& p, ByteWriter& w)
        {
            if (!IsValidPlayerSlot(p.playerId)
                || p.reserved0 != 0
                || p.reserved1 != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(p.playerId);
            w.WriteU8(p.reserved0);
            w.WriteU16(p.reserved1);
            w.WriteU64(p.udpToken);
            return CodecError::NONE;
        }

        // 10. UDP_ACK: UDP 주소 등록 확인 응답 (슬롯 1B + 등록여부 1B + 예약 2B = 총 4바이트)
        CodecError EncodePayload(const UdpAckPayload& p, ByteWriter& w)
        {
            if (!IsValidPlayerSlot(p.playerId)
                || !IsValidBoolU8(p.registered)
                || p.reserved != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(p.playerId);
            w.WriteU8(p.registered);
            w.WriteU16(p.reserved);
            return CodecError::NONE;
        }

        // 11. PLAYER_INPUT: 플레이어 키 입력 전송 (슬롯 1B + 방향키 비트 1B + 발사유지 1B + 예약 1B + 입력순번 4B = 총 8바이트)
        CodecError EncodePayload(const PlayerInputPayload& p, ByteWriter& w)
        {
            // 상하좌우 4개 비트(0x0F) 외의 이상한 비트가 켜져 있으면 거부합니다.
            if (!IsValidPlayerSlot(p.playerId)
                || (p.moveBits & ~MoveBit::ALL_VALID) != 0
                || !IsValidBoolU8(p.fireHeld)
                || p.reserved != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(p.playerId);
            w.WriteU8(p.moveBits);
            w.WriteU8(p.fireHeld);
            w.WriteU8(p.reserved);
            w.WriteU32(p.inputSequence);
            return CodecError::NONE;
        }

        // 12. STATE_SNAPSHOT: 월드 내 객체들의 상태 스냅샷 (메타데이터 8B + 객체당 32B * N개)
        //     UDP 패킷 상한(1200B)을 맞추기 위해 패킷당 최대 36개(1192B)까지만 허용합니다.
        CodecError EncodePayload(const StateSnapshotPayload& p, ByteWriter& w)
        {
            if (p.partCount == 0 || p.partIndex >= p.partCount)
                return CodecError::INVALID_FIELD;

            if (p.objects.size() > MAX_SNAPSHOT_OBJECTS_PER_PACKET)
                return CodecError::TOO_LARGE;

            w.WriteU32(p.snapshotTick);
            w.WriteU8(p.partIndex);
            w.WriteU8(p.partCount);
            w.WriteU16(static_cast<std::uint16_t>(p.objects.size()));

            for (const auto& obj : p.objects)
            {
                if (obj.objectId == 0
                    || !IsValidNetObjectKind(obj.kind)
                    || !IsValidBoolU8(obj.alive))
                {
                    return CodecError::INVALID_FIELD;
                }

                w.WriteU32(obj.objectId);
                w.WriteU8(static_cast<std::uint8_t>(obj.kind));
                w.WriteU8(obj.subType);
                w.WriteU8(obj.alive);
                w.WriteU8(obj.fsmState);
                w.WriteVec2(obj.pos);
                w.WriteVec2(obj.vel);
                w.WriteI32(obj.hp);
                w.WriteI32(obj.maxHp);
            }
            return CodecError::NONE;
        }

        // 13. SPAWN: 몬스터/탄환/아이템 생성 이벤트 (본문 36바이트)
        CodecError EncodePayload(const SpawnPayload& p, ByteWriter& w)
        {
            if (p.eventId == 0
                || p.objectId == 0
                || !IsValidNetObjectKind(p.kind)
                || !IsValidOptionalPlayerSlot(p.ownerPlayerId))
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU32(p.eventId);
            w.WriteU32(p.objectId);
            w.WriteU8(static_cast<std::uint8_t>(p.kind));
            w.WriteU8(p.subType);
            w.WriteU8(p.ownerPlayerId);
            w.WriteU8(p.patternType);
            w.WriteVec2(p.pos);
            w.WriteVec2(p.vel);
            w.WriteI32(p.hp);
            w.WriteI32(p.damage);
            return CodecError::NONE;
        }

        // 14. DESPAWN: 객체 삭제 이벤트 (본문 12바이트)
        CodecError EncodePayload(const DespawnPayload& p, ByteWriter& w)
        {
            if (p.eventId == 0
                || p.objectId == 0
                || !IsValidDespawnReason(p.reason)
                || p.reserved0 != 0
                || p.reserved1 != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU32(p.eventId);
            w.WriteU32(p.objectId);
            w.WriteU8(static_cast<std::uint8_t>(p.reason));
            w.WriteU8(p.reserved0);
            w.WriteU16(p.reserved1);
            return CodecError::NONE;
        }

        // 15. COMBAT_EVENT: 피격/사망/아이템획득/점수변동 확정 이벤트 (본문 28바이트)
        CodecError EncodePayload(const CombatEventPayload& p, ByteWriter& w)
        {
            if (p.eventId == 0
                || !IsValidCombatEventKind(p.kind)
                || !IsValidOptionalPlayerSlot(p.instigatorPlayerId)
                || p.updatedPlayerLevel > 5
                || p.reserved != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU32(p.eventId);
            w.WriteU8(static_cast<std::uint8_t>(p.kind));
            w.WriteU8(p.instigatorPlayerId);
            w.WriteU8(p.updatedPlayerLevel);
            w.WriteU8(p.reserved);
            w.WriteU32(p.sourceObjectId);
            w.WriteU32(p.targetObjectId);
            w.WriteI32(p.confirmedHp);
            w.WriteI32(p.scoreDelta);
            w.WriteU32(p.teamScore);
            return CodecError::NONE;
        }

        // 16. RESULT: 보스 처치(승리) 또는 전원 사망(패배) 결과 통보 (본문 12바이트)
        CodecError EncodePayload(const ResultPayload& p, ByteWriter& w)
        {
            if (p.eventId == 0
                || !IsValidMatchOutcome(p.outcome)
                || p.reserved0 != 0
                || p.reserved1 != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU32(p.eventId);
            w.WriteU8(static_cast<std::uint8_t>(p.outcome));
            w.WriteU8(p.reserved0);
            w.WriteU16(p.reserved1);
            w.WriteU32(p.finalTeamScore);
            return CodecError::NONE;
        }

        // 17. HEARTBEAT: 연결 유지 확인 신호 (본문 없음, 0바이트)
        CodecError EncodePayload(const HeartbeatPayload&, ByteWriter&)
        {
            return CodecError::NONE;
        }

        // 18. LEAVE: 플레이어 퇴장 통보 (본문 4바이트)
        CodecError EncodePayload(const LeavePayload& p, ByteWriter& w)
        {
            if (!IsValidPlayerSlot(p.playerId)
                || !IsValidDisconnectReason(p.reason)
                || p.reserved != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(p.playerId);
            w.WriteU8(static_cast<std::uint8_t>(p.reason));
            w.WriteU16(p.reserved);
            return CodecError::NONE;
        }

        // 19. END: 세션 종료 통보 (본문 4바이트)
        CodecError EncodePayload(const EndPayload& p, ByteWriter& w)
        {
            if (!IsValidDisconnectReason(p.reason)
                || p.reserved0 != 0
                || p.reserved1 != 0)
            {
                return CodecError::INVALID_FIELD;
            }

            w.WriteU8(static_cast<std::uint8_t>(p.reason));
            w.WriteU8(p.reserved0);
            w.WriteU16(p.reserved1);
            return CodecError::NONE;
        }

        // =========================================================================
        // [5] 19종 메시지 본문(payload) 역질렬화 함수 (DecodeMessagePayload, 11/03 추가)
        // EncodePayload와 정반대로, 먼저 본문 바이트 길이가 정확한지 확인(INVALID_LENGTH)한 뒤,
        // ByteReader로 값을 꺼내고 필드 범위를 검사(INVALID_FIELD)합니다.
        CodecError DecodeMessagePayload(
            MessageType type,
            ByteReader& r,
            MessagePayload& outPayload
        )
        {
            switch (type)
            {
            case MessageType::JOIN:
            {
                // JOIN은 본문이 0바이트여야 합니다.
                if (r.Remaining() != 0) return CodecError::INVALID_LENGTH;

                outPayload = JoinPayload{};
                return CodecError::NONE;
            }

            case MessageType::ACCEPT:
            {
                if (r.Remaining() != 12) return CodecError::INVALID_LENGTH;

                AcceptPayload p{};
                p.assignedPlayerId = r.ReadU8();
                p.maxPlayers = r.ReadU8();
                p.reserved = r.ReadU16();
                p.udpToken = r.ReadU64();

                if (!IsValidPlayerSlot(p.assignedPlayerId)
                    || p.maxPlayers != MAX_PROTOCOL_PLAYERS
                    || p.reserved != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::REJECT:
            {
                if (r.Remaining() != 4) return CodecError::INVALID_LENGTH;

                RejectPayload p{};
                p.reason = static_cast<RejectReason>(r.ReadU8());
                p.reserved0 = r.ReadU8();
                p.reserved1 = r.ReadU16();

                if (!IsValidRejectReason(p.reason)
                    || p.reserved0 != 0
                    || p.reserved1 != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;

            }

            case MessageType::READY:
            {
                if (r.Remaining() != 4) return CodecError::INVALID_LENGTH;

                ReadyPayload p{};
                p.playerId = r.ReadU8();
                p.isReady = r.ReadU8();
                p.reserved = r.ReadU16();

                if (!IsValidPlayerSlot(p.playerId)
                    || !IsValidBoolU8(p.isReady)
                    || p.reserved != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::ROOM_STATE:
            {
                if (r.Remaining() != 8) return CodecError::INVALID_LENGTH;

                RoomStatePayload p{};
                for (std::size_t i = 0; i < p.slots.size(); ++i)
                {
                    auto& slot = p.slots[i];
                    slot.playerId = r.ReadU8();
                    slot.connected = r.ReadU8();
                    slot.ready = r.ReadU8();
                    slot.udpRegistered = r.ReadU8();

                    if (slot.playerId != static_cast<PlayerSlot>(i)
                        || !IsValidBoolU8(slot.connected)
                        || !IsValidBoolU8(slot.ready)
                        || !IsValidBoolU8(slot.udpRegistered))
                    {
                        return CodecError::INVALID_FIELD;
                    }
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::LOAD:
            {
                if (r.Remaining() != 4) return CodecError::INVALID_LENGTH;

                LoadPayload p{};
                p.stageId = r.ReadU16();
                p.reserved = r.ReadU16();

                if (p.stageId != 1 || p.reserved != 0)
                {
                    return CodecError::INVALID_FIELD;
                }

                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::LOADED:
            {
                if (r.Remaining() != 4) return CodecError::INVALID_LENGTH;
                LoadedPayload p{};
                p.playerId = r.ReadU8();
                p.reserved0 = r.ReadU8();
                p.reserved1 = r.ReadU16();

                if (!IsValidPlayerSlot(p.playerId)
                    || p.reserved0 != 0
                    || p.reserved1 != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;

            }

            case MessageType::START:
            {
                if (r.Remaining() != 12) return CodecError::INVALID_LENGTH;

                StartPayload p{};
                p.startHostTick = r.ReadU32();
                p.simRateHz = r.ReadU16();
                p.stateRateHz = r.ReadU16();
                p.inputRateHz = r.ReadU16();
                p.reserved = r.ReadU16();

                if (p.simRateHz == 0
                    || p.stateRateHz == 0
                    || p.inputRateHz == 0
                    || p.reserved != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::UDP_REGISTER:
            {
                if (r.Remaining() != 12) return CodecError::INVALID_LENGTH;
                UdpRegisterPayload p{};
                p.playerId = r.ReadU8();
                p.reserved0 = r.ReadU8();
                p.reserved1 = r.ReadU16();
                p.udpToken = r.ReadU64();
                if (!IsValidPlayerSlot(p.playerId)
                    || p.reserved0 != 0
                    || p.reserved1 != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::UDP_ACK:
            {
                if (r.Remaining() != 4) return CodecError::INVALID_LENGTH;
                UdpAckPayload p{};
                p.playerId = r.ReadU8();
                p.registered = r.ReadU8();
                p.reserved = r.ReadU16();
                if (!IsValidPlayerSlot(p.playerId)
                    || !IsValidBoolU8(p.registered)
                    || p.reserved != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::PLAYER_INPUT:
            {
                if (r.Remaining() != 8) return CodecError::INVALID_LENGTH;
                PlayerInputPayload p{};
                p.playerId = r.ReadU8();
                p.moveBits = r.ReadU8();
                p.fireHeld = r.ReadU8();
                p.reserved = r.ReadU8();
                p.inputSequence = r.ReadU32();
                if (!IsValidPlayerSlot(p.playerId)
                    || (p.moveBits & ~MoveBit::ALL_VALID) != 0
                    || !IsValidBoolU8(p.fireHeld)
                    || p.reserved != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::STATE_SNAPSHOT:
            {
                // 스냅샷 본문은 최소 8바이트 이상이어야 합니다.
                if (r.Remaining() < SNAPSHOT_HEADER_BYTES) return CodecError::INVALID_LENGTH;

                StateSnapshotPayload p{};
                p.snapshotTick = r.ReadU32();
                p.partIndex = r.ReadU8();
                p.partCount = r.ReadU8();
                const std::uint16_t objectCount = r.ReadU16();

                // 남은 바이트 수가 (객체 개수 * 32바이트)와 정확히 일치하는지 확인
                const std::size_t expectedObjectBytes =
                    static_cast<std::size_t>(objectCount) * OBJECT_STATE_RECORD_BYTES;

                if (r.Remaining() != expectedObjectBytes) return CodecError::INVALID_LENGTH;

                if (objectCount > MAX_SNAPSHOT_OBJECTS_PER_PACKET) return CodecError::TOO_LARGE;

                if (p.partCount == 0 || p.partIndex >= p.partCount) return CodecError::INVALID_FIELD;

                p.objects.resize(objectCount);
                for (std::uint16_t i = 0; i < objectCount; ++i) {
                    ObjectStateRecord& obj = p.objects[i];
                    obj.objectId = r.ReadU32();
                    obj.kind = static_cast<NetObjectKind>(r.ReadU8());
                    obj.subType = r.ReadU8();
                    obj.alive = r.ReadU8();
                    obj.fsmState = r.ReadU8();
                    obj.pos = r.ReadVec2();
                    obj.vel = r.ReadVec2();
                    obj.hp = r.ReadI32();
                    obj.maxHp = r.ReadI32();

                    if (obj.objectId == 0
                        || !IsValidNetObjectKind(obj.kind)
                        || !IsValidBoolU8(obj.alive))
                    {
                        return CodecError::INVALID_FIELD;
                    }
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::SPAWN:
            {
                if (r.Remaining() != 36) return CodecError::INVALID_LENGTH;

                SpawnPayload p{};
                p.eventId = r.ReadU32();
                p.objectId = r.ReadU32();
                p.kind = static_cast<NetObjectKind>(r.ReadU8());
                p.subType = r.ReadU8();
                p.ownerPlayerId = r.ReadU8();
                p.patternType = r.ReadU8();
                p.pos = r.ReadVec2();
                p.vel = r.ReadVec2();
                p.hp = r.ReadI32();
                p.damage = r.ReadI32();

                if (p.eventId == 0
                    || p.objectId == 0
                    || !IsValidNetObjectKind(p.kind)
                    || !IsValidOptionalPlayerSlot(p.ownerPlayerId))
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::DESPAWN:
            {
                if (r.Remaining() != 12) return CodecError::INVALID_LENGTH;

                DespawnPayload p{};
                p.eventId = r.ReadU32();
                p.objectId = r.ReadU32();
                p.reason = static_cast<DespawnReason>(r.ReadU8());
                p.reserved0 = r.ReadU8();
                p.reserved1 = r.ReadU16();

                if (p.eventId == 0
                    || p.objectId == 0
                    || !IsValidDespawnReason(p.reason)
                    || p.reserved0 != 0
                    || p.reserved1 != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;

            }

            case MessageType::COMBAT_EVENT:
            {
                if (r.Remaining() != 28) return CodecError::INVALID_LENGTH;

                CombatEventPayload p{};
                p.eventId = r.ReadU32();
                p.kind = static_cast<CombatEventKind>(r.ReadU8());
                p.instigatorPlayerId = r.ReadU8();
                p.updatedPlayerLevel = r.ReadU8();
                p.reserved = r.ReadU8();
                p.sourceObjectId = r.ReadU32();
                p.targetObjectId = r.ReadU32();
                p.confirmedHp = r.ReadI32();
                p.scoreDelta = r.ReadI32();
                p.teamScore = r.ReadU32();

                if (p.eventId == 0
                    || !IsValidCombatEventKind(p.kind)
                    || !IsValidOptionalPlayerSlot(p.instigatorPlayerId)
                    || p.updatedPlayerLevel > 5
                    || p.reserved != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::RESULT:
            {
                if (r.Remaining() != 12) return CodecError::INVALID_LENGTH;

                ResultPayload p{};
                p.eventId = r.ReadU32();
                p.outcome = static_cast<MatchOutcome>(r.ReadU8());
                p.reserved0 = r.ReadU8();
                p.reserved1 = r.ReadU16();
                p.finalTeamScore = r.ReadU32();

                if (p.eventId == 0
                    || !IsValidMatchOutcome(p.outcome)
                    || p.reserved0 != 0
                    || p.reserved1 != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::HEARTBEAT:
            {
                if (r.Remaining() != 0) return CodecError::INVALID_LENGTH;
                outPayload = HeartbeatPayload{};
                return CodecError::NONE;
            }

            case MessageType::LEAVE:
            {
                if (r.Remaining() != 4) return CodecError::INVALID_LENGTH;
                LeavePayload p{};
                p.playerId = r.ReadU8();
                p.reason = static_cast<DisconnectReason>(r.ReadU8());
                p.reserved = r.ReadU16();
                if (!IsValidPlayerSlot(p.playerId)
                    || !IsValidDisconnectReason(p.reason)
                    || p.reserved != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            case MessageType::END:
            {
                if (r.Remaining() != 4) return CodecError::INVALID_LENGTH;
                EndPayload p{};
                p.reason = static_cast<DisconnectReason>(r.ReadU8());
                p.reserved0 = r.ReadU8();
                p.reserved1 = r.ReadU16();
                if (!IsValidDisconnectReason(p.reason)
                    || p.reserved0 != 0
                    || p.reserved1 != 0)
                {
                    return CodecError::INVALID_FIELD;
                }
                outPayload = p;
                return CodecError::NONE;
            }

            default:
                return CodecError::UNKNOWN_TYPE;
            }

        }


    }

    

    // =========================================================================
    // [6] CPacketCodec 공개 멤버 함수 구현
    // =========================================================================

    // 32바이트 패킷 헤더만 검증하고 Big-Endian 바이트 배열로 직렬화합니다.
    CodecError CPacketCodec::EncodeHeader(
        const PacketHeader& header,
        std::vector<std::uint8_t>& out)
    {
        // 먼저 헤더 필드들이 규격에 맞는지 검사합니다.
        const CodecError err = ValidateHeaderFields(header);
        if (err != CodecError::NONE)
            return err;

        ByteWriter writer;
        writer.bytes.reserve(HEADER_BYTES);

        // PROTOCOL_V1.md 2절의 바이트 오프셋 순서(총 32바이트)대로 기록합니다.
        writer.WriteU16(header.magic);                          // [0..1]   0x5653 ("VS")
        writer.WriteU16(header.version);                        // [2..3]   1
        writer.WriteU16(static_cast<std::uint16_t>(header.type)); // [4..5] 메시지 종류(1~19)
        writer.WriteU16(header.flags);                          // [6..7]   0
        writer.WriteU32(header.totalBytes);                     // [8..11]  헤더+본문 전체 바이트 수
        writer.WriteU32(header.sessionId);                      // [12..15] 세션 번호
        writer.WriteU32(header.roundId);                        // [16..19] 라운드 번호(1)
        writer.WriteU32(header.sequence);                       // [20..23] 패킷 순번
        writer.WriteU32(header.hostTick);                       // [24..27] 호스트 시뮬레이션 틱
        writer.WriteU32(header.reserved);                       // [28..31] 예약 필드(0)

        // 성공했을 때만 호출자의 출력 벡터(out)를 교체합니다 (실패 시 기존 out 유지).
        out = std::move(writer.bytes);
        return CodecError::NONE;
    }

    // (11/03 구현 ) 바이트 배열에서 32바이트 헤더를 읽고 검증한다.
    // 실패 시 호출자의 out은 변경하지 않는다.
    CodecError CPacketCodec::DecodeHeader(
        std::span<const std::uint8_t> frame,
        PacketHeader& out)
    {
        // 1순위: 입력 바이트가 32바이트 미만이면 즉시 TRUNCATED 반환
        if (frame.size() < HEADER_BYTES) return CodecError::TRUNCATED;

        // 임시 구조체(parsed)에 먼저 32바이트 읽어 들입니다.
        ByteReader reader{ frame.subspan(0, HEADER_BYTES) };
        PacketHeader parsed{};
        parsed.magic = reader.ReadU16();
        parsed.version = reader.ReadU16();
        parsed.type = static_cast<MessageType>(reader.ReadU16());
        parsed.flags = reader.ReadU16();
        parsed.totalBytes = reader.ReadU32();
        parsed.sessionId = reader.ReadU32();
        parsed.roundId = reader.ReadU32();
        parsed.sequence = reader.ReadU32();
        parsed.hostTick = reader.ReadU32();
        parsed.reserved = reader.ReadU32();
    
        // 2~9순위: 헤더 필드 유효성 검사
        const CodecError err = ValidateHeaderFields(parsed);
        if (err != CodecError::NONE) return err;

        // 모든 검사를 통과했을 때만 out에 복사
        out = parsed;
        return CodecError::NONE;
    }

    // 메시지 전체(헤더 32B + 본문 NB)를 검증하고 하나의 Big-Endian 바이트 배열로 직렬화합니다.
    CodecError CPacketCodec::Encode(
        const Message& message,
        std::vector<std::uint8_t>& out)
    {
        // 1단계: 메시지 번호가 1~19 범위 안인지 확인
        const auto rawType = static_cast<std::uint16_t>(message.header.type);
        if (!IsValidMessageTypeValue(rawType))
            return CodecError::UNKNOWN_TYPE;

        // 2단계: header.type(1~19)과 실제 payload(std::variant 인덱스 0~18)의 구조체 종류가 일치하는지 확인
        // 예: 헤더에는 JOIN(1)이라고 적어놓고 본문에는 AcceptPayload(인덱스 1)를 넣었으면 에러 처리
        const std::size_t expectedVariantIndex =
            static_cast<std::size_t>(rawType - 1);
        if (message.payload.index() != expectedVariantIndex)
            return CodecError::INVALID_FIELD;

        // 3단계: 메시지 종류에 맞는 EncodePayload 함수를 호출해 본문(body) 바이트 생성
        ByteWriter bodyWriter;
        const CodecError bodyErr = std::visit(
            [&](const auto& payload) -> CodecError
            {
                return EncodePayload(payload, bodyWriter);
            },
            message.payload);

        if (bodyErr != CodecError::NONE)
            return bodyErr;

        // 4단계: 헤더(32B) + 본문 크기가 UDP(1200B) 또는 TCP(65536B) 상한을 넘지 않는지 확인
        const std::size_t totalSize = HEADER_BYTES + bodyWriter.bytes.size();
        if (totalSize > MaxMessageBytesFor(message.header.type))
            return CodecError::TOO_LARGE;

        // 5단계: 실제 계산된 전체 크기(totalSize)를 헤더의 totalBytes에 채운 뒤 헤더 직렬화
        PacketHeader finalHeader = message.header;
        finalHeader.totalBytes = static_cast<std::uint32_t>(totalSize);

        std::vector<std::uint8_t> encoded;
        const CodecError headerErr = EncodeHeader(finalHeader, encoded);
        if (headerErr != CodecError::NONE)
            return headerErr;

        // 6단계: 직렬화된 32바이트 헤더 뒤에 직렬화된 본문 바이트들을 이어 붙임
        encoded.insert(
            encoded.end(),
            bodyWriter.bytes.begin(),
            bodyWriter.bytes.end());

        // 모든 검증과 조립이 끝났을 때만 호출자의 out 벡터를 교체합니다.
        out = std::move(encoded);
        return CodecError::NONE;
    }

    // (11/03구현) 단일 프레임 바이트 배열을 검증하고 Message 구조체로 복원합니다.
    // 중간에 어떤 에러가 발생하더라도 호출자의 out은 절대 변경되지 않습니다.
    CodecError CPacketCodec::Decode(
        std::span<const std::uint8_t> frame,
        Message& out)
    {
        // 1단계: 앞 32바이트 헤더 역직렬화 및 기본 검증
        if (frame.size() < HEADER_BYTES) return CodecError::TRUNCATED;

        ByteReader headerreader{ frame.subspan(0, HEADER_BYTES) };
        PacketHeader parsedHeader{};
        parsedHeader.magic = headerreader.ReadU16();
        parsedHeader.version = headerreader.ReadU16();
        parsedHeader.type = static_cast<MessageType>(headerreader.ReadU16());
        parsedHeader.flags = headerreader.ReadU16();
        parsedHeader.totalBytes = headerreader.ReadU32();
        parsedHeader.sessionId = headerreader.ReadU32();
        parsedHeader.roundId = headerreader.ReadU32();
        parsedHeader.sequence = headerreader.ReadU32();
        parsedHeader.hostTick = headerreader.ReadU32();
        parsedHeader.reserved = headerreader.ReadU32();

        // PROTOCOL_V1.md 4절의 우선순위에 맞춰 헤더와 프레임 길이검사
        if (parsedHeader.magic != MAGIC) return CodecError::BAD_MAGIC;
        if (parsedHeader.version != VERSION) return CodecError::UNSUPPORTED_VERSION;
        const auto rawType = static_cast<std::uint16_t>(parsedHeader.type);
        if (!IsValidMessageTypeValue(rawType)) return CodecError::UNKNOWN_TYPE;
        if (parsedHeader.flags != FLAGS_NONE) return CodecError::INVALID_FLAGS;
        if (parsedHeader.reserved != RESERVED_VALUE) return CodecError::INVALID_RESERVED;
        if (parsedHeader.totalBytes < HEADER_BYTES) return CodecError::INVALID_LENGTH;
        const std::size_t maxBytes = MaxMessageBytesFor(parsedHeader.type);
        if (parsedHeader.totalBytes > maxBytes || frame.size() > maxBytes) return CodecError::TOO_LARGE;

        if (parsedHeader.totalBytes != frame.size()) return CodecError::INVALID_LENGTH;
        if (!IsValidTotalBytesForType(parsedHeader.type, parsedHeader.totalBytes)) return CodecError::INVALID_LENGTH;
        if (parsedHeader.roundId != 1) return CodecError::INVALID_FIELD;
        if (!IsValidSessionIdForType(parsedHeader.type, parsedHeader.sessionId)) return CodecError::INVALID_FIELD;
        
        // 2단계: 32바이트 이후의 본문(Payload) 바이트 역직렬화 및 범위 검증
        ByteReader bodyReader{ frame.subspan(HEADER_BYTES) };
        MessagePayload parsedPayload{};
        const CodecError bodyErr = DecodeMessagePayload(
            parsedHeader.type,
            bodyReader,
            parsedPayload);

        if (bodyErr != CodecError::NONE) return bodyErr;

        // 3단계: 헤더와 본문이 모두 100% 정상일 때만 호출자의 out에 최종 결과를 옮겨 담습니다.
        out.header = parsedHeader;
        out.payload = std::move(parsedPayload);
        return CodecError::NONE;
    }
}
