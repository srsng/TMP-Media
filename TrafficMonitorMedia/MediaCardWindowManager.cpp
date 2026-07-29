#include "pch.h"
#include "MediaCardWindowManager.h"

#include "MediaCardDismissOverlayWnd.h"
#include "MediaCardInteractionState.h"
#include "MediaCardWnd.h"
#include "TrafficMonitorMedia.h"

CMediaCardWindowManager* CMediaCardWindowManager::s_scheduled_manager = nullptr;
CMediaCardWindowManager* CMediaCardWindowManager::s_animating_manager = nullptr;

CMediaCardWindowManager::CMediaCardWindowManager()
    : m_overlay(std::make_unique<CMediaCardDismissOverlayWnd>())
    , m_card(std::make_unique<CMediaCardWnd>(this))
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
    OpenAtScreenPoint(anchor_point);
}

void CMediaCardWindowManager::OpenAtScreenPoint(CPoint anchor_point)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    DestroyCardAndOverlay();

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

    if (!m_overlay->Create(this))
    {
        return;
    }

    const CRect card_rect(placement.x, placement.y, placement.x + width, placement.y + height);
    if (!m_card->Create(m_overlay.get(), card_rect))
    {
        DestroyCardAndOverlay();
        return;
    }

    m_popup_vertical_direction_y = card_rect.top >= anchor_point.y ? 1 : -1;
    g_plugin.SetMediaCardVisible(true);
    m_card_marked_visible = true;
    m_card->RefreshSnapshot();
    m_card->SetInteractionEnabled(true);
    StartAnimation(AnimationKind::Opening, card_rect, m_popup_vertical_direction_y);
    m_card->ShowWindow(SW_SHOW);
    m_card->BringWindowToTop();
    m_card->SetForegroundWindow();
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
        DestroyCardAndOverlay();
        return;
    }

    m_card->CancelSeekPreview();
    m_card->SetInteractionEnabled(false);
    CRect current_rect;
    m_card->GetWindowRect(&current_rect);
    StartAnimation(AnimationKind::Closing, current_rect, m_popup_vertical_direction_y);
}

void CMediaCardWindowManager::ForceClose()
{
    CancelScheduledOpen();
    DestroyCardAndOverlay();
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

bool CMediaCardWindowManager::IsOpen() const noexcept
{
    return HasCardWindow();
}

void CMediaCardWindowManager::StartAnimation(
    AnimationKind kind,
    const CRect& base_rect,
    int direction_y)
{
    StopAnimation();
    m_animation_kind = kind;
    m_animation_base_rect = base_rect;
    m_animation_started_milliseconds = GetTickCount64();
    m_popup_vertical_direction_y = direction_y == 0 ? 1 : direction_y;
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
            m_state = WindowState::Open;
        }
        else
        {
            DestroyCardAndOverlay();
        }
    }
}

void CMediaCardWindowManager::AdvanceAnimation()
{
    if (m_animation_kind == AnimationKind::None || !HasCardWindow())
    {
        DestroyCardAndOverlay();
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
        m_state = WindowState::Open;
        return;
    }
    DestroyCardAndOverlay();
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

void CMediaCardWindowManager::DestroyCardAndOverlay()
{
    StopAnimation();
    m_close_in_progress = true;
    if (m_card && m_card->GetSafeHwnd())
    {
        m_card->DestroyWindow();
    }
    if (m_overlay && m_overlay->GetSafeHwnd())
    {
        m_overlay->DestroyWindow();
    }
    if (m_card_marked_visible)
    {
        m_card_marked_visible = false;
        g_plugin.SetMediaCardVisible(false);
    }
    m_close_in_progress = false;
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
