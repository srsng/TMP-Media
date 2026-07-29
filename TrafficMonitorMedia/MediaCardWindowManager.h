#pragma once

#include <cstdint>
#include <memory>

class CMediaCardDismissOverlayWnd;
class CMediaCardWnd;

class CMediaCardWindowManager
{
public:
    CMediaCardWindowManager();
    ~CMediaCardWindowManager();

    CMediaCardWindowManager(const CMediaCardWindowManager&) = delete;
    CMediaCardWindowManager& operator=(const CMediaCardWindowManager&) = delete;

    void ScheduleOpen(HWND anchor_window, int client_x, int client_y);
    void CancelScheduledOpen();
    void SuppressScheduledOpenAfterDoubleClick();
    void Open(HWND anchor_window, int client_x, int client_y);
    void Close();
    void OnCardDestroyed();
    [[nodiscard]] bool IsOpen() const noexcept;

private:
    static void CALLBACK OpenTimerProc(HWND window, UINT message, UINT_PTR timer_id, DWORD time);
    void OpenAtScreenPoint(CPoint anchor_point);

    std::unique_ptr<CMediaCardDismissOverlayWnd> m_overlay;
    std::unique_ptr<CMediaCardWnd> m_card;
    HWND m_pending_anchor{};
    CPoint m_pending_client_point{};
    UINT_PTR m_open_timer{};
    std::uint64_t m_open_suppression_deadline_milliseconds{};
    bool m_close_in_progress{};
    bool m_card_marked_visible{};

    static CMediaCardWindowManager* s_scheduled_manager;
};
