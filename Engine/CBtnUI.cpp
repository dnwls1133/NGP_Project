#include "pch.h"
#include "CBtnUI.h"



CBtnUI::CBtnUI()
    :CUI(false)
    , m_pSelectedFunc(nullptr)
    , m_param1(0)
    , m_param2(0)
    , m_pSceneFunc(nullptr)
    , m_pSceneInst(nullptr)
{
}

CBtnUI::~CBtnUI()
{
}


void CBtnUI::MouseOn()
{

}

void CBtnUI::MouseOff()
{
}

void CBtnUI::MouseLbtnDown()
{

}

void CBtnUI::MouseLbtnUP()
{

}

void CBtnUI::MouseLbtnClicked()
{
	if (nullptr != m_pSelectedFunc)
	{
		m_pSelectedFunc(m_param1,m_param2);
	}

    if (m_pSceneInst && m_pSceneFunc)
    {
        ((*m_pSceneInst).*m_pSceneFunc)();
    }
}

void CBtnUI::SetClickedCallBack(CScene* _pScene, SCENE_MEMFUNC _pSceneFunc)
{
    m_pSceneInst = _pScene;
    m_pSceneFunc = _pSceneFunc;
}
