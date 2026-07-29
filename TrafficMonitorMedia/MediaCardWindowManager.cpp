#include "pch.h"
#include "MediaCardWindowManager.h"

#include "MediaCardDismissOverlayWnd.h"
#include "MediaCardInteractionState.h"
#include "MediaCardWnd.h"
#include "TrafficMonitorMedia.h"

#include <algorithm>

CMediaCardWindowManager* CMediaCardWindowManager::s_scheduled_manager = nullptr;

CMediaCardWindowManager::CMediaCardWindowManager()
    : m_overlay(std::make_unique<CMediaCardDismissOverlayWnd>())
    , m_card(std::make_unique<CMediaCardWnd>(this))
{
}

CMediaCardWindowManager::~CMediaCardWindowManager()
{
    Close();
}

void CMediaCardWindowManager::ScheduleOpen(HWND anchor_window, int client_x, int client_y)
{
    CancelScheduledOpen();
    if (!media::ShouldScheduleMediaCardOpen(
            GetTickCount64(),
            m_open_suppression_deadline_milliseconds))
    {
        return;
    }

    m_pending_anchor = anchor_window;
    m_pending_client_point = CPoint(client_x, client_y);
    s_scheduled_manager = this;
    m_open_timer = SetTimer(nullptr, 0, GetDoubleClickTime(), &CMediaCardWindowManager::OpenTimerProc);
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

    Close();

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
        m_overlay->DestroyWindow();
        return;
    }

    g_plugin.SetMediaCardVisible(true);
    m_card_marked_visible = true;
    m_card->RefreshSnapshot();
    m_card->ShowWindow(SW_SHOW);
    m_card->BringWindowToTop();
    m_card->SetForegroundWindow();
}

void CMediaCardWindowManager::Close()
{
    CancelScheduledOpen();
    if (m_close_in_progress)
    {
        return;
    }

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
}

void CMediaCardWindowManager::OnCardDestroyed()
{
    if (media::MediaCardLifecycleState{
            m_close_in_progress,
            m_card_marked_visible,
        }.ShouldHandleUnexpectedCardDestroy())
    {
        Close();
    }
}

bool CMediaCardWindowManager::IsOpen() const noexcept
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
