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
    const std::wstring fontDir = std::wstring(CPathMgr::GetInst()->GetContentPath()) + L"font\\";
    AddFontResourceEx((fontDir + L"BMHANNAPro.ttf").c_str(), FR_PRIVATE, nullptr);
    AddFontResourceEx((fontDir + L"BMHANNAAir_ttf.ttf").c_str(), FR_PRIVATE, nullptr);

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
    const std::wstring fontDir = std::wstring(CPathMgr::GetInst()->GetContentPath()) + L"font\\";
    RemoveFontResourceEx((fontDir + L"BMHANNAPro.ttf").c_str(), FR_PRIVATE, nullptr);
    RemoveFontResourceEx((fontDir + L"BMHANNAAir_ttf.ttf").c_str(), FR_PRIVATE, nullptr);
}

void CStageHUD::update()
{
    // 
}

void CStageHUD::render(HDC canvas_)
{
    if (!m_bIsActive)
    {
        return;
    }

    auto* scene = CSceneMgr::GetInst()->GetCurScene();
    auto* player1 = scene->GetPlayer(0);
    auto* player2 = scene->GetPlayer(1);
    assert(player1 != nullptr);
    assert(player2 != nullptr);

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
        HFONT hOldFont = (HFONT)SelectObject(canvas_, m_hLabelFont);
        SetBkMode(canvas_, TRANSPARENT);

        constexpr int heartSize = 18;
        constexpr int heartGap = 3;
        constexpr int heartCount = 3;
        constexpr int y = 40;
        const wchar_t* invincibleText = L"MUJEOK!!!";
        SIZE invincibleSize{};
        GetTextExtentPoint32(canvas_, invincibleText, 9, &invincibleSize);
        constexpr int heartsWidth = heartCount * heartSize + (heartCount - 1) * heartGap;
        const int healthWidth = heartsWidth > invincibleSize.cx ? heartsWidth : invincibleSize.cx;

        auto drawPlayer = [&](CPlayer* player, const wchar_t* label, bool rightAligned)
        {
            SIZE labelSize{};
            GetTextExtentPoint32(canvas_, label, static_cast<int>(wcslen(label)), &labelSize);
            const std::wstring level = std::format(L"Lv. {}", player->getLv());
            SIZE levelSize{};
            GetTextExtentPoint32(canvas_, level.c_str(), static_cast<int>(level.length()), &levelSize);
            const int groupWidth = labelSize.cx + healthWidth + 8 + levelSize.cx;
            const int x = rightAligned ? 890 - groupWidth : 390;

            SetTextColor(canvas_, RGB(255, 255, 255));
            TextOut(canvas_, x, y, label, static_cast<int>(wcslen(label)));
            const int healthX = x + labelSize.cx;
            if (player->isMujeok)
            {
                SetTextColor(canvas_, RGB(255, 255, 0));
                TextOut(canvas_, healthX, y, invincibleText, 9);
            }
            else
            {
                for (int i = 0; i < heartCount; ++i)
                {
                    CTexture* heart = i < player->getHP() ? m_pHeartFull : m_pHeartEmpty;
                    TransparentBlt(canvas_, healthX + i * (heartSize + heartGap), y,
                        heartSize, heartSize, heart->GetDC(), 0, 0,
                        heart->Width(), heart->Height(), RGB(255, 0, 255));
                }
            }

            SetTextColor(canvas_, RGB(255, 255, 255));
            TextOut(canvas_, healthX + healthWidth + 8, y, level.c_str(), static_cast<int>(level.length()));
        };

        drawPlayer(player1, L"P1 HP: ", false);
        drawPlayer(player2, L"P2 HP: ", true);
        SelectObject(canvas_, hOldFont);
    }
}
