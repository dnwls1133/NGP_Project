# Virus Striker 네트워크 프로토콜 규격 v1

본 문서는 세 담당자(정우진·김영록·이현범)가 공유하는 공통 통신 규격 v1이다.
코드 선언은 `shared/protocol/ProtocolTypes.h`와 `shared/protocol/PacketCodec.h`를 단일 기준으로 삼는다.

## 1. 기본 원칙과 전송 경계

- **바이트 순서**: 모든 다중 바이트 정수(`u16`, `u32`, `u64`, `i32`)는 **big-endian(네트워크 바이트 순서)**으로 직렬화한다.
- **메모리 직접 복사 금지**: `sizeof(Struct)`나 `reinterpret_cast`로 C++ 구조체를 송수신하지 않는다.
- **고정소수점 단위**: 위치(`pos.x`, `pos.y`)와 속도(`vel.x`, `vel.y`)는 부호 있는 32비트 정수(`i32`)이며 각각 **1/100 픽셀**, **1/100 픽셀/초** 단위다.
- **크기 상한 (헤더 32바이트 포함)**:
  - UDP 메시지(`UDP_REGISTER`, `UDP_ACK`, `PLAYER_INPUT`, `STATE_SNAPSHOT`): 최대 **1,200바이트**
  - TCP 프레임(그 외 15종 메시지): 최대 **65,536바이트**
- **식별자 및 스테이지 규칙**:
  - 실제 게임 진행은 `CScene_Stage01`(`stageId = 1`) 단일 스테이지에서 이루어진다.
  - `sessionId`: `JOIN` 시 `0`, 승인(`ACCEPT`) 이후 발급된 0이 아닌 세션 번호 사용. 새 세션마다 변경.
  - `roundId`: `1` 고정.
  - `ObjectId`, `EventId`: 세션 내 단조 증가(`1, 2, 3...`), 세션 종료 전 재사용 금지.
  - `PlayerSlot`: `0`(호스트 P0), `1`(참가자 P1), `0xFF`(해당 없음).

## 2. 공통 헤더 (32바이트 고정)

| 오프셋 | 크기 | 필드 | 타입 | 고정/규칙 |
|---:|---:|---|---|---|
| 0 | 2 | `magic` | `u16` | `0x5653` (`'V'`, `'S'`) |
| 2 | 2 | `version` | `u16` | `1` |
| 4 | 2 | `type` | `u16` | `MessageType` (`1`~`19`) |
| 6 | 2 | `flags` | `u16` | `0` (0이 아니면 `INVALID_FLAGS`) |
| 8 | 4 | `totalBytes` | `u32` | `32 + 본문 바이트 수` (프레임 실제 길이와 일치 필수) |
| 12 | 4 | `sessionId` | `u32` | `JOIN`은 `0`, 승인 후 발급 값 |
| 16 | 4 | `roundId` | `u32` | `1` (`0`이면 `INVALID_FIELD`) |
| 20 | 4 | `sequence` | `u32` | 송신 순번 |
| 24 | 4 | `hostTick` | `u32` | 호스트 시뮬레이션 틱 (비인게임/클라이언트 송신 시 `0`) |
| 28 | 4 | `reserved` | `u32` | `0` (0이 아니면 `INVALID_RESERVED`) |

## 3. 메시지별 본문 레이아웃 (1~19)

| 번호 | 이름 | 경로 | 본문(B) | 총 길이(B) | 본문 필드 순서 (오프셋은 본문 시작 = 바이트 32 기준) |
|---:|---|---|---:|---:|---|
| 1 | `JOIN` | TCP | 0 | 32 | (본문 없음) `header.sessionId`는 반드시 `0` |
| 2 | `ACCEPT` | TCP | 12 | 44 | `+0: assignedPlayerId(u8, 0..1)`, `+1: maxPlayers(u8, 2)`, `+2: reserved(u16, 0)`, `+4: udpToken(u64)` |
| 3 | `REJECT` | TCP | 4 | 36 | `+0: reason(u8, 1..4)`, `+1: reserved0(u8, 0)`, `+2: reserved1(u16, 0)` |
| 4 | `READY` | TCP | 4 | 36 | `+0: playerId(u8, 0..1)`, `+1: isReady(u8, 0..1)`, `+2: reserved(u16, 0)` |
| 5 | `ROOM_STATE` | TCP | 8 | 40 | 슬롯 0(4B) + 슬롯 1(4B): 각 `playerId(u8)`, `connected(u8)`, `ready(u8)`, `udpRegistered(u8)` |
| 6 | `LOAD` | TCP | 4 | 36 | `+0: stageId(u16, 1=CScene_Stage01)`, `+2: reserved(u16, 0)` |
| 7 | `LOADED` | TCP | 4 | 36 | `+0: playerId(u8, 0..1)`, `+1: reserved0(u8, 0)`, `+2: reserved1(u16, 0)` |
| 8 | `START` | TCP | 12 | 44 | `+0: startHostTick(u32)`, `+4: simRateHz(u16)`, `+6: stateRateHz(u16)`, `+8: inputRateHz(u16)`, `+10: reserved(u16, 0)` |
| 9 | `UDP_REGISTER` | UDP | 12 | 44 | `+0: playerId(u8, 0..1)`, `+1: reserved0(u8, 0)`, `+2: reserved1(u16, 0)`, `+4: udpToken(u64)` |
| 10 | `UDP_ACK` | UDP | 4 | 36 | `+0: playerId(u8, 0..1)`, `+1: registered(u8, 0..1)`, `+2: reserved(u16, 0)` |
| 11 | `PLAYER_INPUT` | UDP | 8 | 40 | `+0: playerId(u8, 0..1)`, `+1: moveBits(u8, 상위4비트=0)`, `+2: fireHeld(u8, 0..1)`, `+3: reserved(u8, 0)`, `+4: inputSequence(u32)` |
| 12 | `STATE_SNAPSHOT` | UDP | `8+32N` | `40+32N` | `+0: snapshotTick(u32)`, `+4: partIndex(u8)`, `+5: partCount(u8, >=1)`, `+6: objectCount(u16, 0..36)` 이후 32B 레코드 `N`개 |
| 13 | `SPAWN` | TCP | 36 | 68 | `+0: eventId(u32)`, `+4: objectId(u32)`, `+8: kind(u8, 1..5)`, `+9: subType(u8)`, `+10: ownerPlayerId(u8, 0/1/0xFF)`, `+11: patternType(u8)`, `+12: pos.x(i32)`, `+16: pos.y(i32)`, `+20: vel.x(i32)`, `+24: vel.y(i32)`, `+28: hp(i32)`, `+32: damage(i32)` |
| 14 | `DESPAWN` | TCP | 12 | 44 | `+0: eventId(u32)`, `+4: objectId(u32)`, `+8: reason(u8, 1..4)`, `+9: reserved0(u8, 0)`, `+10: reserved1(u16, 0)` |
| 15 | `COMBAT_EVENT` | TCP | 28 | 60 | `+0: eventId(u32)`, `+4: kind(u8, 1..4)`, `+5: instigatorPlayerId(u8, 0/1/0xFF)`, `+6: updatedPlayerLevel(u8, 0..5)`, `+7: reserved(u8, 0)`, `+8: sourceObjectId(u32)`, `+12: targetObjectId(u32)`, `+16: confirmedHp(i32)`, `+20: scoreDelta(i32)`, `+24: teamScore(u32)` |
| 16 | `RESULT` | TCP | 12 | 44 | `+0: eventId(u32)`, `+4: outcome(u8, 1..2)`, `+5: reserved0(u8, 0)`, `+6: reserved1(u16, 0)`, `+8: finalTeamScore(u32)` |
| 17 | `HEARTBEAT` | TCP | 0 | 32 | (본문 없음) |
| 18 | `LEAVE` | TCP | 4 | 36 | `+0: playerId(u8, 0..1)`, `+1: reason(u8, 0..5)`, `+2: reserved(u16, 0)` |
| 19 | `END` | TCP | 4 | 36 | `+0: reason(u8, 0..5)`, `+1: reserved0(u8, 0)`, `+2: reserved1(u16, 0)` |

### `STATE_SNAPSHOT` 객체 레코드 (32바이트 고정)
| 상대 오프셋 | 크기 | 필드 | 타입 | 비고 |
|---:|---:|---|---|---|
| +0 | 4 | `objectId` | `u32` | `0` 불가 |
| +4 | 1 | `kind` | `u8` | `NetObjectKind` (`1`~`5`) |
| +5 | 1 | `subType` | `u8` | 플레이어 무기 레벨(`1`~`5`) / `MON_TYPE` / `MISSILE_TYPE` |
| +6 | 1 | `alive` | `u8` | `0` 또는 `1` |
| +7 | 1 | `fsmState` | `u8` | `MON_STATE` 또는 플레이어 상태 |
| +8 | 4 | `pos.x` | `i32` | 1/100 px |
| +12 | 4 | `pos.y` | `i32` | 1/100 px |
| +16 | 4 | `vel.x` | `i32` | 1/100 px/s |
| +20 | 4 | `vel.y` | `i32` | 1/100 px/s |
| +24 | 4 | `hp` | `i32` | 현재 체력 |
| +28 | 4 | `maxHp` | `i32` | 최대 체력 |

## 4. `Decode` 검사 우선순위 (오류 판정 순서)

1. `frame.size() < 32` → `CodecError::TRUNCATED`
2. `magic != 0x5653` → `CodecError::BAD_MAGIC`
3. `version != 1` → `CodecError::UNSUPPORTED_VERSION`
4. `type < 1 || type > 19` → `CodecError::UNKNOWN_TYPE`
5. `flags != 0` → `CodecError::INVALID_FLAGS`
6. `reserved != 0` → `CodecError::INVALID_RESERVED`
7. `totalBytes < 32` → `CodecError::INVALID_LENGTH`
8. `totalBytes > MaxMessageBytesFor(type)` 또는 `frame.size() > MaxMessageBytesFor(type)` → `CodecError::TOO_LARGE`
9. `totalBytes != frame.size()` → `CodecError::INVALID_LENGTH`
10. 메시지별 본문 바이트 길이가 규격과 불일치 → `CodecError::INVALID_LENGTH`
11. 헤더/본문 필드 값 범위 위반(`roundId == 0`, `JOIN`의 `sessionId != 0`, 허용되지 않은 `moveBits`, `partIndex >= partCount` 등) → `CodecError::INVALID_FIELD`
12. 아직 구현되지 않은 메시지 번호 처리 시 → `CodecError::UNSUPPORTED_MESSAGE`

## 5. 고정 바이트 Fixture (독립 검산 기준)

### Fixture 1: 정상 `JOIN` (32바이트)
- 입력: `type = JOIN(1)`, `sessionId = 0`, `roundId = 1`, `sequence = 0`, `hostTick = 0`
```text
56 53 00 01 00 01 00 00
00 00 00 20 00 00 00 00
00 00 00 01 00 00 00 00
00 00 00 00 00 00 00 00
```

### Fixture 2: 정상 `PLAYER_INPUT` (40바이트)
- 입력:
  - 헤더: `type = PLAYER_INPUT(11)`, `totalBytes = 40(0x28)`, `sessionId = 0x000003E8(1000)`, `roundId = 1`, `sequence = 0x01020304`, `hostTick = 0`
  - 본문: `playerId = 1`, `moveBits = 0x05(UP | LEFT)`, `fireHeld = 1`, `reserved = 0`, `inputSequence = 0x01020304`
```text
56 53 00 01 00 0B 00 00
00 00 00 28 00 00 03 E8
00 00 00 01 01 02 03 04
00 00 00 00 00 00 00 00
01 05 01 00 01 02 03 04
```

## 6. 객체 생성·삭제·참조 해제 경계 계약

- `CScene`, `CObject`, `CColliderMgr`, `CEventMgr`, 리소스/GDI는 메인 스레드에서만 접근한다.
- 첫 삭제 요청 시 `PendingDestroy`로 전환하여 같은 틱의 이후 충돌·피해·아이템 획득·조준에서 즉시 제외하고 중복 삭제 요청을 무시한다.
- `CScene::render`에서 수행하던 객체 제거를 시뮬레이션 틱 종료 경계로 분리하고, 충돌 쌍·플레이어 슬롯·`ObjectId` 조회표·그룹 벡터를 함께 정리한 뒤 해제한다.
- 참가자는 삭제된 `ObjectId`를 세션 종료까지 기억하여 지연 도착한 `STATE_SNAPSHOT`으로 인해 삭제된 객체가 부활하지 않게 한다.
