#pragma once
class CCollider;

union COLLIDER_ID
{
	struct {
		UINT Left_id;
		UINT Right_id;
	};
	ULONGLONG ID;
};

class CColliderMgr
{
	SINGLE(CColliderMgr)
private:
	// 충돌체 간의 이전 프레임 충돌 정보
	map<ULONGLONG, bool> m_mapCollInfo;
	UINT m_arrCheck[(UINT)GROUP_TYPE::END]; // 그룹간의 충돌 체크 메트릭스
public:
	void update();
	void CheckGroup(GROUP_TYPE _eLeft, GROUP_TYPE _eRight);
	void Reset()
	{
		memset(m_arrCheck, 0, sizeof(UINT) * (UINT)GROUP_TYPE::END);
	}

private:
	void CollisionGroupUpdate(GROUP_TYPE _eLeft, GROUP_TYPE _eRight);
	bool isCollision(CCollider* _pLeftCol, CCollider* _pRightCol,CObject* _pLeftObj);
};

