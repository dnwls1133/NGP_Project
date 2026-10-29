#pragma once
class CPathMgr
{
	SINGLE(CPathMgr);
private:
	std::wstring m_resourcePath;
	std::wstring m_contentPath;

public:
	void init();
	const wchar_t* GetContentPath() const { return m_contentPath.c_str(); }
	const wchar_t* GetResourcePath() const { return m_resourcePath.c_str(); }

    wstring GetRelativePath(const wchar_t* _filepath);
};


