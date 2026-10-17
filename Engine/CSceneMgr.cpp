#include "pch.h"
#include "CSceneMgr.h"
#include "CScene_Title.h"
#include "CScene_Ranking.h"
#include "CScene_Start.h"
#include "CScene_Tool.h"
#include "CScene_Stage01.h"
#include "CScene_Manual.h"
#include "CScene_Credits.h"

CSceneMgr::CSceneMgr()
	: m_arrScene{}
	, m_pCurScene(nullptr)
	
{

}

CSceneMgr::~CSceneMgr()
{
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

    m_arrScene[static_cast<UINT>(SCENE_TYPE::STAGE_01)] = new CScene_Stage01;
    m_arrScene[static_cast<UINT>(SCENE_TYPE::STAGE_01)]->SetName(L"Stage01 Scene");

    m_arrScene[static_cast<UINT>(SCENE_TYPE::CREDITS)] = new CScene_Credits;
    m_arrScene[static_cast<UINT>(SCENE_TYPE::CREDITS)]->SetName(L"Credits Scene");

	m_arrScene[static_cast<UINT>(SCENE_TYPE::TOOL)] = new CScene_Tool;
	m_arrScene[static_cast<UINT>(SCENE_TYPE::TOOL)]->SetName(L"Tool Scene");

	//m_arrScene[(UINT)SCENE_TYPE::STAGE_01] = new CScene_Stage_01;
	//m_arrScene[(UINT)SCENE_TYPE::STAGE_02] = new CScene_Stage_02;

	m_pCurScene = m_arrScene[(UINT)SCENE_TYPE::TITLE];
	m_pCurScene->Enter();
}

void CSceneMgr::update()
{
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
}

void CSceneMgr::ChangeScene(SCENE_TYPE _eNext)
{
	m_pCurScene->Exit();
	m_pCurScene = m_arrScene[(UINT)_eNext];
	m_pCurScene->Enter();
}

