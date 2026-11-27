#include "pch.h"
#include "CEventMgr.h"
#include "CSceneMgr.h"
#include "CScene.h"
#include "CUIMgr.h"

#include "AI.h"
#include "CState.h"


#include "CObject.h"
#include "CMonster.h"
CEventMgr::CEventMgr()
{

}
CEventMgr::~CEventMgr()
{

}

void CEventMgr::update()
{
	// =================================================
	// 이전 프레임에서 등록해둔 Dead Object 들을 삭제한다.
    // (render가 아닌 이벤트 경계에서 씬 컨테이너·슬롯 참조를 먼저 분리한 뒤 해제)
	// =================================================

    if (!m_vecDead.empty()) {
        if (CScene* pCurScene = CSceneMgr::GetInst()->GetCurScene()) {
            pCurScene->CleanupDeadObjects();
        }

        for (size_t i = 0; i < m_vecDead.size(); ++i)
        {

            delete(m_vecDead[i]);


        }
        m_vecDead.clear();
    }
    

	//============
	//Event 처리
	//===========

	/*for (size_t i = 0; i < m_vecEvent.size(); ++i)
	{
		Excute(m_vecEvent[i]);
	}*/

    // 11/20 
    int sceneChangeIdx = -1;
    for (size_t i = 0; i < m_vecEvent.size(); ++i) {
        if (m_vecEvent[i].eEven == EVENT_TYPE::SCENE_CHANGE) {
            sceneChangeIdx = static_cast<int>(i);
            break;
        }
    }

    if (sceneChangeIdx >= 0) {
        // 11/20 : 씬 전환 시 같은 프레임에 대기 중이던 이전 씬 지연 이벤트가 새 씬에 적용되지
        // 않도록 정리
        for (size_t i = 0; i < m_vecEvent.size(); ++i) {
            if (m_vecEvent[i].eEven == EVENT_TYPE::CREATE_OBJECT) {
                CObject* pPendingObj = (CObject*)m_vecEvent[i].lParam;
                delete pPendingObj;
            }
        }
        const tEvent sceneEvt = m_vecEvent[sceneChangeIdx];
        m_vecEvent.clear();
        Excute(sceneEvt);
        return;
    }

    for (size_t i = 0; i < m_vecEvent.size(); ++i) {
        Excute(m_vecEvent[i]);
    }

	m_vecEvent.clear();
}

void CEventMgr::Excute(const tEvent& _eve)
{
	
	switch (_eve.eEven)
	{
	case EVENT_TYPE::CREATE_OBJECT:
	{
		// lParam : Object Adress
		// wParam : Group 
		CObject* pNewObj = (CObject*)_eve.lParam;
		GROUP_TYPE eType = (GROUP_TYPE)_eve.wParam;
		CSceneMgr::GetInst()->GetCurScene()->AddObject(pNewObj, eType);
	}
		break;
	case EVENT_TYPE::DELETE_OBJECT:
	{
		// lParam : Object Adress
		// Object 를  Dead 상태로 변경
		// 삭제예정 오브젝트들을 모아둔다.
		CObject* pDeadObj = (CObject*)_eve.lParam;
        if (pDeadObj != nullptr && !pDeadObj->IsDead()) {
            pDeadObj->SetDead();
            m_vecDead.push_back(pDeadObj);
        }

       


	}
		break;
	case EVENT_TYPE::SCENE_CHANGE:
	{
		// lparam : Next Scene Type
		// Scene 변경
		CSceneMgr::GetInst()->ChangeScene((SCENE_TYPE)_eve.lParam);
		
		// 포커스 UI 헤제 (이전 SCENE을 가르키고있는 UI를 해제해준다)
		CUIMgr::GetInst()->SetFocusedUI(nullptr);
	}
		break;
    case EVENT_TYPE::CHANGE_AI_STATE:
    {
        // lParam : AI
        // wParam : Next type
        AI* pAI = (AI*)_eve.lParam;
        MON_STATE eNextState = (MON_STATE)_eve.wParam;

        if (pAI != nullptr && pAI->GetOwner() != nullptr
            && !pAI->GetOwner()->IsDead()) {
            pAI->ChangeState(eNextState);
        }
        
    }
    break;
	}

}
