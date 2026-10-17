#include "pch.h"
#include "CPathMgr.h"

#include "CCore.h"
CPathMgr::CPathMgr()
	: m_szContentPath{}
{
    
}
CPathMgr::~CPathMgr()
{

};

void CPathMgr::init()
{
	GetCurrentDirectory(255, m_szContentPath); // 실행파일이 있는 디렉토리 자동으로 찾아주는 메크로 
    
	int iLen = (int)wcslen(m_szContentPath);

	for (int i = iLen - 1; i >= 0; --i)
	{
		if ('\\' == m_szContentPath[i])
		{
			m_szContentPath[i] = '\0';
			break;
		}
	}

	wcscat_s(m_szContentPath, 255, L"\\bin\\content\\");
	// 상위폴더로

	// + bin\\content\\



	//SetWindowText(CCore::GetInst()->GetMainHwnd(), m_szContentPath);
}

wstring CPathMgr::GetRelativePath(const wchar_t* _filepath)
{
    wstring strFilePath = _filepath;

    size_t iAbsLen = wcslen(m_szContentPath);
    size_t iFullLen = strFilePath.length();

    wstring strRelativePaht = strFilePath.substr(iAbsLen,iFullLen);
    return strRelativePaht;
}

