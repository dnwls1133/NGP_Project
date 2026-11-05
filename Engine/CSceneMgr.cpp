#include "pch.h"
#include "CSceneMgr.h"

#include "CScene_Credits.h"
#include "CScene_Lobby.h"
#include "CScene_Manual.h"
#include "CScene_Ranking.h"
#include "CScene_Stage01.h"
#include "CScene_Start.h"
#include "CScene_Title.h"
#include "CScene_Tool.h"

#include "CLoadingBackground.h"
#include "CTimeMgr.h"

CSceneMgr::CSceneMgr()
	: m_arrScene{}
	, m_pCurScene(nullptr)
	
{

}

CSceneMgr::~CSceneMgr()
{
	delete m_pBackground;
	// 씬 전부 삭제
	for (UINT i = 0; i < (UINT)SCENE_TYPE::END; ++i)
	{
		if (nullptr != m_arrScene[i])
		{
			delete m_arrScene[i];
		}
		
	}
}

void CSceneMgr::init()
{
	// SCene 생성
    m_arrScene[static_cast<UINT>(SCENE_TYPE::TITLE)] = new CScene_Title;
    m_arrScene[static_cast<UINT>(SCENE_TYPE::TITLE)]->SetName(TEXT("Title Scene"));

    m_arrScene[static_cast<UINT>(SCENE_TYPE::MANUAL)] = new CScene_Manual;
    m_arrScene[static_cast<UINT>(SCENE_TYPE::MANUAL)]->SetName(L"Manual Scene");

    m_arrScene[static_cast<UINT>(SCENE_TYPE::RANKING)] = new CScene_Ranking;
    m_arrScene[static_cast<UINT>(SCENE_TYPE::RANKING)]->SetName(L"Ranking Scene");

	m_arrScene[static_cast<UINT>(SCENE_TYPE::START)] = new CScene_Start;
	m_arrScene[static_cast<UINT>(SCENE_TYPE::START)]->SetName(L"Start Scene");

    m_arrScene[static_cast<UINT>(SCENE_TYPE::LOBBY)] = new CScene_Lobby;
    m_arrScene[static_cast<UINT>(SCENE_TYPE::LOBBY)]->SetName(L"Lobby Scene");

    m_arrScene[static_cast<UINT>(SCENE_TYPE::STAGE_01)] = new CScene_Stage01;
    m_arrScene[static_cast<UINT>(SCENE_TYPE::STAGE_01)]->SetName(L"Stage01 Scene");

    m_arrScene[static_cast<UINT>(SCENE_TYPE::CREDITS)] = new CScene_Credits;
    m_arrScene[static_cast<UINT>(SCENE_TYPE::CREDITS)]->SetName(L"Credits Scene");

	m_arrScene[static_cast<UINT>(SCENE_TYPE::TOOL)] = new CScene_Tool;
	m_arrScene[static_cast<UINT>(SCENE_TYPE::TOOL)]->SetName(L"Tool Scene");

	//m_arrScene[(UINT)SCENE_TYPE::STAGE_01] = new CScene_Stage_01;
	//m_arrScene[(UINT)SCENE_TYPE::STAGE_02] = new CScene_Stage_02;
    m_pBackground = new CLoadingBackground();

	m_pCurScene = m_arrScene[(UINT)SCENE_TYPE::TITLE];
	m_pCurScene->Enter();
}

void CSceneMgr::update()
{
    constexpr float fadeSpeed = 510.0f;
    const float alphaDelta = CTimeMgr::GetInst()->GetfDT() * fadeSpeed;

    if (m_pNextScene)
    {
        m_pBackground->SetAlpha(m_pBackground->GetAlpha() + alphaDelta);

        if (m_pBackground->GetAlpha() >= 255)
        {
            m_pBackground->SetAlpha(255);
            m_pCurScene->Exit();
            m_pCurScene = m_pNextScene;

            m_pCurScene->Enter();
            m_pNextScene = nullptr;
        }
    }
    else if (m_pBackground->IsActive())
    {
        m_pBackground->SetAlpha(m_pBackground->GetAlpha() - alphaDelta);

        if (m_pBackground->GetAlpha() <= 0)
        {
            m_pBackground->SetAlpha(0);
            m_pBackground->Close();
        }
    }

    m_pBackground->Update();

	m_pCurScene->update();
	finalupdate();
}

void CSceneMgr::finalupdate()
{
	m_pCurScene->finalupdate();
}

void CSceneMgr::render(HDC _dc)
{
	m_pCurScene->render(_dc);
    m_pBackground->Render(_dc);
}

void CSceneMgr::ChangeScene(SCENE_TYPE _eNext)
{
    if (m_pBackground->IsActive())
    {
        return;
    }

    m_pNextScene = m_arrScene[(UINT)_eNext];
    m_pBackground->Open();
}

