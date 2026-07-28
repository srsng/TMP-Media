#include "pch.h"
#include "TrafficMonitorMedia.h"
#include "OptionsDlg.h"

#include <array>

IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent)
    : CDialog(IDD_OPTIONS_DIALOG, pParent)
{
}

COptionsDlg::~COptionsDlg() = default;

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_SHOW_PROGRESS_CHECK, m_show_progress);
    DDX_Check(pDX, IDC_SHOW_STATUS_ICON_CHECK, m_show_status_icon);
    DDX_Check(pDX, IDC_SHOW_ARTIST_SECOND_LINE_CHECK, m_show_artist_on_second_line);
    DDX_Check(pDX, IDC_SHOW_COVER_BACKGROUND_CHECK, m_show_cover_background);
    DDX_Check(pDX, IDC_SMOOTH_COVER_SCALING_CHECK, m_smooth_cover_scaling);
    DDX_Text(pDX, IDC_MAX_TITLE_WIDTH_EDIT, m_max_title_width);
    DDV_MinMaxInt(pDX, m_max_title_width, media::kMinimumTitleWidth, media::kMaximumTitleWidth);
    DDX_Text(pDX, IDC_SYSTEM_VOLUME_STEP_EDIT, m_system_volume_step_percent);
    DDV_MinMaxInt(
        pDX,
        m_system_volume_step_percent,
        media::kMinimumSystemVolumeStepPercent,
        media::kMaximumSystemVolumeStepPercent);
    DDX_Control(pDX, IDC_MAX_TITLE_WIDTH_SPIN, m_max_title_width_spin);
    DDX_Control(pDX, IDC_SYSTEM_VOLUME_STEP_SPIN, m_system_volume_step_spin);
    DDX_Control(pDX, IDC_LEFT_CLICK_ACTION_COMBO, m_left_click_combo);
    DDX_Control(pDX, IDC_LEFT_DOUBLE_CLICK_ACTION_COMBO, m_left_double_click_combo);
    DDX_Control(pDX, IDC_RIGHT_CLICK_ACTION_COMBO, m_right_click_combo);
    DDX_Control(pDX, IDC_WHEEL_ACTION_COMBO, m_wheel_action_combo);
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
END_MESSAGE_MAP()

BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_data = media::NormalizeSettings(m_data);
    m_show_progress = m_data.show_progress ? TRUE : FALSE;
    m_show_status_icon = m_data.show_status_icon ? TRUE : FALSE;
    m_show_artist_on_second_line = m_data.show_artist_on_second_line ? TRUE : FALSE;
    m_show_cover_background = m_data.show_cover_background ? TRUE : FALSE;
    m_smooth_cover_scaling = m_data.smooth_cover_scaling ? TRUE : FALSE;
    m_max_title_width = m_data.max_title_width;
    m_system_volume_step_percent = m_data.system_volume_step_percent;
    UpdateData(FALSE);

    m_max_title_width_spin.SetRange32(media::kMinimumTitleWidth, media::kMaximumTitleWidth);
    m_max_title_width_spin.SetBuddy(GetDlgItem(IDC_MAX_TITLE_WIDTH_EDIT));
    m_system_volume_step_spin.SetRange32(
        media::kMinimumSystemVolumeStepPercent,
        media::kMaximumSystemVolumeStepPercent);
    m_system_volume_step_spin.SetBuddy(GetDlgItem(IDC_SYSTEM_VOLUME_STEP_EDIT));

    FillActionCombo(m_left_click_combo);
    FillActionCombo(m_left_double_click_combo);
    FillActionCombo(m_right_click_combo);
    FillWheelActionCombo(m_wheel_action_combo);

    SelectAction(m_left_click_combo, m_data.input.left_click);
    SelectAction(m_left_double_click_combo, m_data.input.left_double_click);
    SelectAction(m_right_click_combo, m_data.input.right_click);
    SelectWheelAction(m_wheel_action_combo, m_data.input.wheel);

    return TRUE;
}

void COptionsDlg::OnOK()
{
    if (!UpdateData(TRUE))
    {
        return;
    }

    media::SettingData updated = m_data;
    updated.show_progress = m_show_progress != FALSE;
    updated.show_status_icon = m_show_status_icon != FALSE;
    updated.show_artist_on_second_line = m_show_artist_on_second_line != FALSE;
    updated.show_cover_background = m_show_cover_background != FALSE;
    updated.smooth_cover_scaling = m_smooth_cover_scaling != FALSE;
    updated.max_title_width = m_max_title_width;
    updated.system_volume_step_percent = m_system_volume_step_percent;
    updated.input.left_click = ReadAction(m_left_click_combo, updated.input.left_click);
    updated.input.left_double_click = ReadAction(
        m_left_double_click_combo,
        updated.input.left_double_click);
    updated.input.right_click = ReadAction(m_right_click_combo, updated.input.right_click);
    updated.input.wheel = ReadWheelAction(m_wheel_action_combo, updated.input.wheel);
    m_data = media::NormalizeSettings(updated);

    CDialog::OnOK();
}

void COptionsDlg::FillActionCombo(CComboBox& combo)
{
    const std::array actions{
        std::pair{ IDS_ACTION_NONE, media::MediaControlAction::None },
        std::pair{ IDS_ACTION_TOGGLE_PLAY_PAUSE, media::MediaControlAction::TogglePlayPause },
        std::pair{ IDS_ACTION_SKIP_PREVIOUS, media::MediaControlAction::SkipPrevious },
        std::pair{ IDS_ACTION_SKIP_NEXT, media::MediaControlAction::SkipNext },
    };

    combo.ResetContent();
    for (const auto& [string_id, action] : actions)
    {
        const int index = combo.AddString(g_plugin.StringRes(string_id));
        if (index != CB_ERR && index != CB_ERRSPACE)
        {
            combo.SetItemData(index, static_cast<DWORD_PTR>(action));
        }
    }
}

void COptionsDlg::FillWheelActionCombo(CComboBox& combo)
{
    const std::array actions{
        std::pair{ IDS_WHEEL_ACTION_NONE, media::WheelAction::None },
        std::pair{ IDS_WHEEL_ACTION_SWITCH_TRACK, media::WheelAction::SwitchTrack },
        std::pair{ IDS_WHEEL_ACTION_SWITCH_SESSION, media::WheelAction::SwitchMediaSession },
        std::pair{ IDS_WHEEL_ACTION_ADJUST_VOLUME, media::WheelAction::AdjustSystemVolume },
    };

    combo.ResetContent();
    for (const auto& [string_id, action] : actions)
    {
        const int index = combo.AddString(g_plugin.StringRes(string_id));
        if (index != CB_ERR && index != CB_ERRSPACE)
        {
            combo.SetItemData(index, static_cast<DWORD_PTR>(action));
        }
    }
}

void COptionsDlg::SelectAction(CComboBox& combo, media::MediaControlAction action)
{
    for (int index = 0; index < combo.GetCount(); ++index)
    {
        if (combo.GetItemData(index) == static_cast<DWORD_PTR>(action))
        {
            combo.SetCurSel(index);
            return;
        }
    }
    combo.SetCurSel(0);
}

void COptionsDlg::SelectWheelAction(CComboBox& combo, media::WheelAction action)
{
    for (int index = 0; index < combo.GetCount(); ++index)
    {
        if (combo.GetItemData(index) == static_cast<DWORD_PTR>(action))
        {
            combo.SetCurSel(index);
            return;
        }
    }
    combo.SetCurSel(0);
}

media::MediaControlAction COptionsDlg::ReadAction(
    const CComboBox& combo,
    media::MediaControlAction fallback)
{
    const int index = combo.GetCurSel();
    if (index == CB_ERR)
    {
        return fallback;
    }

    const DWORD_PTR item_data = combo.GetItemData(index);
    if (item_data == static_cast<DWORD_PTR>(CB_ERR))
    {
        return fallback;
    }

    return media::NormalizeAction(
        static_cast<media::MediaControlAction>(item_data),
        fallback);
}

media::WheelAction COptionsDlg::ReadWheelAction(
    const CComboBox& combo,
    media::WheelAction fallback)
{
    const int index = combo.GetCurSel();
    if (index == CB_ERR)
    {
        return fallback;
    }

    const DWORD_PTR item_data = combo.GetItemData(index);
    if (item_data == static_cast<DWORD_PTR>(CB_ERR))
    {
        return fallback;
    }

    return media::NormalizeWheelAction(
        static_cast<media::WheelAction>(item_data),
        fallback);
}
