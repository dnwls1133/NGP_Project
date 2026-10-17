#include "pch.h"
#include "CUIMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CUI.h"

#include "CKeyMgr.h"
CUIMgr::CUIMgr()
	: m_pFocusedUI(nullptr)
{

}
CUIMgr::~CUIMgr()
{

}

void CUIMgr::update()
{
    
	// 1. FocusedUI 확인 
	m_pFocusedUI = GetFocusedUI();
  
	if (!m_pFocusedUI)
	{
		return;
	}

	bool bLbtnTap = KEY_TAP(KEY::LBTN);
	bool bLbtnAway = KEY_AWAY(KEY::LBTN);
	
	

	// 부모 UI 포함, 자식 UI 들 중 실제 타갯팅 된 UI르 가져온다.
	CUI* pTargetUI = GetTargetedUI(m_pFocusedUI);


	if (pTargetUI != nullptr)
	{
		pTargetUI->MouseOn();

		if (bLbtnTap)
		{
			pTargetUI->MouseLbtnDown();
			pTargetUI->m_bLbtnDown = true;
		}
		else if (bLbtnAway)
		{
			pTargetUI->MouseLbtnUP();
			if (pTargetUI->m_bLbtnDown)
			{
				pTargetUI->MouseLbtnClicked();
			}

			pTargetUI->m_bLbtnDown = false;
		}
	}
	
	
}

void CUIMgr::SetFocusedUI(CUI* _pUI)
{
	// 이미 포커싱 중인 경우 or 포커싱 해제요청인 경우
	if (m_pFocusedUI == _pUI || nullptr == _pUI)
	{
		m_pFocusedUI = _pUI;
		return;
	}

	m_pFocusedUI = _pUI;


	CScene* pCurScene = CSceneMgr::GetInst()->GetCurScene();
	vector<CObject*>& vecUI = pCurScene->GetUIGroup();
	
	vector<CObject*>::iterator iter = vecUI.begin();

	for (; iter != vecUI.end(); ++iter)
	{
		if (m_pFocusedUI == *iter)
		{
			break;
		}


	}

	// 벡터 내에서 맨 뒤로 순번 교체
	vecUI.erase(iter);
	vecUI.push_back(m_pFocusedUI);


}

CUI* CUIMgr::GetFocusedUI()
{
	CScene* pCurScene = CSceneMgr::GetInst()->GetCurScene();
	vector<CObject*>& vecUI = pCurScene->GetUIGroup();

	// 기존 포커싱 UI 를 받아두고 변경되었는지 확인한다.
	CUI* pFocusedUI = m_pFocusedUI;
	bool bLbtnTap = KEY_TAP(KEY::LBTN);
	bool bLbtnAway = KEY_AWAY(KEY::LBTN);

	/*if (!bLbtnTap)
	{
		return pFocusedUI;
	}*/

	// 왼쪽버튼 TAP 이 발생했다는 전재
	vector<CObject*>::iterator targetiter = vecUI.end();
	vector<CObject*>::iterator iter = vecUI.begin();

	for (; iter != vecUI.end(); ++iter)
	{
		if(((CUI*)*iter)->IsMouseOn())
		{
			targetiter = iter;
		}

		
	}

	if (vecUI.end() == targetiter)
	{
		return nullptr;
	}

	pFocusedUI = (CUI*)*targetiter;

	// 벡터 내에서 맨 뒤로 순번 교체
	vecUI.erase(targetiter);
	vecUI.push_back(pFocusedUI);
	return pFocusedUI;
}

CUI* CUIMgr::GetTargetedUI(CUI* _pParentUI)
{
	bool bLbtnAway = KEY_AWAY(KEY::LBTN);

	CUI* pTargetUI = nullptr;
	// 1. 부모 UI 포함, 모든 자식들을 검사 한다.
	// 2. 타갯 UI 들 중, 더 우선순위가 높은 기준은 더 낮은 계층의 자식 UI
	// Level 순회
	// Queue를 사용
	static list<CUI*> queue;
	static vector<CUI*> vecNoneTargetUI;

	queue.clear();
	vecNoneTargetUI.clear();


	queue.push_back(_pParentUI);
	
	
	while (!queue.empty())
	{
		// 큐에서 데이터 하나 꺼내기
		CUI* pUI  = queue.front();
		queue.pop_front();

		// 큐에서 꺼내온 UI 가 TargetUI 인지 확인
		if (pUI->IsMouseOn())
		{
			if (nullptr != pTargetUI)
			{
				vecNoneTargetUI.push_back(pTargetUI);
			}
			pTargetUI = pUI;
		}
		else
		{
			vecNoneTargetUI.push_back(pUI);
		}


		const vector<CUI*>& vecChild = pUI->GetChildUI();
		for (size_t i = 0; i < vecChild.size(); ++i)
		{
			queue.push_back(vecChild[i]);
		}

	}
	if (bLbtnAway)
	{
		for (size_t i = 0; i < vecNoneTargetUI.size(); ++i)
		{
			vecNoneTargetUI[i]->m_bLbtnDown = false;
		}
		
	}
	

	





	return pTargetUI;
}
