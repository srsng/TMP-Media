#pragma once
#include "MediaSettings.h"

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
    void OnOK() override;

    DECLARE_MESSAGE_MAP()

private:
    static void SelectAction(CComboBox& combo, media::MediaControlAction action);
    static media::MediaControlAction ReadAction(
        const CComboBox& combo,
        media::MediaControlAction fallback);
    void FillActionCombo(CComboBox& combo);

    BOOL m_show_progress{ TRUE };
    BOOL m_show_status_icon{ TRUE };
    BOOL m_show_artist_on_second_line{ TRUE };
    BOOL m_show_cover_background{ FALSE };
    BOOL m_smooth_cover_scaling{ TRUE };
    int m_max_title_width{ media::kDefaultMaxTitleWidth };
    CSpinButtonCtrl m_max_title_width_spin;
    CComboBox m_left_click_combo;
    CComboBox m_left_double_click_combo;
    CComboBox m_right_click_combo;
};
