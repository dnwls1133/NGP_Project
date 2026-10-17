#include "pch.h"
#include "CObject.h"
#include "CKeyMgr.h"
#include "CTimeMgr.h"

#include "CCollider.h"
#include "CAnimator.h"
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

void CObject::finalupdate()
{
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
