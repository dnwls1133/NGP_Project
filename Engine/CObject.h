#pragma once


#include <optional>
#include "CCamera.h"
#include "../shared/protocol/ProtocolTypes.h"

class CCollider;
class CAnimator;



class CObject
{
private:
	wstring m_strName;
	Vec2	m_vPos;
	Vec2	m_vScale;

    


	// Component
	CCollider* m_pCollider;
	CAnimator* m_pAnimator;

	bool	   m_bAlive;
	bool	   m_bCollideroff;

    std::uint32_t m_objectId = 0;
    std::optional<std::uint32_t> m_lastPositionTick;
    Vec2 m_positionStart;
    Vec2 m_positionTarget;
    float m_positionDuration = 0.f;
    float m_positionElapsed = 0.f;
public:
	void SetPos(Vec2 _vPos) { m_vPos = _vPos; }
	void SetScale(Vec2 _vScale) { m_vScale = _vScale; }
	void SetName(const wstring& _strName) { m_strName = _strName; }
    bool ApplySnapshotPosition(Vec2 target, std::uint32_t tick, float duration);
    void ResetSnapshotPosition();
    void SetObjectId(std::uint32_t _id) { m_objectId = _id; }
    std::uint32_t GetObjectId() const { return m_objectId; }

    virtual protocol::SpawnPayload ExportSpawnPayload(protocol::EventId eventId) const
    {
        protocol::SpawnPayload spawn{};
        spawn.eventId = eventId;
        spawn.objectId = m_objectId;
        spawn.pos.x = static_cast<std::int32_t>(m_vPos.x * 100.f);
        spawn.pos.y = static_cast<std::int32_t>(m_vPos.y * 100.f);
        return spawn;
    }
   

	bool IsDead() const { return !m_bAlive; }
	bool IsCollideroff() const { return m_bCollideroff; }


	Vec2 GetPos() const { return m_vPos; }
	Vec2 GetScale() const { return m_vScale; }
	const wstring& GetName() const { return m_strName; }
	
	CCollider* GetCollider() { return m_pCollider; }
	CAnimator* GetAnimator() { return m_pAnimator; }

	void CreaeteCollider();
	void CreaeteAnimator();

	virtual void OnCollision(CCollider* _pOther) {}
	virtual void OnCollisionEnter(CCollider* _pOther){}
	virtual void OnCollisionExit(CCollider* _pOther){}

protected:
    void SetCollideroff() { m_bCollideroff = true; }
    void SetCollideron() { m_bCollideroff = false; }
    void SafeColliderdelete();
private:
	void SetDead() { m_bAlive = false; }
	void SetAlive() { m_bAlive = true; }
	

public:
	virtual void update()=0;
	virtual void finalupdate();
	virtual void render(HDC _dc);

	void component_render(HDC _dc);

	virtual CObject* Clone() = 0; // 복사 생성자는 자식이있는 클래스에서 구현되어있으면 안된다.

public:
	CObject();
	CObject(const CObject& _origin); // 자식 클래스 자체적인 복사 생성자를 구현하면 부모쪽 복사 생성자를 사용하겠다고 명시해야한다.
	virtual ~CObject(); // 자식 클래스의 소멸자도 호출 시키게 하기위해 소멸자 가상함수로 해야한다.
	friend class CEventMgr;
	friend class CPlayer;
};

