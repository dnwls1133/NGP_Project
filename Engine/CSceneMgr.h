#pragma once

class CScene;
class CLoadingBackground;

class CSceneMgr // 씬을 전환시켜주는 역활
{
	SINGLE(CSceneMgr);
private:
	CScene*		m_arrScene[(UINT)SCENE_TYPE::END];     // 모든 씬 목록
	CScene*		m_pCurScene;					       // 현재 씬
    CScene*     m_pNextScene = nullptr;				       // 다음 씬

    CLoadingBackground* m_pBackground = nullptr;

public:
	void init();
	void update();
	void finalupdate();
	void render(HDC _dc);

public:
	CScene* GetCurScene() { return m_pCurScene; }
	
private:
	void ChangeScene(SCENE_TYPE _eNext);

	friend class CEventMgr;

};

