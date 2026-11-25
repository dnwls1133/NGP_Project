#pragma once

#include <functional>

#include <windef.h>

enum class ETransitionState : unsigned char
{
    Idle,
    Open,
    OpenComplete,
    Close,
    CloseComplete,
};

class CTransitionBase
{
public:
    using TransitionCallback = std::function<void()>;
    using TransitionStep = std::function<bool()>;

    virtual ~CTransitionBase() = default;

    virtual void Update();
    virtual void Render(HDC _dc) = 0;

    virtual void Open(TransitionStep step = {});
    virtual void OpenComplete();
    virtual void Close(TransitionStep step = {});
    virtual void CloseComplete();

    [[nodiscard]] ETransitionState GetCurrentState() const noexcept;
    [[nodiscard]] bool IsPlaying() const noexcept;

    [[nodiscard]] TransitionCallback& OnOpen() noexcept;
    [[nodiscard]] const TransitionCallback& OnOpen() const noexcept;

    [[nodiscard]] TransitionCallback& OnOpenComplete() noexcept;
    [[nodiscard]] const TransitionCallback& OnOpenComplete() const noexcept;

    [[nodiscard]] TransitionCallback& OnClose() noexcept;
    [[nodiscard]] const TransitionCallback& OnClose() const noexcept;

    [[nodiscard]] TransitionCallback& OnCloseComplete() noexcept;
    [[nodiscard]] const TransitionCallback& OnCloseComplete() const noexcept;

private:
    ETransitionState m_currentState = ETransitionState::Idle;

    TransitionStep m_step;

    TransitionCallback m_onOpen;
    TransitionCallback m_onOpenComplete;
    TransitionCallback m_onClose;
    TransitionCallback m_onCloseComplete;
};

inline ETransitionState CTransitionBase::GetCurrentState() const noexcept
{
    return m_currentState;
}

inline bool CTransitionBase::IsPlaying() const noexcept
{
    return (m_currentState == ETransitionState::Open) ||
           (m_currentState == ETransitionState::Close);
}

inline CTransitionBase::TransitionCallback& CTransitionBase::OnOpen() noexcept
{
    return m_onOpen;
}

inline const CTransitionBase::TransitionCallback& CTransitionBase::OnOpen() const noexcept
{
    return m_onOpen;
}

inline std::function<void()>& CTransitionBase::OnOpenComplete() noexcept
{
    return m_onOpenComplete;
}

inline const std::function<void()>& CTransitionBase::OnOpenComplete() const noexcept
{
    return m_onOpenComplete;
}

inline std::function<void()>& CTransitionBase::OnClose() noexcept
{
    return m_onClose;
}

inline const std::function<void()>& CTransitionBase::OnClose() const noexcept
{
    return m_onClose;
}

inline std::function<void()>& CTransitionBase::OnCloseComplete() noexcept
{
    return m_onCloseComplete;
}

inline const std::function<void()>& CTransitionBase::OnCloseComplete() const noexcept
{
    return m_onCloseComplete;
}
