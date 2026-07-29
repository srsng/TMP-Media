#include "pch.h"
#include "TrafficMonitorMedia.h"
#include "OptionsDlg.h"

#include <algorithm>
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
    DDX_Text(pDX, IDC_MIN_TITLE_WIDTH_EDIT, m_min_title_width);
    DDV_MinMaxInt(pDX, m_min_title_width, media::kMinimumTitleWidth, media::kMaximumTitleWidth);
    DDX_Text(pDX, IDC_MAX_TITLE_WIDTH_EDIT, m_max_title_width);
    DDV_MinMaxInt(pDX, m_max_title_width, media::kMinimumTitleWidth, media::kMaximumTitleWidth);
    DDX_Text(pDX, IDC_SYSTEM_VOLUME_STEP_EDIT, m_system_volume_step_percent);
    DDV_MinMaxInt(
        pDX,
        m_system_volume_step_percent,
        media::kMinimumSystemVolumeStepPercent,
        media::kMaximumSystemVolumeStepPercent);
    DDX_Control(pDX, IDC_MIN_TITLE_WIDTH_SPIN, m_min_title_width_spin);
    DDX_Control(pDX, IDC_MAX_TITLE_WIDTH_SPIN, m_max_title_width_spin);
    DDX_Control(pDX, IDC_SYSTEM_VOLUME_STEP_SPIN, m_system_volume_step_spin);
    DDX_Control(pDX, IDC_LEFT_CLICK_ACTION_COMBO, m_left_click_combo);
    DDX_Control(pDX, IDC_LEFT_DOUBLE_CLICK_ACTION_COMBO, m_left_double_click_combo);
    DDX_Control(pDX, IDC_RIGHT_CLICK_ACTION_COMBO, m_right_click_combo);
    DDX_Control(pDX, IDC_WHEEL_ACTION_COMBO, m_wheel_action_combo);
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
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
    m_min_title_width = m_data.min_title_width;
    m_max_title_width = m_data.max_title_width;
    m_system_volume_step_percent = m_data.system_volume_step_percent;
    UpdateData(FALSE);

    m_min_title_width_spin.SetRange32(media::kMinimumTitleWidth, media::kMaximumTitleWidth);
    m_min_title_width_spin.SetBuddy(GetDlgItem(IDC_MIN_TITLE_WIDTH_EDIT));
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

    CaptureInitialLayout();
    m_layout_initialized = true;
    ApplyLayout();

    return TRUE;
}

BOOL COptionsDlg::PreTranslateMessage(MSG* message)
{
    if (message != nullptr
        && message->message == WM_MOUSEWHEEL
        && m_max_scroll_offset > 0)
    {
        const short delta = GET_WHEEL_DELTA_WPARAM(message->wParam);
        SetScrollOffset(m_scroll_offset - (delta > 0 ? DPI(40) : -DPI(40)));
        return TRUE;
    }
    return CDialog::PreTranslateMessage(message);
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
    updated.min_title_width = m_min_title_width;
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
        std::pair{ IDS_ACTION_OPEN_MEDIA_CARD, media::MediaControlAction::OpenMediaCard },
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

int COptionsDlg::DPI(int value) const
{
    UINT dpi = 96;
    if (GetSafeHwnd() != nullptr)
    {
        dpi = ::GetDpiForWindow(GetSafeHwnd());
    }
    return MulDiv(value, static_cast<int>(dpi), 96);
}

void COptionsDlg::CaptureInitialLayout()
{
    CRect client_rect;
    CRect window_rect;
    GetClientRect(&client_rect);
    GetWindowRect(&window_rect);
    m_initial_client_size = client_rect.Size();

    const int non_client_height = window_rect.Height() - client_rect.Height();
    m_min_window_size.cx = window_rect.Width();
    m_min_window_size.cy = non_client_height + (std::min)(client_rect.Height(), DPI(260));

    const auto capture = [this](
        int control_id,
        media::HorizontalAnchor horizontal_anchor,
        bool fixed_to_bottom = false)
    {
        CWnd* control = GetDlgItem(control_id);
        if (control == nullptr)
        {
            return;
        }

        CRect rect;
        control->GetWindowRect(&rect);
        ScreenToClient(&rect);
        m_control_layouts.push_back({ control_id, rect, horizontal_anchor, fixed_to_bottom });
    };

    capture(IDC_DISPLAY_GROUP, media::HorizontalAnchor::Stretch);
    capture(IDC_SHOW_PROGRESS_CHECK, media::HorizontalAnchor::Left);
    capture(IDC_SHOW_STATUS_ICON_CHECK, media::HorizontalAnchor::Left);
    capture(IDC_SHOW_ARTIST_SECOND_LINE_CHECK, media::HorizontalAnchor::Left);
    capture(IDC_SHOW_COVER_BACKGROUND_CHECK, media::HorizontalAnchor::Right);
    capture(IDC_SMOOTH_COVER_SCALING_CHECK, media::HorizontalAnchor::Right);
    capture(IDC_MIN_TITLE_WIDTH_LABEL_STATIC, media::HorizontalAnchor::Right);
    capture(IDC_MIN_TITLE_WIDTH_EDIT, media::HorizontalAnchor::Right);
    capture(IDC_MIN_TITLE_WIDTH_SPIN, media::HorizontalAnchor::Right);
    capture(IDC_MIN_TITLE_WIDTH_UNIT_STATIC, media::HorizontalAnchor::Right);
    capture(IDC_TITLE_WIDTH_LABEL_STATIC, media::HorizontalAnchor::Right);
    capture(IDC_MAX_TITLE_WIDTH_EDIT, media::HorizontalAnchor::Right);
    capture(IDC_MAX_TITLE_WIDTH_SPIN, media::HorizontalAnchor::Right);
    capture(IDC_TITLE_WIDTH_UNIT_STATIC, media::HorizontalAnchor::Right);

    capture(IDC_MOUSE_ACTIONS_GROUP, media::HorizontalAnchor::Stretch);
    capture(IDC_LEFT_CLICK_LABEL_STATIC, media::HorizontalAnchor::Left);
    capture(IDC_LEFT_CLICK_ACTION_COMBO, media::HorizontalAnchor::Left);
    capture(IDC_LEFT_DOUBLE_CLICK_LABEL_STATIC, media::HorizontalAnchor::Left);
    capture(IDC_LEFT_DOUBLE_CLICK_ACTION_COMBO, media::HorizontalAnchor::Left);
    capture(IDC_RIGHT_CLICK_LABEL_STATIC, media::HorizontalAnchor::Right);
    capture(IDC_RIGHT_CLICK_ACTION_COMBO, media::HorizontalAnchor::Right);
    capture(IDC_WHEEL_LABEL_STATIC, media::HorizontalAnchor::Right);
    capture(IDC_WHEEL_ACTION_COMBO, media::HorizontalAnchor::Right);
    capture(IDC_SYSTEM_VOLUME_LABEL_STATIC, media::HorizontalAnchor::Right);
    capture(IDC_SYSTEM_VOLUME_STEP_EDIT, media::HorizontalAnchor::Right);
    capture(IDC_SYSTEM_VOLUME_STEP_SPIN, media::HorizontalAnchor::Right);
    capture(IDC_SYSTEM_VOLUME_UNIT_STATIC, media::HorizontalAnchor::Right);
    capture(IDC_RIGHT_CLICK_HINT_STATIC, media::HorizontalAnchor::Stretch);

    capture(IDOK, media::HorizontalAnchor::Right, true);
    capture(IDCANCEL, media::HorizontalAnchor::Right, true);

    const auto mouse_group = std::find_if(
        m_control_layouts.begin(),
        m_control_layouts.end(),
        [](const ControlLayout& layout) { return layout.control_id == IDC_MOUSE_ACTIONS_GROUP; });
    const auto ok_button = std::find_if(
        m_control_layouts.begin(),
        m_control_layouts.end(),
        [](const ControlLayout& layout) { return layout.control_id == IDOK; });
    if (mouse_group != m_control_layouts.end())
    {
        m_content_height = mouse_group->initial_rect.bottom;
    }
    if (mouse_group != m_control_layouts.end() && ok_button != m_control_layouts.end())
    {
        m_button_content_gap = ok_button->initial_rect.top - mouse_group->initial_rect.bottom;
    }
    m_button_content_gap = (std::max)(m_button_content_gap, DPI(6));
}

void COptionsDlg::MoveControl(int control_id, const CRect& rect, BOOL repaint)
{
    if (CWnd* control = GetDlgItem(control_id); control != nullptr)
    {
        control->MoveWindow(rect, repaint);
    }
}

void COptionsDlg::MoveScrollableControl(int control_id, const CRect& rect, BOOL repaint)
{
    CWnd* control = GetDlgItem(control_id);
    if (control == nullptr)
    {
        return;
    }

    control->MoveWindow(rect, FALSE);
    const media::LayoutRect clipped = media::IntersectLayoutRect(
        { rect.left, rect.top, rect.right, rect.bottom },
        {
            m_content_viewport.left,
            m_content_viewport.top,
            m_content_viewport.right,
            m_content_viewport.bottom,
        });
    if (clipped.Width() == 0 || clipped.Height() == 0)
    {
        control->ShowWindow(SW_HIDE);
        return;
    }

    const bool fully_visible = clipped.left == rect.left
        && clipped.top == rect.top
        && clipped.right == rect.right
        && clipped.bottom == rect.bottom;
    if (fully_visible)
    {
        ::SetWindowRgn(control->GetSafeHwnd(), nullptr, FALSE);
    }
    else
    {
        HRGN region = ::CreateRectRgn(
            clipped.left - rect.left,
            clipped.top - rect.top,
            clipped.right - rect.left,
            clipped.bottom - rect.top);
        if (region != nullptr && ::SetWindowRgn(control->GetSafeHwnd(), region, FALSE) == 0)
        {
            ::DeleteObject(region);
        }
    }
    control->ShowWindow(SW_SHOWNA);
    if (repaint)
    {
        control->Invalidate();
    }
}

void COptionsDlg::ApplyLayout()
{
    if (!m_layout_initialized || m_layout_updating || GetSafeHwnd() == nullptr)
    {
        return;
    }

    m_layout_updating = true;

    CRect client_rect;
    GetClientRect(&client_rect);
    int client_width = client_rect.Width();
    int client_height = client_rect.Height();
    int height_delta = client_height - m_initial_client_size.cy;

    const auto find_control_layout = [this](int control_id) -> const ControlLayout*
    {
        const auto found = std::find_if(
            m_control_layouts.begin(),
            m_control_layouts.end(),
            [control_id](const ControlLayout& layout) { return layout.control_id == control_id; });
        return found == m_control_layouts.end() ? nullptr : &*found;
    };

    const ControlLayout* ok_layout = find_control_layout(IDOK);
    int viewport_bottom = client_height;
    if (ok_layout != nullptr)
    {
        viewport_bottom = ok_layout->initial_rect.top + height_delta - m_button_content_gap;
    }

    int max_scroll_offset = media::CalculateMaxScrollOffset(m_content_height, viewport_bottom);
    const bool show_scrollbar = max_scroll_offset > 0;
    if (show_scrollbar != m_scrollbar_visible)
    {
        ShowScrollBar(SB_VERT, show_scrollbar ? TRUE : FALSE);
        m_scrollbar_visible = show_scrollbar;
        GetClientRect(&client_rect);
        client_width = client_rect.Width();
        client_height = client_rect.Height();
        height_delta = client_height - m_initial_client_size.cy;
        if (ok_layout != nullptr)
        {
            viewport_bottom = ok_layout->initial_rect.top + height_delta - m_button_content_gap;
        }
        else
        {
            viewport_bottom = client_height;
        }
        max_scroll_offset = media::CalculateMaxScrollOffset(m_content_height, viewport_bottom);
    }

    m_max_scroll_offset = max_scroll_offset;
    m_scroll_offset = media::ClampScrollOffset(m_scroll_offset, m_max_scroll_offset);
    m_content_viewport.SetRect(0, 0, client_width, (std::max)(0, viewport_bottom));

    SetRedraw(FALSE);
    for (const ControlLayout& control_layout : m_control_layouts)
    {
        const media::LayoutRect initial{
            control_layout.initial_rect.left,
            control_layout.initial_rect.top,
            control_layout.initial_rect.right,
            control_layout.initial_rect.bottom,
        };
        media::LayoutRect target = media::CalculateAnchoredControlRect(
            initial,
            m_initial_client_size.cx,
            client_width,
            control_layout.fixed_to_bottom ? 0 : m_scroll_offset,
            control_layout.horizontal_anchor);
        if (control_layout.fixed_to_bottom)
        {
            target.top += height_delta;
            target.bottom += height_delta;
        }

        const CRect target_rect(target.left, target.top, target.right, target.bottom);
        if (control_layout.fixed_to_bottom)
        {
            MoveControl(control_layout.control_id, target_rect, FALSE);
        }
        else
        {
            MoveScrollableControl(control_layout.control_id, target_rect, FALSE);
        }
    }

    UpdateScrollInfo(m_content_height, viewport_bottom);
    SetRedraw(TRUE);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);

    m_layout_updating = false;
}

void COptionsDlg::UpdateScrollInfo(int content_height, int viewport_height)
{
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = (std::max)(0, content_height - 1);
    info.nPage = static_cast<UINT>((std::max)(0, viewport_height));
    info.nPos = m_scroll_offset;
    SetScrollInfo(SB_VERT, &info, TRUE);
}

void COptionsDlg::SetScrollOffset(int offset)
{
    const int clamped_offset = media::ClampScrollOffset(offset, m_max_scroll_offset);
    if (clamped_offset == m_scroll_offset)
    {
        return;
    }
    m_scroll_offset = clamped_offset;
    ApplyLayout();
}

void COptionsDlg::OnSize(UINT type, int cx, int cy)
{
    CDialog::OnSize(type, cx, cy);
    if (type != SIZE_MINIMIZED)
    {
        ApplyLayout();
    }
}

void COptionsDlg::OnGetMinMaxInfo(MINMAXINFO* min_max_info)
{
    CDialog::OnGetMinMaxInfo(min_max_info);
    if (m_min_window_size.cx > 0 && m_min_window_size.cy > 0)
    {
        min_max_info->ptMinTrackSize.x = m_min_window_size.cx;
        min_max_info->ptMinTrackSize.y = m_min_window_size.cy;
    }
}

void COptionsDlg::OnVScroll(UINT scroll_code, UINT position, CScrollBar* scroll_bar)
{
    if (scroll_bar != nullptr)
    {
        CDialog::OnVScroll(scroll_code, position, scroll_bar);
        return;
    }

    int target = m_scroll_offset;
    switch (scroll_code)
    {
    case SB_LINEUP:
        target -= DPI(20);
        break;
    case SB_LINEDOWN:
        target += DPI(20);
        break;
    case SB_PAGEUP:
        target -= (std::max)(DPI(80), m_content_viewport.Height() - DPI(20));
        break;
    case SB_PAGEDOWN:
        target += (std::max)(DPI(80), m_content_viewport.Height() - DPI(20));
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
    {
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = SIF_TRACKPOS;
        if (GetScrollInfo(SB_VERT, &info))
        {
            target = info.nTrackPos;
        }
        break;
    }
    case SB_TOP:
        target = 0;
        break;
    case SB_BOTTOM:
        target = m_max_scroll_offset;
        break;
    default:
        return;
    }
    SetScrollOffset(target);
}

BOOL COptionsDlg::OnMouseWheel(UINT flags, short delta, CPoint point)
{
    if (m_max_scroll_offset > 0)
    {
        SetScrollOffset(m_scroll_offset - (delta > 0 ? DPI(40) : -DPI(40)));
        return TRUE;
    }
    return CDialog::OnMouseWheel(flags, delta, point);
}
