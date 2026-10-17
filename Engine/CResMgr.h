#pragma once
class CResource;
class CTexture;
class CSound;
class CResMgr
{
	SINGLE(CResMgr);
private:
	map<wstring, CResource*> m_Resources;

public:
	CTexture* LoadTexture(const wstring& _strKey, const wstring& _strRelativePath);
    CTexture* CreateTexture(const wstring& _strKey, UINT _iWidth, UINT _iHeight);

	CTexture* FindTexture(const wstring& _strKey);

    CSound* const LoadSound(const wstring& key_, const wstring& relativePath_);
    CSound* const FindSound(const wstring& key_);

};

