#pragma once

/**
 * @brief 게임 랭킹 데이터.
 */
struct RankingData
{
    int Score;
    wstring DateTime;
};

class CPlayer;

class CRankMgr
{
    SINGLE(CRankMgr);
public:
    /**
     * @brief 랭킹 데이터 -> .txt 파일
     */
    void SaveRanking();

    /**
     * @brief .txt 파일 -> 랭킹 데이터.
     */
    void LoadRanking();

    /**
     * @brief 지정한 랭킹 데이터 삽입하기.
     * @param data_ 지정한 랭킹 데이터.
     */
    void AddRanking();

    /**
     * @brief 1등 데이터 가져오기.
     * @return 1등 데이터.
     */
    inline const RankingData& GetFirstRanking() const
    {
        return m_vecGameRankings[0];
    }

    /**
     * @brief 해당 번째 데이터 가져오기.
     * @param index_ 가져올 번째.
     * @return 해당 번째 랭킹 데이터.
     */
    inline const RankingData& GetRanking(size_t index_) const
    {
        return m_vecGameRankings[index_];
    }

    /**
     * @brief 랭킹 다 가져오기.
     * @return 모든 랭킹.
     */
    inline const auto& GetRankings() const
    {
        return m_vecGameRankings;
    }

    /**
     * @brief 현재 랭킹 데이터.
     */
    RankingData CurrentRanking;

    /**
     * @brief 플레이어 반환.
     * @return 플레이어.
     */
    CPlayer* GetPlayer()
    {
        return m_pPlayer;
    }

    /**
     * @brief 플레이어 설정.
     * @param player_ 설정할 플레이어.
     */
    inline void SetPlayer(CPlayer* const player_)
    {
        m_pPlayer = player_;
    }

    std::wstring GetCurrentDateTimeString();

private:
    /**
     * @brief 게임 랭킹.
     */
    vector<RankingData> m_vecGameRankings;

    /**
     * @brief 플레이어.
     */
    CPlayer* m_pPlayer;

    /**
     * @brief 더티 플래그.
     */
    bool m_bIsDirty = false;
};

