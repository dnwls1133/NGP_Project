#pragma once

class CObject;
class AI;
class CCollider;

void CreateObject(CObject* _pObj,GROUP_TYPE _eGroup);
void DeleteObject(CObject* _pObj);

void ChangeScene(SCENE_TYPE _eNext);
void ChangeAIState(AI* _pAI, MON_STATE _eNextState);



template<typename T>
void Safe_Delete_Vec(vector<T>& _vec)
{
	for (size_t i = 0; i < _vec.size(); ++i)
	{
		if (nullptr != _vec[i])
		{
			delete _vec[i];
		}
	}
	_vec.clear();
}

template<typename T1,typename T2>
void Safe_Delete_Map(map<T1, T2>& _map)
{
	typename map<T1, T2>::iterator iter = _map.begin();

	for (; iter != _map.end(); ++iter)
	{
		if (nullptr != iter->second)
		{
			delete iter->second;
		}
	}
	_map.clear();
}



void FScanf(char* _pOutBuff, FILE* _pFile);
void SaveWString(const wstring& _str, FILE* _pFile);
void LoadWString(wstring& _str,FILE* _pFile);

inline const std::wstring ToString(int score)
{
    std::wstring result;
    int count = 0;

    if (score == 0)
        return L"0";

    while (score > 0) {
        if (count > 0 && count % 3 == 0)
            result = L"'" + result;

        result = std::to_wstring(score % 10) + result;
        score /= 10;
        ++count;
    }

    return result;
}


#include "CRankMgr.h"


inline const auto& GetFirstRanking()
{
    return CRankMgr::GetInst()->GetFirstRanking();
}

inline const auto& GetCurrentRanking()
{
    return CRankMgr::GetInst()->CurrentRanking;
}

inline const auto& GetRanking(size_t index_)
{
    return CRankMgr::GetInst()->GetRanking(index_);
}

inline const auto& GetRankings()
{
    return CRankMgr::GetInst()->GetRankings();
}
