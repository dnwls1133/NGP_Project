#pragma once

#include <functional>
#include <vector>

#include <windef.h>

class CMenuItem;

enum class ETransitionState : unsigned char
{
    Idle,
    Open,
    OpenComplete,
    Close,
    CloseComplete,
};

class CMenuTransition
{
public:
    using TransitionCallback = std::function<void()>;
    using TransitionStep = std::function<bool()>;

    void AddMenuItem(CMenuItem& item);

    [[nodiscard]] std::vector<CMenuItem*>& GetMenuItems() noexcept;
    [[nodiscard]] const std::vector<CMenuItem*>& GetMenuItems() const noexcept;

    void Update();
    void Render(HDC _dc);

    [[nodiscard]] ETransitionState GetCurrentState() const noexcept;

    [[nodiscard]] bool IsPlaying() const noexcept;

    void Open(TransitionStep step = {});
    void Close(TransitionStep step = {});

    [[nodiscard]] TransitionCallback& OnOpen() noexcept;
    [[nodiscard]] const TransitionCallback& OnOpen() const noexcept;

    [[nodiscard]] TransitionCallback& OnOpenComplete() noexcept;
    [[nodiscard]] const TransitionCallback& OnOpenComplete() const noexcept;

    [[nodiscard]] TransitionCallback& OnClose() noexcept;
    [[nodiscard]] const TransitionCallback& OnClose() const noexcept;

    [[nodiscard]] TransitionCallback& OnCloseComplete() noexcept;
    [[nodiscard]] const TransitionCallback& OnCloseComplete() const noexcept;

private:
    void OpenComplete();
    void CloseComplete();

    std::vector<CMenuItem*> m_menuItems;

    ETransitionState m_currentState = ETransitionState::Idle;

    TransitionStep m_step;
    TransitionCallback m_onOpen;
    TransitionCallback m_onOpenComplete;
    TransitionCallback m_onClose;
    TransitionCallback m_onCloseComplete;
};

inline std::vector<CMenuItem*>& CMenuTransition::GetMenuItems() noexcept
{
    return m_menuItems;
}

inline const std::vector<CMenuItem*>& CMenuTransition::GetMenuItems() const noexcept
{
    return m_menuItems;
}

inline ETransitionState CMenuTransition::GetCurrentState() const noexcept
{
    return m_currentState;
}

inline bool CMenuTransition::IsPlaying() const noexcept
{
    return (m_currentState == ETransitionState::Open) ||
           (m_currentState == ETransitionState::Close);
}

inline CMenuTransition::TransitionCallback& CMenuTransition::OnOpen() noexcept
{
    return m_onOpen;
}

inline const CMenuTransition::TransitionCallback& CMenuTransition::OnOpen() const noexcept
{
    return m_onOpen;
}

inline std::function<void()>& CMenuTransition::OnOpenComplete() noexcept
{
    return m_onOpenComplete;
}

inline const std::function<void()>& CMenuTransition::OnOpenComplete() const noexcept
{
    return m_onOpenComplete;
}

inline std::function<void()>& CMenuTransition::OnClose() noexcept
{
    return m_onClose;
}

inline const std::function<void()>& CMenuTransition::OnClose() const noexcept
{
    return m_onClose;
}

inline std::function<void()>& CMenuTransition::OnCloseComplete() noexcept
{
    return m_onCloseComplete;
}

inline const std::function<void()>& CMenuTransition::OnCloseComplete() const noexcept
{
    return m_onCloseComplete;
}
