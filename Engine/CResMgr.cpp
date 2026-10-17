#include "pch.h"
#include "CResMgr.h"

#include "CPathMgr.h"

#include "CResource.h"
#include "CTexture.h"
#include "CSound.h"

CResMgr::CResMgr()
{

}
CResMgr::~CResMgr()
{
    Safe_Delete_Map(m_Resources);
}

CTexture* CResMgr::LoadTexture(const wstring& _strKey, const wstring& _strRelativePath)
{

    CTexture* pTex = FindTexture(_strKey);
    if (nullptr != pTex)
    {
        return pTex;
    }
    wstring strFilPath = CPathMgr::GetInst()->GetContentPath();
    strFilPath += _strRelativePath;

    pTex = new CTexture;
    pTex->Load(strFilPath);
    pTex->SetKey(_strKey);
    pTex->SetRelativePath(_strRelativePath);
    m_Resources.insert(make_pair(_strKey, pTex));

    return pTex;
}

CTexture* CResMgr::CreateTexture(const wstring& _strKey, UINT _iWidth, UINT _iHeight)
{
    CTexture* pTex = FindTexture(_strKey);
    if (nullptr != pTex)
    {
        return pTex;
    }


    pTex = new CTexture;
    pTex->Create(_iWidth, _iHeight);
    pTex->SetKey(_strKey);
    m_Resources.insert(make_pair(_strKey, pTex));



    return pTex;
}

CTexture* CResMgr::FindTexture(const wstring& _strKey)
{
    map<wstring, CResource*>::iterator iter = m_Resources.find(_strKey);

    if (iter == m_Resources.end())
    {
        return nullptr;
    }
    return (CTexture*)iter->second;

}

CSound* const CResMgr::LoadSound(const wstring& key_, const wstring& relativePath_)
{
    CSound* pTex = FindSound(key_);
    if (nullptr != pTex)
    {
        return pTex;
    }
    wstring strFilPath = CPathMgr::GetInst()->GetContentPath();
    strFilPath += relativePath_;

    pTex = new CSound();
    pTex->Load(strFilPath);
    pTex->SetKey(key_);
    m_Resources.insert(make_pair(key_, pTex));
    return pTex;
}

CSound* const CResMgr::FindSound(const wstring& key_)
{
    auto iter = m_Resources.find(key_);
    if (iter == m_Resources.end())
    {
        return nullptr;
    }

    return dynamic_cast<CSound*>(iter->second);
}
