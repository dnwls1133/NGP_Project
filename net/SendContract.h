#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>
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

    // ─────────────────────────────────────────────────────────────────────────
    // 11/06 가짜 송신 링크 (FakeSendSink)
    // - 소켓이나 실제 통신 스레드 없이 호스트 상태 배치 교체(B01~B12)와
    // 단건 메시지 송신 계약을 독립 검증하기 위한 테스트 입니다.
    // - CNetworkMgr와 동일한 잠금 검증 교체 용량 생명주기 규칙을 따릅니다.
    // ─────────────────────────────────────────────────────────────────────────
    class FakeSendSink : public ISendSink
    {
    public:
        struct QueuedSendEntry
        {
            Channel channel = Channel::Tcp;
            protocol::Message message{};

            bool operator==(const QueuedSendEntry&) const = default;
        };

        SendResult EnqueueSend(
            Channel channel,
            const protocol::Message& message) override
        {
            if (!m_running.load())
                return SendResult::NotRunning;

            // 1. 잠금 밖에서 구조·길이·채널 검증 및 독립 복사본 준비
            const SendResult validation = ValidateSendRequest(channel, message);
            if (validation != SendResult::Queued)
                return validation;

            QueuedSendEntry copy{ channel, message };

            // 2. 송신 mutex 안에서 실행 상태와 큐 상한(1,024건) 확인 후 등록
            std::lock_guard<std::mutex> lock(m_sendMutex);
            if (!m_running.load())
                return SendResult::NotRunning;

            const std::size_t pendingBatchParts =
                m_pendingBatch.has_value() ? m_pendingBatch->parts.size() : 0;
            if (m_sendQueue.size() + pendingBatchParts + 1 > MAX_SEND_QUEUE_ENTRIES)
                return SendResult::QueueFull;

            m_sendQueue.push_back(std::move(copy));
            return SendResult::Queued;

        }

        SendResult EnqueueSnapshotBatch(const SnapshotBatch& batch) override
        {
            if (!m_running.load())
                return SendResult::NotRunning;

            // 1. 잠금 밖에서 배치 구성·크기 검증 및 독립 값 복사본 준비 (B10 원본 변경 불변 보장)
            const SendResult validation = ValidateSnapshotBatch(batch);
            if (validation != SendResult::Queued)
                return validation;

            SnapshotBatch copy{ batch };
            const auto& firstHeader = copy.parts.front().header;

            // 2. 송신 mutex 안에서 실행 상태, 최신 틱(StateBatch), 큐 용량(QueueFull) 확인
            std::lock_guard<std::mutex> lock{ m_sendMutex };
            if (!m_running.load())
                return SendResult::NotRunning;

            const auto lastIt = m_lastSnapshotTickBySession.find(firstHeader.sessionId);
            if (lastIt != m_lastSnapshotTickBySession.end()
                && firstHeader.hostTick <= lastIt->second)
            {
                return SendResult::StaleBatch;
            }

            // 교체 용량: (현재 단건 대기 건수 + 새 배치 조각 수)
            // <= 1,024건 (기존 대기 배치 조각 수는 교체되므로 제외)
            if (m_sendQueue.size() + copy.parts.size() > MAX_SEND_QUEUE_ENTRIES)
                return SendResult::QueueFull;

            const bool replaced = m_pendingBatch.has_value();
            m_lastSnapshotTickBySession.insert_or_assign(
                firstHeader.sessionId,
                firstHeader.hostTick
            );
            m_pendingBatch = std::move(copy);

            return replaced ? SendResult::ReplacedPending : SendResult::Queued;

        }

        // 통신 스레드가 송신 mutex 안에서 대기 배치를 가져가는(전송 시작 경계) 동작을 모사합니다. (B03)
        std::optional<SnapshotBatch> TakePendingBatchForSend()
        {
            std::lock_guard<std::mutex> lock{ m_sendMutex };
            if (!m_pendingBatch.has_value())
                return std::nullopt;

            m_inFlightBatch = std::move(m_pendingBatch);
            m_pendingBatch.reset();
            return m_inFlightBatch;
        }

        // 종료(Stop): 새 송신 요청을 차단하고 대기 데이터를 정리합니다. (반복 호출 안전, B11)
        void Stop()
        {
            m_running.store(false);
            std::lock_guard<std::mutex> lock(m_sendMutex);
            m_sendQueue.clear();
            m_pendingBatch.reset();
            m_inFlightBatch.reset();
            m_lastSnapshotTickBySession.clear();
        }

        // 새 세션 시작: 이전 세션의 틱·배치·큐 기록을 비우고 실행 상태로 전환한다.
        void StartNewSession()
        {
            std::lock_guard<std::mutex> lock(m_sendMutex);
            m_sendQueue.clear();
            m_pendingBatch.reset();
            m_inFlightBatch.reset();
            m_lastSnapshotTickBySession.clear();
            m_running.store(true);
        }

        std::optional<SnapshotBatch> PeekPendingBatch() const
        {
            std::lock_guard<std::mutex> lock{ m_sendMutex };
            return m_pendingBatch;
        }

        std::optional<SnapshotBatch> PeekInFlightBatch() const
        {
            std::lock_guard<std::mutex> lock{ m_sendMutex };
            return m_inFlightBatch;
        }

        std::vector<QueuedSendEntry> PeekSendQueue() const
        {
            std::lock_guard<std::mutex> lock{ m_sendMutex };
            return { m_sendQueue.begin(), m_sendQueue.end() };
        }
    private:
        mutable std::mutex m_sendMutex;
        std::atomic<bool> m_running{ true };
        std::deque<QueuedSendEntry> m_sendQueue;
        std::optional<SnapshotBatch> m_pendingBatch;
        std::optional<SnapshotBatch> m_inFlightBatch;
        std::unordered_map<std::uint32_t, std::uint32_t> m_lastSnapshotTickBySession;
    };
}
