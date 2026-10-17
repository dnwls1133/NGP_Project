#include "pch.h"
#include "CRankMgr.h"

CRankMgr::CRankMgr()
{
    
}

CRankMgr::~CRankMgr()
{

}

void CRankMgr::LoadRanking()
{
    FILE* pFile = nullptr;
    _wfopen_s(&pFile, L"Ranking.txt", L"rt");
    assert(pFile);

    wchar_t szLine[128];
    m_vecGameRankings.clear();

    while (fgetws(szLine, 128, pFile))
    {
        if (wcsncmp(szLine, L"[Record]", 8) == 0)
        {
            int score = 0;
            std::wstring date;

            fgetws(szLine, 128, pFile); // [Score]
            fgetws(szLine, 128, pFile);
            score = _wtoi(szLine);

            fgetws(szLine, 128, pFile); // [Date]
            fgetws(szLine, 128, pFile);
            date = szLine;
            if (!date.empty() && date.back() == L'\n')
                date.pop_back();

            m_vecGameRankings.emplace_back(score, date);
        }
    }

    fclose(pFile);
}

void CRankMgr::SaveRanking()
{
    if (!m_bIsDirty)
        return;

    FILE* pFile = nullptr;
    _wfopen_s(&pFile, L"Ranking.txt", L"wt");
    assert(pFile);

    fwprintf(pFile, L"[Ranking_Count]\n");
    fwprintf(pFile, L"%d\n\n", (int)m_vecGameRankings.size());

    for (const auto& rec : m_vecGameRankings) {
        fwprintf(pFile, L"[Record]\n");
        fwprintf(pFile, L"[Score]\n%d\n", rec.Score);
        fwprintf(pFile, L"[Date]\n%ls\n\n", rec.DateTime.c_str());
    }

    fclose(pFile);
}

void CRankMgr::AddRanking()
{
    CurrentRanking.DateTime = GetCurrentDateTimeString();
    m_vecGameRankings.push_back(CurrentRanking);

    std::sort(m_vecGameRankings.begin(), m_vecGameRankings.end(),
        [](const RankingData& a, const RankingData& b) {
            return a.Score > b.Score; // 내림차순
        });

    if (m_vecGameRankings.size() > 3) {
        m_vecGameRankings.resize(3);
    }

    m_bIsDirty = true;

    SaveRanking(); // 파일에 저장

    CurrentRanking.Score = 0; // 그 후 초기화
    CurrentRanking.DateTime = L"";
}

std::wstring CRankMgr::GetCurrentDateTimeString()
{
    std::time_t t = std::time(nullptr);
    std::tm localTime;
    localtime_s(&localTime, &t);

    wchar_t buffer[64];
    // 직접 포맷팅 (wcsftime는 wchar_t 기반)
    wcsftime(buffer, sizeof(buffer) / sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S", &localTime);

    return std::wstring(buffer);
}
