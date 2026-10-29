#include "pch.h"
#include "CPathMgr.h"

#include "CCore.h"
#include <filesystem>
CPathMgr::CPathMgr()
{
    
}
CPathMgr::~CPathMgr()
{

};

void CPathMgr::init()
{
	std::vector<wchar_t> path(32768);
	DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
	if (length == 0 || length >= path.size())
	{
		MessageBoxW(nullptr, L"Unable to determine the executable path.", L"Path error", MB_OK | MB_ICONERROR);
		std::abort();
	}
	const auto executableDir = std::filesystem::path(path.data()).parent_path();
	const auto resourceDir = executableDir.filename() == L"Resource"
		? executableDir : executableDir.parent_path() / L"Resource";
	m_resourcePath = resourceDir.wstring() + L"\\";
	m_contentPath = (resourceDir / L"content").wstring() + L"\\";
}

wstring CPathMgr::GetRelativePath(const wchar_t* _filepath)
{
    wstring strFilePath = _filepath;

    size_t iAbsLen = m_contentPath.length();
    size_t iFullLen = strFilePath.length();

    wstring strRelativePaht = strFilePath.substr(iAbsLen,iFullLen);
    return strRelativePaht;
}

