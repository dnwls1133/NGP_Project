#pragma once


#include "CCamera.h"

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
public:
	void SetPos(Vec2 _vPos) { m_vPos = _vPos; }
	void SetScale(Vec2 _vScale) { m_vScale = _vScale; }
	void SetName(const wstring& _strName) { m_strName = _strName; }
	
	bool IsDead() { return !m_bAlive; }
	bool IsCollideroff() { return m_bCollideroff; }


	Vec2 GetPos() { return m_vPos; }
	Vec2 GetScale() { return m_vScale; }
	const wstring& GetName() { return m_strName; }
	
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

