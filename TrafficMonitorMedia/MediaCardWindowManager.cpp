#include "pch.h"
#include "MediaCardWindowManager.h"

#include "MediaCardInteractionState.h"
#include "MediaCardWindowBehavior.h"
#include "MediaCardWnd.h"
#include "TrafficMonitorMedia.h"

namespace
{
    [[nodiscard]] HWND ResolveMediaCardOwner(HWND anchor_window) noexcept
    {
        if (anchor_window == nullptr || !IsWindow(anchor_window))
        {
            return nullptr;
        }

        const HWND root_window = GetAncestor(anchor_window, GA_ROOT);
        if (root_window == nullptr || !IsWindow(root_window))
        {
            return nullptr;
        }

        wchar_t class_name[64]{};
        const int length = GetClassNameW(root_window, class_name, static_cast<int>(std::size(class_name)));
        if (length <= 0 || !media::IsTrustedMediaCardOwnerClass(
                std::wstring_view(class_name, static_cast<std::size_t>(length))))
        {
            return nullptr;
        }
        return root_window;
    }
}

CMediaCardWindowManager* CMediaCardWindowManager::s_scheduled_manager = nullptr;
CMediaCardWindowManager* CMediaCardWindowManager::s_animating_manager = nullptr;

CMediaCardWindowManager::CMediaCardWindowManager()
    : m_card(std::make_unique<CMediaCardWnd>(this))
{
}

CMediaCardWindowManager::~CMediaCardWindowManager()
{
    ForceClose();
}

void CMediaCardWindowManager::ScheduleOpen(
    HWND anchor_window,
    int client_x,
    int client_y,
    unsigned int confirmation_delay_milliseconds)
{
    CancelScheduledOpen();
    if (!media::ShouldScheduleMediaCardOpen(
            GetTickCount64(),
            m_open_suppression_deadline_milliseconds))
    {
        return;
    }

    if (confirmation_delay_milliseconds == 0)
    {
        Open(anchor_window, client_x, client_y);
        return;
    }

    m_pending_anchor = anchor_window;
    m_pending_client_point = CPoint(client_x, client_y);
    s_scheduled_manager = this;
    m_open_timer = SetTimer(
        nullptr,
        0,
        confirmation_delay_milliseconds,
        &CMediaCardWindowManager::OpenTimerProc);
    if (m_open_timer == 0)
    {
        s_scheduled_manager = nullptr;
        Open(anchor_window, client_x, client_y);
    }
}

void CMediaCardWindowManager::CancelScheduledOpen()
{
    if (m_open_timer != 0)
    {
        KillTimer(nullptr, m_open_timer);
        m_open_timer = 0;
    }
    if (s_scheduled_manager == this)
    {
        s_scheduled_manager = nullptr;
    }
    m_pending_anchor = nullptr;
}

void CMediaCardWindowManager::SuppressScheduledOpenAfterDoubleClick()
{
    CancelScheduledOpen();
    m_open_suppression_deadline_milliseconds =
        media::CalculateMediaCardOpenSuppressionDeadline(GetTickCount64(), GetDoubleClickTime());
}

void CMediaCardWindowManager::Open(HWND anchor_window, int client_x, int client_y)
{
    CancelScheduledOpen();

    CPoint anchor_point(client_x, client_y);
    if (anchor_window != nullptr && IsWindow(anchor_window))
    {
        ::ClientToScreen(anchor_window, &anchor_point);
    }
    else
    {
        GetCursorPos(&anchor_point);
    }
    OpenAtScreenPoint(anchor_window, anchor_point);
}

void CMediaCardWindowManager::OpenAtScreenPoint(HWND anchor_window, CPoint anchor_point)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    DestroyCardWindow();

    const HMONITOR monitor = MonitorFromPoint(anchor_point, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{ sizeof(MONITORINFO) };
    if (!GetMonitorInfo(monitor, &monitor_info))
    {
        monitor_info.rcWork = {
            0,
            0,
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN),
        };
    }

    const int width = g_plugin.DPI(380);
    const int height = g_plugin.DPI(236);
    const int gap = g_plugin.DPI(8);
    const media::PopupPlacement placement = media::CalculatePopupPlacement(
        anchor_point.x,
        anchor_point.y,
        width,
        height,
        gap,
        media::PixelRect{
            monitor_info.rcWork.left,
            monitor_info.rcWork.top,
            monitor_info.rcWork.right,
            monitor_info.rcWork.bottom,
        });

    const CRect card_rect(placement.x, placement.y, placement.x + width, placement.y + height);
    const HWND owner_window = ResolveMediaCardOwner(anchor_window);
    ++m_card_message_generation;
    if (m_card_message_generation == 0)
    {
        ++m_card_message_generation;
    }
    if (!m_card->Create(owner_window, card_rect, m_card_message_generation))
    {
        DestroyCardWindow();
        return;
    }

    g_plugin.SetMediaCardVisible(true);
    m_card_marked_visible = true;
    m_card->RefreshSnapshot();
    m_card->SetInteractionEnabled(true);
    m_activation_compensation_attempted = false;
    m_opening_animation_completed = false;
    StartAnimation(AnimationKind::Opening, card_rect);
    m_card->ShowWindow(media::kMediaCardShowCommand);
    SetForegroundWindow(m_card->GetSafeHwnd());
    if (!m_card->BeginActivationVerification())
    {
        ForceClose();
    }
}

void CMediaCardWindowManager::Close()
{
    CancelScheduledOpen();
    if (m_state == WindowState::Closed || m_state == WindowState::Closing)
    {
        return;
    }
    if (!HasCardWindow())
    {
        DestroyCardWindow();
        return;
    }

    m_card->CancelSeekPreview();
    m_card->SetInteractionEnabled(false);
    CRect current_rect;
    m_card->GetWindowRect(&current_rect);
    StartAnimation(AnimationKind::Closing, current_rect);
}

void CMediaCardWindowManager::ForceClose()
{
    CancelScheduledOpen();
    DestroyCardWindow();
}

void CMediaCardWindowManager::OnCardDestroyed()
{
    if (media::MediaCardLifecycleState{
            m_close_in_progress,
            m_card_marked_visible,
        }.ShouldHandleUnexpectedCardDestroy())
    {
        ForceClose();
    }
}

void CMediaCardWindowManager::OnCardDeactivated(
    HWND card_window,
    std::uintptr_t message_generation)
{
    if (!media::IsCurrentMediaCardMessageGeneration(
            message_generation,
            m_card_message_generation)
        || !IsWindow(card_window)
        || !IsCurrentCard(card_window)
        || (m_state != WindowState::Opening && m_state != WindowState::Open)
        || !m_card->HasBeenActivated()
        || GetForegroundWindow() == card_window)
    {
        return;
    }

    Close();
}

void CMediaCardWindowManager::OnCardActivationVerification(
    HWND card_window,
    std::uintptr_t message_generation)
{
    if (!media::IsCurrentMediaCardMessageGeneration(
            message_generation,
            m_card_message_generation)
        || !IsWindow(card_window)
        || !IsCurrentCard(card_window)
        || (m_state != WindowState::Opening && m_state != WindowState::Open))
    {
        return;
    }
    if (m_card->HasBeenActivated())
    {
        if (media::IsMediaCardActivationVerified(
                true,
                GetForegroundWindow() == card_window))
        {
            TryCompleteOpening();
        }
        else
        {
            Close();
        }
        return;
    }
    if (m_activation_compensation_attempted)
    {
        ForceClose();
        return;
    }

    m_activation_compensation_attempted = true;
    SetForegroundWindow(card_window);
    if (GetForegroundWindow() == card_window
        && GetWindowThreadProcessId(card_window, nullptr) == GetCurrentThreadId())
    {
        SetActiveWindow(card_window);
    }
    if (!m_card->BeginActivationVerification())
    {
        ForceClose();
    }
}

bool CMediaCardWindowManager::IsCurrentCard(HWND card_window) const noexcept
{
    return card_window != nullptr
        && m_card != nullptr
        && m_card->GetSafeHwnd() == card_window;
}

bool CMediaCardWindowManager::IsOpen() const noexcept
{
    return HasCardWindow();
}

void CMediaCardWindowManager::StartAnimation(
    AnimationKind kind,
    const CRect& base_rect)
{
    StopAnimation();
    m_animation_kind = kind;
    m_animation_base_rect = base_rect;
    m_animation_started_milliseconds = GetTickCount64();
    m_popup_vertical_direction_y = kind == AnimationKind::Opening
        ? media::kMediaCardOpeningVerticalDirectionY
        : media::kMediaCardClosingVerticalDirectionY;
    m_animation_start_alpha = kind == AnimationKind::Closing ? m_current_alpha : 255;
    m_state = kind == AnimationKind::Opening ? WindowState::Opening : WindowState::Closing;

    ApplyAnimationFrame(0.0);
    s_animating_manager = this;
    m_animation_timer = SetTimer(
        nullptr,
        0,
        kAnimationFrameIntervalMilliseconds,
        &CMediaCardWindowManager::AnimationTimerProc);
    if (m_animation_timer == 0)
    {
        s_animating_manager = nullptr;
        ApplyAnimationFrame(1.0);
        if (kind == AnimationKind::Opening)
        {
            m_animation_kind = AnimationKind::None;
            m_opening_animation_completed = true;
            TryCompleteOpening();
        }
        else
        {
            DestroyCardWindow();
        }
    }
}

void CMediaCardWindowManager::AdvanceAnimation()
{
    if (m_animation_kind == AnimationKind::None || !HasCardWindow())
    {
        DestroyCardWindow();
        return;
    }

    const UINT duration = m_animation_kind == AnimationKind::Opening
        ? kOpeningAnimationDurationMilliseconds
        : kClosingAnimationDurationMilliseconds;
    const double progress = static_cast<double>(GetTickCount64() - m_animation_started_milliseconds)
        / static_cast<double>(duration);
    ApplyAnimationFrame(progress);
    if (progress < 1.0)
    {
        return;
    }

    if (m_animation_kind == AnimationKind::Opening)
    {
        StopAnimation();
        m_opening_animation_completed = true;
        TryCompleteOpening();
        return;
    }
    DestroyCardWindow();
}

void CMediaCardWindowManager::StopAnimation()
{
    if (m_animation_timer != 0)
    {
        KillTimer(nullptr, m_animation_timer);
        m_animation_timer = 0;
    }
    if (s_animating_manager == this)
    {
        s_animating_manager = nullptr;
    }
    m_animation_kind = AnimationKind::None;
}

void CMediaCardWindowManager::TryCompleteOpening()
{
    if (m_state != WindowState::Opening || !HasCardWindow())
    {
        return;
    }

    const bool activation_verified = media::IsMediaCardActivationVerified(
        m_card->HasBeenActivated(),
        GetForegroundWindow() == m_card->GetSafeHwnd());
    if (media::ShouldCompleteMediaCardOpening(
            m_opening_animation_completed,
            activation_verified))
    {
        m_state = WindowState::Open;
    }
}

void CMediaCardWindowManager::DestroyCardWindow()
{
    StopAnimation();
    m_close_in_progress = true;
    if (m_card && m_card->GetSafeHwnd())
    {
        m_card->DestroyWindow();
    }
    if (m_card_marked_visible)
    {
        m_card_marked_visible = false;
        g_plugin.SetMediaCardVisible(false);
    }
    m_close_in_progress = false;
    m_activation_compensation_attempted = false;
    m_opening_animation_completed = false;
    m_state = WindowState::Closed;
    m_current_alpha = 255;
    m_animation_start_alpha = 255;
    m_popup_vertical_direction_y = 0;
}

void CMediaCardWindowManager::ApplyAnimationFrame(double progress)
{
    if (!HasCardWindow() || m_animation_kind == AnimationKind::None)
    {
        return;
    }

    const media::MediaCardAnimationPhase phase =
        m_animation_kind == AnimationKind::Opening
        ? media::MediaCardAnimationPhase::Opening
        : media::MediaCardAnimationPhase::Closing;
    media::MediaCardAnimationFrame frame = media::CalculateMediaCardAnimationFrame(
        phase,
        progress,
        g_plugin.DPI(8),
        g_plugin.DPI(5));
    if (m_animation_kind == AnimationKind::Closing)
    {
        frame.alpha = static_cast<BYTE>(
            (static_cast<unsigned int>(frame.alpha) * m_animation_start_alpha + 127U) / 255U);
    }

    CRect rect(m_animation_base_rect);
    rect.OffsetRect(0, frame.offset_y * m_popup_vertical_direction_y);
    m_card->ApplyAnimationFrame(frame.alpha, rect);
    m_current_alpha = frame.alpha;
}

bool CMediaCardWindowManager::HasCardWindow() const noexcept
{
    return m_card && m_card->GetSafeHwnd() != nullptr;
}

void CALLBACK CMediaCardWindowManager::OpenTimerProc(HWND, UINT, UINT_PTR timer_id, DWORD)
{
    CMediaCardWindowManager* manager = s_scheduled_manager;
    if (manager == nullptr || manager->m_open_timer != timer_id)
    {
        KillTimer(nullptr, timer_id);
        return;
    }

    const HWND anchor = manager->m_pending_anchor;
    const CPoint point = manager->m_pending_client_point;
    manager->CancelScheduledOpen();
    manager->Open(anchor, point.x, point.y);
}

void CALLBACK CMediaCardWindowManager::AnimationTimerProc(HWND, UINT, UINT_PTR timer_id, DWORD)
{
    CMediaCardWindowManager* manager = s_animating_manager;
    if (manager == nullptr || manager->m_animation_timer != timer_id)
    {
        KillTimer(nullptr, timer_id);
        return;
    }
    manager->AdvanceAnimation();
}
