#include "pch.h"
#include "CStageHUD.h"

#include "CPathMgr.h"
#include "CRankMgr.h"
#include "CPlayer.h"
#include "CResMgr.h"
#include "CTexture.h"
#include "CScene.h"
#include "CSceneMgr.h"

CStageHUD::CStageHUD()
    :CUI(false)
{
    // m_fontPath = CPathMgr::GetInst()->GetContentPath();
    // m_fontPath += L"font\\BMDOHYEON_ttf.ttf";

    // 폰트를 현재 프로세스에 등록
    AddFontResourceEx(L"font\\BMHANNAPro.ttf", FR_PRIVATE, nullptr);
    AddFontResourceEx(L"font\\BMHANNAAir.ttf", FR_PRIVATE, nullptr);

    m_hLabelFont = CreateFont(
        -15,            // height (픽셀 기준, 숫자를 키우면 글자가 커짐)
        0,             // width (0이면 자동)
        0, 0,          // escapement, orientation
        FW_BOLD,       // 굵기
        FALSE, FALSE, FALSE, // italic, underline, strikeout
        HANGUL_CHARSET,// charset
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"배달의민족 한나체 Pro"    // 글꼴 이름
    );

    m_hValueFont = CreateFont(
        -10,            // height (픽셀 기준, 숫자를 키우면 글자가 커짐)
        0,             // width (0이면 자동)
        0, 0,          // escapement, orientation
        FW_BOLD,       // 굵기
        FALSE, FALSE, FALSE, // italic, underline, strikeout
        HANGUL_CHARSET,// charset
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"배달의민족 한나체 Air"    // 글꼴 이름
    );

    m_pHeartFull = CResMgr::GetInst()->LoadTexture(L"Full Heart", L"texture\\Heart\\FullHeart.png");
    m_pHeartEmpty = CResMgr::GetInst()->LoadTexture(L"Empty Heart", L"texture\\Heart\\EmptyHeart.png");
}

CStageHUD::~CStageHUD()
{
    DeleteObject(m_hLabelFont);
    DeleteObject(m_hValueFont);
    RemoveFontResourceEx(L"font\\BMHANNAPro.ttf", FR_PRIVATE, nullptr);
    RemoveFontResourceEx(L"font\\BMHANNAAir.ttf", FR_PRIVATE, nullptr);
}

void CStageHUD::update()
{
    // 
}

void CStageHUD::render(HDC canvas_)
{
    if (!m_bIsActive)
        return;

    // 점수 띄우는 로직.
    {
        auto currentScore = CRankMgr::GetInst()->CurrentRanking.Score;
        auto hiScore = CRankMgr::GetInst()->GetFirstRanking().Score;
        // 텍스트 생성
        std::wstring str = L"SCORE: " + ToString(currentScore) + L"  HI-SCORE: " + ToString(hiScore);

        HFONT hOldFont = (HFONT)SelectObject(canvas_, m_hLabelFont);
        SetBkMode(canvas_, TRANSPARENT);
        SetTextColor(canvas_, RGB(255, 255, 255));

        int bgX = (1280 - 540) / 2;
        int y = 20;
        TextOut(canvas_, bgX + 20, y, str.c_str(), (int)str.length());
        SelectObject(canvas_, hOldFont);
    }
    {
        auto Player = static_cast<CPlayer*>(CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0));
        auto currentHP = Player->getHP();
        auto currentLv = Player->getLv();
        // HP: N/3 Lv. N

        std::wstring str = L"";
        if (Player->isMujeok)
        {
            str = std::format(L"HP: MUJEOK!!! Lv. {}", currentLv);
        }
        else
        {
            str = std::format(L"HP: {}/3 Lv. {}", currentHP, currentLv);
        }

        HFONT hOldFont = (HFONT)SelectObject(canvas_, m_hLabelFont);
        SetBkMode(canvas_, TRANSPARENT);
        SetTextColor(canvas_, RGB(255, 255, 255));

        int bgX = (1280 - 540) / 2;
        int y = 40;
        TextOut(canvas_, bgX + 20, y, str.c_str(), (int)str.length());
        SelectObject(canvas_, hOldFont);
    }
    // 하트 띄우는 로직.
    {
        const Vec2 position = GetPos();    // 중심 위치
        const Vec2 scale = GetScale();     // 하트 아이콘 크기 (픽셀 단위)

        auto Player = static_cast<CPlayer*>(CSceneMgr::GetInst()->GetCurScene()->GetPlayer(0));
        assert(Player != nullptr);
        auto currentHP = Player->getHP();

        const float heartSpacing = scale.x + 10.f;  // 하트 간 간격 (하트 크기 + 여백 10px)

        for (size_t i = 0; i < 3; ++i)
        {
            Vec2 heartPos = position;
            // 각 하트 아이콘을 좌측에서 우측으로 나열 (중심 위치 기준으로 좌측 정렬)
            heartPos.x = position.x - heartSpacing * (1.5f - i);

            if (i < currentHP)
            {
                // 채워진 하트 텍스쳐 출력
                const int width = m_pHeartFull->Width();
                const int height = m_pHeartFull->Height();

                const int drawWidth = static_cast<int>(scale.x);
                const int drawHeight = static_cast<int>(scale.y);

                const int drawX = static_cast<int>(heartPos.x - drawWidth / 2.0f);
                const int drawY = static_cast<int>(heartPos.y - drawHeight / 2.0f);

                // 투명색 지정 (예: RGB(255, 0, 255) - 보라색)
                COLORREF transparentColor = RGB(255, 0, 255);

                TransparentBlt(
                    canvas_,
                    drawX, drawY,
                    drawWidth, drawHeight,              // 출력 크기
                    m_pHeartFull->GetDC(),
                    0, 0,
                    width, height,                      // 원본 크기
                    transparentColor
                );
            }
            else
            {
                // 빈 하트 텍스쳐 출력
                const int width = m_pHeartEmpty->Width();
                const int height = m_pHeartEmpty->Height();

                const int drawWidth = static_cast<int>(scale.x);
                const int drawHeight = static_cast<int>(scale.y);

                const int drawX = static_cast<int>(heartPos.x - drawWidth / 2.0f);
                const int drawY = static_cast<int>(heartPos.y - drawHeight / 2.0f);

                // 투명색 지정 (예: RGB(255, 0, 255) - 보라색)
                COLORREF transparentColor = RGB(255, 0, 255);

                TransparentBlt(
                    canvas_,
                    drawX, drawY,
                    drawWidth, drawHeight,              // 출력 크기
                    m_pHeartEmpty->GetDC(),
                    0, 0,
                    width, height,                      // 원본 크기
                    transparentColor
                );
            }
        }
    }
}
