#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace protocol
{
    // ─────────────────────────────
    // 공통 규격
    // ─────────────────────────────

    // 전송 바이트로는 0x56, 0x53이며 ASCII "VS"에 해당한다.
    inline constexpr std::uint16_t MAGIC = 0x5653;

    inline constexpr std::uint16_t VERSION = 1;

    // 헤더를 포함한 길이다.
    inline constexpr std::size_t HEADER_BYTES = 32;
    inline constexpr std::size_t MAX_UDP_BYTES = 1'200;
    inline constexpr std::size_t MAX_TCP_BYTES = 65'536;

    // v1에서는 부가 플래그와 예약 필드를 사용하지 않는다.
    inline constexpr std::uint16_t FLAGS_NONE = 0;
    inline constexpr std::uint32_t RESERVED_VALUE = 0;

    // ─────────────────────────────
    // 공통 식별자 및 고정소수점 단위
    // ─────────────────────────────

    // Engine/define.h의 PlayerId(std::uint8_t), MAX_PLAYERS(2)와 동일한 폭
    using PlayerSlot = std::uint8_t;
    inline constexpr PlayerSlot MAX_PROTOCOL_PLAYERS = 2;
    inline constexpr PlayerSlot INVALID_PLAYER_SLOT = 0xFF;

    using ObjectId = std::uint32_t;
    using EventId = std::uint32_t;
    using UdpToken = std::uint64_t;

    // 위치(1/100 픽셀) 및 속도(1/100 픽셀/초) i32 고정소수점
    struct FixedVec2
    {
        std::int32_t x = 0;
        std::int32_t y = 0;

        bool operator==(const FixedVec2&) const = default;
    };


    // ─────────────────────────────
    // 메시지 종류
    // ─────────────────────────────

    enum class MessageType : std::uint16_t
    {
        JOIN = 1,
        ACCEPT = 2,
        REJECT = 3,

        READY = 4,
        ROOM_STATE = 5,

        LOAD = 6,
        LOADED = 7,
        START = 8,

        UDP_REGISTER = 9,
        UDP_ACK = 10,

        PLAYER_INPUT = 11,
        STATE_SNAPSHOT = 12,

        SPAWN = 13,
        DESPAWN = 14,

        COMBAT_EVENT = 15,
        RESULT = 16,

        HEARTBEAT = 17,
        LEAVE = 18,
        END = 19
    };

    constexpr bool IsUdpMessageType(MessageType type) noexcept
    {
        return type == MessageType::UDP_REGISTER
            || type == MessageType::UDP_ACK
            || type == MessageType::PLAYER_INPUT
            || type == MessageType::STATE_SNAPSHOT;
    }

    constexpr std::size_t MaxMessageBytesFor(MessageType type) noexcept
    {
        return IsUdpMessageType(type) ? MAX_UDP_BYTES : MAX_TCP_BYTES;
    }


    // ─────────────────────────────
    // 공통 헤더의 값 표현
    // ─────────────────────────────

    // 메모리에서 필드 값을 보관하는 구조체다.
    // 이 구조체의 메모리를 그대로 네트워크에 전송하면 안 된다.
    struct PacketHeader
    {
        std::uint16_t magic = MAGIC;
        std::uint16_t version = VERSION;

        MessageType type = MessageType::JOIN;

        std::uint16_t flags = FLAGS_NONE;

        std::uint32_t totalBytes =
            static_cast<std::uint32_t>(HEADER_BYTES);

        std::uint32_t sessionId = 0;
        std::uint32_t roundId = 1;

        std::uint32_t sequence = 0;
        std::uint32_t hostTick = 0;

        std::uint32_t reserved = RESERVED_VALUE;

        bool operator==(const PacketHeader&) const = default;
    };


    // ─────────────────────────────
    // 코덱 처리 결과
    // ─────────────────────────────

    enum class CodecError
    {
        NONE = 0,

        TRUNCATED,           // 헤더 또는 필요한 데이터가 부족함
        BAD_MAGIC,           // 다른 프로토콜의 데이터
        UNSUPPORTED_VERSION, // 지원하지 않는 버전
        UNKNOWN_TYPE,        // 정의되지 않은 메시지 번호

        INVALID_FLAGS,       // v1에서 허용하지 않는 플래그
        INVALID_RESERVED,    // 예약 필드가 0이 아님

        INVALID_LENGTH,      // 선언된 길이와 실제 길이 등이 맞지 않음
        TOO_LARGE,           // 메시지 최대 크기 초과
        INVALID_FIELD,       // 본문 등의 필드 값이 허용 범위를 벗어남

        UNSUPPORTED_MESSAGE  // 번호는 정의됐으나 코덱 구현이 아직 없음
    };


    // ─────────────────────────────
    // 메시지 본문 열거형 (CScene_Stage01 / Engine 대응)
    // ─────────────────────────────

    // REJECT 본문 거부 사유 (CodecError와 별개)
    enum class RejectReason : std::uint8_t
    {
        NONE = 0,
        ROOM_FULL = 1,
        ALREADY_IN_GAME = 2,
        VERSION_MISMATCH = 3,
        INTERNAL_ERROR = 4
    };

    // LEAVE / END 연결·세션 종료 사유
    enum class DisconnectReason : std::uint8_t
    {
        NORMAL = 0,
        USER_LEFT = 1,
        HEARTBEAT_TIMEOUT = 2,
        LOAD_TIMEOUT = 3,
        QUEUE_OVERFLOW = 4,
        PROTOCOL_ERROR = 5
    };

    // CPlayer 이동 키 비트 (KEY::UP, DOWN, LEFT, RIGHT 대응)
    namespace MoveBit
    {
        inline constexpr std::uint8_t NONE = 0x00;
        inline constexpr std::uint8_t UP = 0x01; // bit 0
        inline constexpr std::uint8_t DOWN = 0x02; // bit 1
        inline constexpr std::uint8_t LEFT = 0x04; // bit 2
        inline constexpr std::uint8_t RIGHT = 0x08; // bit 3
        inline constexpr std::uint8_t ALL_VALID = UP | DOWN | LEFT | RIGHT;
    }

    // CScene_Stage01의 동기화 대상 객체 종류
    // (CScene_Stage01에서 보스도 GROUP_TYPE::MONSTER에 추가되며 MON_TYPE::BOSS로 구분됨)
    enum class NetObjectKind : std::uint8_t
    {
        PLAYER = 1,       // GROUP_TYPE::PLAYER (CPlayer)
        MONSTER = 2,      // GROUP_TYPE::MONSTER (CMonster: NORMAL1~5, EPIC, EPIC2, BOSS)
        PROJ_PLAYER = 3,  // GROUP_TYPE::PROJ_PLAYER (CMissile: LVL1~LVL6)
        PROJ_MONSTER = 4, // GROUP_TYPE::PROJ_MONSTER (CMissile: SMALL, SPIN, MIDDLE, BSMALL)
        ITEM = 5          // GROUP_TYPE::ITEM (CItem)
    };

    // DESPAWN 사유
    enum class DespawnReason : std::uint8_t
    {
        DESTROYED = 1,     // 피격·사망 연출 완료 후 제거
        COLLECTED = 2,     // 플레이어가 아이템 획득
        OUT_OF_BOUNDS = 3, // 배경 경계 밖 이탈 (CMonster/CMissile 경계 판정)
        STAGE_END = 4      // 씬 종료 정리
    };

    // COMBAT_EVENT 종류
    enum class CombatEventKind : std::uint8_t
    {
        DAMAGE_APPLIED = 1, // 탄환 피격으로 대상 HP 변화 확정
        ENTITY_DIED = 2,    // 몬스터(MON_STATE::DEAD 진입) 또는 플레이어 사망 확정
        ITEM_PICKED = 3,    // CItem 단일 획득자 확정 (레벨 증가 또는 +100점)
        SCORE_CHANGED = 4   // 팀 공용 점수(CRankMgr::CurrentRanking.Score) 갱신
    };

    // RESULT 공동 승패 결과
    enum class MatchOutcome : std::uint8_t
    {
        VICTORY = 1, // MON_TYPE::BOSS 처치 시 공동 승리 (동일 틱 전원 사망 시 승리 우선)
        DEFEAT = 2   // 두 플레이어 전원 사망 시 패배
    };


    // ─────────────────────────────
    // 메시지별 본문 값 구조체 (1~19)
    // ─────────────────────────────

    // 1. JOIN (TCP, C->H, 본문 0B, 총 32B)
    struct JoinPayload
    {
        bool operator==(const JoinPayload&) const = default;
    };

    // 2. ACCEPT (TCP, H->C, 본문 12B, 총 44B)
    struct AcceptPayload
    {
        PlayerSlot assignedPlayerId = 1; // 호스트=0, 참가자=1
        std::uint8_t maxPlayers = MAX_PROTOCOL_PLAYERS;
        std::uint16_t reserved = 0;
        UdpToken udpToken = 0;

        bool operator==(const AcceptPayload&) const = default;
    };

    // 3. REJECT (TCP, H->C, 본문 4B, 총 36B)
    struct RejectPayload
    {
        RejectReason reason = RejectReason::ROOM_FULL;
        std::uint8_t reserved0 = 0;
        std::uint16_t reserved1 = 0;

        bool operator==(const RejectPayload&) const = default;
    };

    // 4. READY (TCP, 양방향, 본문 4B, 총 36B)
    struct ReadyPayload
    {
        PlayerSlot playerId = 0;
        std::uint8_t isReady = 0; // 0 또는 1
        std::uint16_t reserved = 0;

        bool operator==(const ReadyPayload&) const = default;
    };

    struct RoomSlotState
    {
        PlayerSlot playerId = 0;
        std::uint8_t connected = 0;     // 0 또는 1
        std::uint8_t ready = 0;         // 0 또는 1
        std::uint8_t udpRegistered = 0; // 0 또는 1

        bool operator==(const RoomSlotState&) const = default;
    };

    // 5. ROOM_STATE (TCP, H->C, 본문 8B, 총 40B)
    struct RoomStatePayload
    {
        std::array<RoomSlotState, MAX_PROTOCOL_PLAYERS> slots{};

        bool operator==(const RoomStatePayload&) const = default;
    };

    // 6. LOAD (TCP, H->C, 본문 4B, 총 36B)
    // 실제 게임은 CScene_Stage01(stageId = 1) 단일 스테이지에서 진행된다.
    struct LoadPayload
    {
        std::uint16_t stageId = 1; // 1 = CScene_Stage01
        std::uint16_t reserved = 0;

        bool operator==(const LoadPayload&) const = default;
    };

    // 7. LOADED (TCP, C->H, 본문 4B, 총 36B)
    // CScene_Stage01::Enter() 및 보스 애니메이션 사전 로딩 완료 보고
    struct LoadedPayload
    {
        PlayerSlot playerId = 1;
        std::uint8_t reserved0 = 0;
        std::uint16_t reserved1 = 0;

        bool operator==(const LoadedPayload&) const = default;
    };

    // 8. START (TCP, H->C, 본문 12B, 총 44B)
    struct StartPayload
    {
        std::uint32_t startHostTick = 0;
        std::uint16_t simRateHz = 60;
        std::uint16_t stateRateHz = 20;
        std::uint16_t inputRateHz = 30;
        std::uint16_t reserved = 0;

        bool operator==(const StartPayload&) const = default;
    };

    // 9. UDP_REGISTER (UDP, C->H, 본문 12B, 총 44B)
    struct UdpRegisterPayload
    {
        PlayerSlot playerId = 1;
        std::uint8_t reserved0 = 0;
        std::uint16_t reserved1 = 0;
        UdpToken udpToken = 0;

        bool operator==(const UdpRegisterPayload&) const = default;
    };

    // 10. UDP_ACK (UDP, H->C, 본문 4B, 총 36B)
    struct UdpAckPayload
    {
        PlayerSlot playerId = 1;
        std::uint8_t registered = 1; // 0 또는 1
        std::uint16_t reserved = 0;

        bool operator==(const UdpAckPayload&) const = default;
    };

    // 11. PLAYER_INPUT (UDP, C->H, 본문 8B, 총 40B)
    // 계획서 필수: playerId, sequence, 이동 비트(UP/DOWN/LEFT/RIGHT), 발사 유지(KEY::S)
    struct PlayerInputPayload
    {
        PlayerSlot playerId = 1;
        std::uint8_t moveBits = MoveBit::NONE;
        std::uint8_t fireHeld = 0; // KEY::S 유지 여부 (0 또는 1)
        std::uint8_t reserved = 0;
        std::uint32_t inputSequence = 0;

        bool operator==(const PlayerInputPayload&) const = default;
    };

    // STATE_SNAPSHOT 내 개별 객체 상태 레코드 (고정 32B)
    // 계획서 필수: objectId, type(kind/subType), hostTick, 위치, 속도, HP, alive, FSM 상태
    struct ObjectStateRecord
    {
        ObjectId objectId = 0;                       // 4B
        NetObjectKind kind = NetObjectKind::MONSTER; // 1B
        // subType: PLAYER면 무기 레벨(m_iLvl 1~5), MONSTER면 MON_TYPE, 탄환이면 MISSILE_TYPE
        std::uint8_t subType = 0;                    // 1B
        std::uint8_t alive = 1;                      // 1B (0: 사망/전투불능, 1: 생존)
        // fsmState: MONSTER면 MON_STATE, PLAYER면 정상(0)/피격무적(m_bHit=1)/클리어(2)
        std::uint8_t fsmState = 0;                   // 1B
        FixedVec2 pos{};                             // 8B (1/100 px)
        FixedVec2 vel{};                             // 8B (1/100 px/s)
        std::int32_t hp = 0;                         // 4B (현재 HP)
        std::int32_t maxHp = 0;                      // 4B (최대 HP)

        bool operator==(const ObjectStateRecord&) const = default;
    };

    inline constexpr std::size_t SNAPSHOT_HEADER_BYTES = 8;
    inline constexpr std::size_t OBJECT_STATE_RECORD_BYTES = 32;
    // (1200 - 32 - 8) / 32 = 36개
    inline constexpr std::size_t MAX_SNAPSHOT_OBJECTS_PER_PACKET =
        (MAX_UDP_BYTES - HEADER_BYTES - SNAPSHOT_HEADER_BYTES) / OBJECT_STATE_RECORD_BYTES;

    // 12. STATE_SNAPSHOT (UDP, H->C, 본문 8 + 32*N B, 최대 1192B)
    struct StateSnapshotPayload
    {
        std::uint32_t snapshotTick = 0;
        std::uint8_t partIndex = 0;
        std::uint8_t partCount = 1;
        std::vector<ObjectStateRecord> objects{};

        bool operator==(const StateSnapshotPayload&) const = default;
    };

    // 13. SPAWN (TCP, H->C, 본문 36B, 총 68B)
    // 계획서 필수: 발사자(ownerPlayerId), 피해량(damage), 패턴 초기값(patternType)
    // - 몬스터: subType = MON_TYPE, patternType = MISSILE_PTRN
    // - 탄환: subType = MISSILE_TYPE, patternType = CMissile::m_iType(0:플레이어, 1:등속, 2:가속, 3:감속)
    struct SpawnPayload
    {
        EventId eventId = 0;                            // 4B
        ObjectId objectId = 0;                          // 4B
        NetObjectKind kind = NetObjectKind::MONSTER;    // 1B
        std::uint8_t subType = 0;                       // 1B (MON_TYPE / MISSILE_TYPE)
        PlayerSlot ownerPlayerId = INVALID_PLAYER_SLOT; // 1B (발사자 PlayerId, 몬스터/아이템은 0xFF)
        std::uint8_t patternType = 0;                   // 1B (MISSILE_PTRN 또는 CMissile::m_iType)
        FixedVec2 pos{};                                // 8B (초기 위치)
        FixedVec2 vel{};                                // 8B (초기 속도 = m_vDir * m_fVec)
        std::int32_t hp = 0;                            // 4B (초기 체력)
        std::int32_t damage = 0;                        // 4B (발사 시 확정된 공격력 m_iAtk 등)

        bool operator==(const SpawnPayload&) const = default;
    };

    // 14. DESPAWN (TCP, H->C, 본문 12B, 총 44B)
    struct DespawnPayload
    {
        EventId eventId = 0;
        ObjectId objectId = 0;
        DespawnReason reason = DespawnReason::DESTROYED;
        std::uint8_t reserved0 = 0;
        std::uint16_t reserved1 = 0;

        bool operator==(const DespawnPayload&) const = default;
    };

    // 15. COMBAT_EVENT (TCP, H->C, 본문 28B, 총 60B)
    // 계획서 필수: 확정 HP(confirmedHp), 획득자(instigatorPlayerId), 팀 점수(teamScore)
    struct CombatEventPayload
    {
        EventId eventId = 0;                            // 4B
        CombatEventKind kind =
            CombatEventKind::DAMAGE_APPLIED;            // 1B
        PlayerSlot instigatorPlayerId =
            INVALID_PLAYER_SLOT;                        // 1B (공격자 또는 아이템 획득자 PlayerId)
        std::uint8_t updatedPlayerLevel = 0;            // 1B (아이템/충돌로 변동된 플레이어 m_iLvl, 무관하면 0)
        std::uint8_t reserved = 0;                      // 1B
        ObjectId sourceObjectId = 0;                    // 4B (원인 탄환/아이템 ObjectId)
        ObjectId targetObjectId = 0;                    // 4B (대상 몬스터/플레이어 ObjectId)
        std::int32_t confirmedHp = 0;                   // 4B (호스트가 확정한 대상 남은 HP)
        std::int32_t scoreDelta = 0;                    // 4B (점수 증감량)
        std::uint32_t teamScore = 0;                    // 4B (확정 후 팀 공용 점수)

        bool operator==(const CombatEventPayload&) const = default;
    };

    // 16. RESULT (TCP, H->C, 본문 12B, 총 44B)
    struct ResultPayload
    {
        EventId eventId = 0;
        MatchOutcome outcome = MatchOutcome::VICTORY;
        std::uint8_t reserved0 = 0;
        std::uint16_t reserved1 = 0;
        std::uint32_t finalTeamScore = 0;

        bool operator==(const ResultPayload&) const = default;
    };

    // 17. HEARTBEAT (TCP, 양방향, 본문 0B, 총 32B)
    struct HeartbeatPayload
    {
        bool operator==(const HeartbeatPayload&) const = default;
    };

    // 18. LEAVE (TCP, 양방향, 본문 4B, 총 36B)
    struct LeavePayload
    {
        PlayerSlot playerId = 0;
        DisconnectReason reason = DisconnectReason::USER_LEFT;
        std::uint16_t reserved = 0;

        bool operator==(const LeavePayload&) const = default;
    };

    // 19. END (TCP, H->C, 본문 4B, 총 36B)
    struct EndPayload
    {
        DisconnectReason reason = DisconnectReason::NORMAL;
        std::uint8_t reserved0 = 0;
        std::uint16_t reserved1 = 0;

        bool operator==(const EndPayload&) const = default;
    };


    // ─────────────────────────────
    // 통합 메시지 표현
    // ─────────────────────────────

    using MessagePayload = std::variant<
        JoinPayload,
        AcceptPayload,
        RejectPayload,
        ReadyPayload,
        RoomStatePayload,
        LoadPayload,
        LoadedPayload,
        StartPayload,
        UdpRegisterPayload,
        UdpAckPayload,
        PlayerInputPayload,
        StateSnapshotPayload,
        SpawnPayload,
        DespawnPayload,
        CombatEventPayload,
        ResultPayload,
        HeartbeatPayload,
        LeavePayload,
        EndPayload>;

    struct Message
    {
        PacketHeader header{};
        MessagePayload payload = JoinPayload{};

        bool operator==(const Message&) const = default;
    };
}
