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

    void ScheduleOpen(
        HWND anchor_window,
        int client_x,
        int client_y,
        unsigned int confirmation_delay_milliseconds);
    void CancelScheduledOpen();
    void SuppressScheduledOpenAfterDoubleClick();
    void Open(HWND anchor_window, int client_x, int client_y);
    void Close();
    void ForceClose();
    void OnCardDestroyed();
    [[nodiscard]] bool IsOpen() const noexcept;

private:
    enum class WindowState
    {
        Closed,
        Opening,
        Open,
        Closing,
    };

    enum class AnimationKind
    {
        None,
        Opening,
        Closing,
    };

    static constexpr UINT kAnimationFrameIntervalMilliseconds = 16;
    static constexpr UINT kOpeningAnimationDurationMilliseconds = 150;
    static constexpr UINT kClosingAnimationDurationMilliseconds = 110;

    static void CALLBACK OpenTimerProc(HWND window, UINT message, UINT_PTR timer_id, DWORD time);
    static void CALLBACK AnimationTimerProc(HWND window, UINT message, UINT_PTR timer_id, DWORD time);
    void OpenAtScreenPoint(CPoint anchor_point);
    void StartAnimation(AnimationKind kind, const CRect& base_rect, int direction_y);
    void AdvanceAnimation();
    void StopAnimation();
    void DestroyCardAndOverlay();
    void ApplyAnimationFrame(double progress);
    [[nodiscard]] bool HasCardWindow() const noexcept;

    std::unique_ptr<CMediaCardDismissOverlayWnd> m_overlay;
    std::unique_ptr<CMediaCardWnd> m_card;
    HWND m_pending_anchor{};
    CPoint m_pending_client_point{};
    UINT_PTR m_open_timer{};
    std::uint64_t m_open_suppression_deadline_milliseconds{};
    WindowState m_state{ WindowState::Closed };
    AnimationKind m_animation_kind{ AnimationKind::None };
    UINT_PTR m_animation_timer{};
    std::uint64_t m_animation_started_milliseconds{};
    CRect m_animation_base_rect{};
    int m_popup_vertical_direction_y{};
    BYTE m_current_alpha{ 255 };
    BYTE m_animation_start_alpha{ 255 };
    bool m_close_in_progress{};
    bool m_card_marked_visible{};

    static CMediaCardWindowManager* s_scheduled_manager;
    static CMediaCardWindowManager* s_animating_manager;
};
