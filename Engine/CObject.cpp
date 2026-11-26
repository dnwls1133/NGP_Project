#include "pch.h"
#include "CObject.h"
#include "CKeyMgr.h"
#include "CTimeMgr.h"

#include "CCollider.h"
#include "CAnimator.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

CObject::CObject()
	: m_vPos{}
	, m_vScale{}
	, m_pCollider(nullptr)
	, m_strName{}
	, m_bAlive(true)
	, m_pAnimator(nullptr)
	, m_bCollideroff(false)
{

}

CObject::CObject(const CObject& _origin)
	: m_strName(_origin.m_strName)
	, m_vPos(_origin.m_vPos)
	, m_vScale(_origin.m_vScale)
	, m_pCollider(nullptr)
	, m_pAnimator(nullptr)
	, m_bAlive(true)
	, m_bCollideroff(false)
{
	if (_origin.m_pCollider)
	{
		m_pCollider = new CCollider(*_origin.m_pCollider);
		m_pCollider->m_pOwner = this;
	}
	
	
}

CObject::~CObject()
{
	if (nullptr != m_pCollider)
	{
		delete m_pCollider;
	}
	if (nullptr != m_pAnimator)
	{
		delete m_pAnimator;
	}
}

void CObject::CreaeteCollider()
{
	m_pCollider = new CCollider;
	m_pCollider->m_pOwner = this;
}

void CObject::CreaeteAnimator()
{
	m_pAnimator = new CAnimator;
	m_pAnimator->m_pOwner = this;
}

void CObject::SafeColliderdelete()
{
    if (nullptr != m_pCollider)
    {
        delete m_pCollider;
    }
}

bool CObject::ApplySnapshotPosition(Vec2 target, std::uint32_t tick, float duration)
{
    if (m_lastPositionTick && tick <= *m_lastPositionTick)
        return false;

    if (m_positionElapsed < m_positionDuration)
        m_vPos = m_positionStart + (m_positionTarget - m_positionStart) *
            (m_positionElapsed / m_positionDuration);

    m_lastPositionTick = tick;
    m_positionStart = m_vPos;
    m_positionTarget = target;
    m_positionDuration = duration;
    m_positionElapsed = 0.f;
    if (duration == 0.f)
        SetPos(target);

    return true;
}

void CObject::ResetSnapshotPosition()
{
    m_lastPositionTick.reset();
    m_positionDuration = 0.f;
}

void CObject::finalupdate()
{
    if (m_positionElapsed < m_positionDuration)
    {
        m_positionElapsed = min(m_positionElapsed + fDT, m_positionDuration);
        const float t = m_positionElapsed / m_positionDuration;
        SetPos(m_positionStart + (m_positionTarget - m_positionStart) * t);
    }

	if (m_pCollider)
	{
		m_pCollider->finalupdate();
	}
	if (m_pAnimator)
	{
		m_pAnimator->finalupdate();
	}
}

void CObject::render(HDC _dc)
{
	Rectangle(_dc, (int)(m_vPos.x - m_vScale.x / 2.f)
				 , (int)(m_vPos.y - m_vScale.y / 2.f)
				 , (int)(m_vPos.x + m_vScale.x / 2.f)
				 , (int)(m_vPos.y + m_vScale.y / 2.f));
	

}

void CObject::component_render(HDC _dc)
{
	if (nullptr != m_pAnimator)
	{
		m_pAnimator->render(_dc);
	}
	if (nullptr != m_pCollider)
	{
		m_pCollider->render(_dc);
	}

}
