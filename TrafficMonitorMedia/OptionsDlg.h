#pragma once
#include "MediaSettings.h"
#include "OptionsLayout.h"

#include <vector>

class COptionsDlg : public CDialog
{
    DECLARE_DYNAMIC(COptionsDlg)

public:
    explicit COptionsDlg(CWnd* pParent = nullptr);
    ~COptionsDlg() override;

    media::SettingData m_data;

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_OPTIONS_DIALOG };
#endif

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG* message) override;
    void OnOK() override;

    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* min_max_info);
    afx_msg void OnVScroll(UINT scroll_code, UINT position, CScrollBar* scroll_bar);
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    struct ControlLayout
    {
        int control_id{};
        CRect initial_rect;
        media::HorizontalAnchor horizontal_anchor{ media::HorizontalAnchor::Left };
        bool fixed_to_bottom{};
    };

    static void SelectAction(CComboBox& combo, media::MediaControlAction action);
    static media::MediaControlAction ReadAction(
        const CComboBox& combo,
        media::MediaControlAction fallback);
    static void SelectWheelAction(CComboBox& combo, media::WheelAction action);
    static media::WheelAction ReadWheelAction(
        const CComboBox& combo,
        media::WheelAction fallback);
    void FillActionCombo(CComboBox& combo);
    void FillWheelActionCombo(CComboBox& combo);

    [[nodiscard]] int DPI(int value) const;
    void CaptureInitialLayout();
    void ApplyLayout();
    void MoveControl(int control_id, const CRect& rect, BOOL repaint = TRUE);
    void MoveScrollableControl(int control_id, const CRect& rect, BOOL repaint = TRUE);
    void SetScrollOffset(int offset);
    void UpdateScrollInfo(int content_height, int viewport_height);

    BOOL m_show_progress{ TRUE };
    BOOL m_show_status_icon{ TRUE };
    BOOL m_show_artist_on_second_line{ TRUE };
    BOOL m_show_cover_background{ FALSE };
    BOOL m_smooth_cover_scaling{ TRUE };
    int m_min_title_width{ media::kDefaultMinTitleWidth };
    int m_max_title_width{ media::kDefaultMaxTitleWidth };
    int m_system_volume_step_percent{ media::kDefaultSystemVolumeStepPercent };
    CSpinButtonCtrl m_min_title_width_spin;
    CSpinButtonCtrl m_max_title_width_spin;
    CSpinButtonCtrl m_system_volume_step_spin;
    CComboBox m_icon_left_click_combo;
    CComboBox m_icon_left_double_click_combo;
    CComboBox m_title_left_click_combo;
    CComboBox m_title_left_double_click_combo;
    CComboBox m_right_click_combo;
    CComboBox m_wheel_action_combo;
    std::vector<ControlLayout> m_control_layouts;
    bool m_layout_initialized{};
    bool m_layout_updating{};
    bool m_scrollbar_visible{};
    CSize m_initial_client_size;
    CSize m_min_window_size;
    int m_content_height{};
    int m_button_content_gap{};
    int m_scroll_offset{};
    int m_max_scroll_offset{};
    CRect m_content_viewport;
};
