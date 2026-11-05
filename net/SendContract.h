#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "../shared/protocol/PacketCodec.h"
#include "../shared/protocol/ProtocolTypes.h"

namespace net
{
    // 전송 채널 구분 (THREAD_QUEUE_BATCH_CONTRACT_1105.md 3절)
    enum class Channel : std::uint8_t
    {
        Tcp = 0,
        Udp = 1
    };

    // 송신 큐 및 상태 배치 등록 결과 (THREAD_QUEUE_BATCH_CONTRACT_1105.md 3절)
    enum class SendResult : std::uint8_t
    {
        Queued = 0,          // 새 데이터 등록 성공
        ReplacedPending = 1, // 기존 미전송 입력 또는 대기 상태 배치 교체 성공
        NotRunning = 2,      // 통신 시작 전 또는 종료 중(Stop 이후)
        InvalidRequest = 3,  // 채널·메시지 종류·필드·배치 구성 오류
        TooLarge = 4,        // 단일 메시지 크기(1200B/65536B) 또는 최대 조각 수(255개) 초과
        StaleBatch = 5,      // 이미 수락한 상태 시점(hostTick)보다 오래되거나 같음
        QueueFull = 6,       // 송신 큐 용량(1,024건) 부족
        NoMemory = 7         // 복사/등록 중 메모리 확보 실패
    };

    // 상태 배치 상한 및 송수신 큐 상한 상수 (4절·6절)
    inline constexpr std::size_t MAX_SNAPSHOT_PARTS_PER_BATCH = 255;
    inline constexpr std::size_t MAX_SEND_QUEUE_ENTRIES = 1'024;
    inline constexpr std::size_t MAX_RECV_QUEUE_ENTRIES = 1'024;

    // 한 시점(동일 sessionId, 동일 hostTick)의 STATE_SNAPSHOT 조각 묶음 (3절)
    struct SnapshotBatch
    {
        std::vector<protocol::Message> parts{};

        bool operator==(const SnapshotBatch&) const = default;
    };

    // CNetworkMgr와 FakeSendSink가 동일하게 따르는 송신 호출 계약 인터페이스 (3절)
    class ISendSink
    {
    public:
        virtual ~ISendSink() = default;

        virtual SendResult EnqueueSend(
            Channel channel,
            const protocol::Message& message) = 0;

        virtual SendResult EnqueueSnapshotBatch(
            const SnapshotBatch& batch) = 0;
    };

    // ─────────────────────────────────────────────────────────────────────────
    // 잠금 밖(Lock-free 구간)에서 수행하는 단건 송신 및 상태 배치 구성 검증 함수 (3~5절)
    // - 성공 시 SendResult::Queued 를 반환합니다 (잠금 진입 전 구조·크기 검증 통과 의미).
    // - 크기 초과 시 SendResult::TooLarge, 그 외 구성 오류 시 SendResult::InvalidRequest 를 반환합니다.
    // ─────────────────────────────────────────────────────────────────────────

    inline SendResult ValidateSendRequest(
        Channel channel,
        const protocol::Message& message)
    {
        using namespace protocol;

        // 3절 규칙: STATE_SNAPSHOT을 EnqueueSend로 보내면 InvalidRequest (1조각이어도 EnqueueSnapshotBatch 사용)
        if (message.header.type == MessageType::STATE_SNAPSHOT
            || std::holds_alternative<StateSnapshotPayload>(message.payload))
        {
            return SendResult::InvalidRequest;
        }

        // 3절 규칙: 메시지 종류별 전송 경로(UDP 4종 vs TCP 15종)와 채널이 다르면 InvalidRequest
        const Channel expectedChannel =
            IsUdpMessageType(message.header.type) ? Channel::Udp : Channel::Tcp;
        if (channel != expectedChannel)
        {
            return SendResult::InvalidRequest;
        }

        // 코덱 규칙으로 헤더·본문 필드 범위 및 최대 크기(UDP 1200B / TCP 65536B) 검사
        std::vector<std::uint8_t> encoded;
        const CodecError err = CPacketCodec::Encode(message, encoded);
        if (err == CodecError::TOO_LARGE)
            return SendResult::TooLarge;
        if (err != CodecError::NONE)
            return SendResult::InvalidRequest;

        return SendResult::Queued;
    }

    inline SendResult ValidateSnapshotBatch(const SnapshotBatch& batch)
    {
        using namespace protocol;

        // 4절 규칙: 빈 배치는 InvalidRequest, 255조각 초과는 TooLarge
        if (batch.parts.empty())
            return SendResult::InvalidRequest;

        if (batch.parts.size() > MAX_SNAPSHOT_PARTS_PER_BATCH)
            return SendResult::TooLarge;

        const std::uint8_t expectedPartCount =
            static_cast<std::uint8_t>(batch.parts.size());

        // 첫 번째 조각에서 기준 sessionId와 기준 hostTick 추출
        const Message& firstMsg = batch.parts[0];
        if (firstMsg.header.type != MessageType::STATE_SNAPSHOT
            || !std::holds_alternative<StateSnapshotPayload>(firstMsg.payload))
        {
            return SendResult::InvalidRequest;
        }

        const std::uint32_t batchSessionId = firstMsg.header.sessionId;
        const std::uint32_t batchTick = firstMsg.header.hostTick;

        // 4절 규칙: 전 조각의 sessionId는 동일한 0 아닌 값이어야 함
        if (batchSessionId == 0)
            return SendResult::InvalidRequest;

        // 같은 배치 전체에서 ObjectId 중복 검사용 컨테이너
        std::vector<ObjectId> seenObjectIds;
        std::vector<std::uint8_t> scratchBytes;

        for (std::size_t i = 0; i < batch.parts.size(); ++i)
        {
            const Message& part = batch.parts[i];

            // 1) 메시지 종류 확인 (모든 항목은 STATE_SNAPSHOT)
            if (part.header.type != MessageType::STATE_SNAPSHOT
                || !std::holds_alternative<StateSnapshotPayload>(part.payload))
            {
                return SendResult::InvalidRequest;
            }

            // 2) 세션·회차 일치 확인 (동일한 0 아닌 sessionId, roundId == 1)
            if (part.header.sessionId != batchSessionId
                || part.header.roundId != 1)
            {
                return SendResult::InvalidRequest;
            }

            const auto& snap = std::get<StateSnapshotPayload>(part.payload);

            // 3) 시점 일치 확인 (전 조각의 header.hostTick과 payload.snapshotTick이 동일)
            if (part.header.hostTick != batchTick
                || snap.snapshotTick != batchTick)
            {
                return SendResult::InvalidRequest;
            }

            // 4) 조각 번호(0..N-1 순서) 및 조각 수(partCount == parts.size()) 일치 확인
            if (snap.partIndex != static_cast<std::uint8_t>(i)
                || snap.partCount != expectedPartCount)
            {
                return SendResult::InvalidRequest;
            }

            // 5) 조각당 최대 객체 수(36개) 초과 시 TooLarge
            if (snap.objects.size() > MAX_SNAPSHOT_OBJECTS_PER_PACKET)
            {
                return SendResult::TooLarge;
            }

            // 6) 배치 내 ObjectId 중복 검사 (0번 ID 및 중복 ID 거부)
            for (const ObjectStateRecord& obj : snap.objects)
            {
                if (obj.objectId == 0)
                    return SendResult::InvalidRequest;

                if (std::find(seenObjectIds.begin(), seenObjectIds.end(), obj.objectId)
                    != seenObjectIds.end())
                {
                    return SendResult::InvalidRequest;
                }
                seenObjectIds.push_back(obj.objectId);
            }

            // 7) 개별 조각의 헤더·본문 코덱 유효성 및 1200B 상한 확인
            const CodecError err = CPacketCodec::Encode(part, scratchBytes);
            if (err == CodecError::TOO_LARGE)
                return SendResult::TooLarge;
            if (err != CodecError::NONE)
                return SendResult::InvalidRequest;
        }

        return SendResult::Queued;
    }
}
