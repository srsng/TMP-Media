#pragma once

#include "MediaCardVisuals.h"
#include "MediaSessionService.h"

#include <cstdint>

class CMediaCardWindowManager;

class CMediaCardWnd : public CWnd
{
public:
    explicit CMediaCardWnd(CMediaCardWindowManager* manager);

    BOOL Create(HWND owner_window, const CRect& rect, std::uintptr_t message_generation);
    void RefreshSnapshot();
    void ApplyAnimationFrame(BYTE alpha, const CRect& rect);
    void SetInteractionEnabled(bool enabled) noexcept;
    void CancelSeekPreview();
    [[nodiscard]] bool HasBeenActivated() const noexcept;
    [[nodiscard]] bool BeginActivationVerification() noexcept;

protected:
    afx_msg int OnCreate(LPCREATESTRUCT create_struct);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnNcDestroy();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnKeyDown(UINT character, UINT repeat_count, UINT flags);
    afx_msg void OnActivate(UINT state, CWnd* other_window, BOOL minimized);
    afx_msg LRESULT OnDeferredDeactivateDismiss(WPARAM, LPARAM);
    afx_msg LRESULT OnVerifyActivation(WPARAM, LPARAM);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* window);
    DECLARE_MESSAGE_MAP()

private:
    struct Layout
    {
        CRect cover;
        CRect title;
        CRect artist;
        CRect switch_previous;
        CRect switch_next;
        CRect previous;
        CRect play_pause;
        CRect next;
        CRect progress;
        CRect elapsed;
        CRect duration;
    };

    [[nodiscard]] Layout CalculateLayout(const CRect& client) const;
    [[nodiscard]] double ProgressFractionAt(CPoint point) const;
    void DrawCard(CDC& dc, const CRect& client);
    void DrawCover(CDC& dc, const CRect& rect) const;
    void DrawButton(
        CDC& dc,
        const CRect& rect,
        media::MediaCardButtonIcon icon,
        bool enabled,
        bool primary) const;
    void SubmitSeek(double fraction);

    CMediaCardWindowManager* m_manager{};
    MediaTitleSnapshot m_snapshot;
    Layout m_layout;
    bool m_dragging_progress{};
    double m_preview_fraction{};
    bool m_interaction_enabled{ true };
    bool m_has_been_activated{};
    bool m_deactivate_dismiss_posted{};
    bool m_deactivate_dismiss_fallback_pending{};
    bool m_activation_verification_posted{};
    std::uintptr_t m_message_generation{};
};
