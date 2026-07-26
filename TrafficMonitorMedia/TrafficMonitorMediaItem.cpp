#include "pch.h"
#include "TrafficMonitorMediaItem.h"
#include "TrafficMonitorMedia.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    constexpr int kHorizontalPadding96Dpi = 8;
    constexpr int kMinimumWidth96Dpi = 100;
    constexpr int kStatusIconSize96Dpi = 16;
    constexpr int kStatusIconGap96Dpi = 4;
    constexpr int kProgressHeight96Dpi = 2;
    constexpr int kProgressTopGap96Dpi = 2;

    UINT SelectStatusIconResource(
        media::MediaStatusIcon status,
        bool dark_mode)
    {
        switch (status)
        {
        case media::MediaStatusIcon::Playing:
            return dark_mode ? IDI_STATUS_PLAY_ON_DARK : IDI_STATUS_PLAY_ON_LIGHT;
        case media::MediaStatusIcon::Paused:
            return dark_mode ? IDI_STATUS_PAUSE_ON_DARK : IDI_STATUS_PAUSE_ON_LIGHT;
        case media::MediaStatusIcon::NoMedia:
        default:
            return dark_mode ? IDI_STATUS_NO_MEDIA_ON_DARK : IDI_STATUS_NO_MEDIA_ON_LIGHT;
        }
    }

}

const wchar_t* CTrafficMonitorMediaItem::GetItemName() const
{
    return g_plugin.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* CTrafficMonitorMediaItem::GetItemId() const
{
    return L"TrafficMonitorMediaTitle";
}

const wchar_t* CTrafficMonitorMediaItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CTrafficMonitorMediaItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* CTrafficMonitorMediaItem::GetItemValueSampleText() const
{
    return L"";
}

bool CTrafficMonitorMediaItem::IsCustomDraw() const
{
    return true;
}

int CTrafficMonitorMediaItem::IsDoubleLineExclusive() const
{
    return 1;
}

int CTrafficMonitorMediaItem::GetItemWidthEx(void* hDC) const
{
    const media::SettingData settings = g_plugin.GetSettingsSnapshot();
    const int status_icon_width = settings.show_status_icon
        ? g_plugin.DPI(kStatusIconSize96Dpi + kStatusIconGap96Dpi)
        : 0;

    CDC* pDC = CDC::FromHandle(static_cast<HDC>(hDC));
    if (pDC == nullptr)
    {
        return g_plugin.DPI(kMinimumWidth96Dpi) + status_icon_width;
    }

    const MediaTitleSnapshot snapshot = g_plugin.GetMediaSnapshot();
    const media::TitleLineLayout lines = media::SelectTitleLineLayout(
        snapshot.state,
        snapshot.title,
        snapshot.artist,
        snapshot.source_app_id,
        settings.show_artist_on_second_line);
    const std::wstring primary(lines.primary);
    const std::wstring secondary(lines.secondary);

    const int padding = g_plugin.DPI(kHorizontalPadding96Dpi);
    const int minimum_text_width = g_plugin.DPI(kMinimumWidth96Dpi);
    const int maximum_text_width = g_plugin.DPI(settings.max_title_width);
    int measured_text_width = pDC->GetTextExtent(primary.c_str()).cx;
    if (lines.split_lines)
    {
        measured_text_width = (std::max)(
            measured_text_width,
            static_cast<int>(pDC->GetTextExtent(secondary.c_str()).cx));
    }
    const int requested_text_width = measured_text_width + padding * 2;
    const int text_width = std::clamp(
        requested_text_width,
        minimum_text_width,
        maximum_text_width);

    return text_width + status_icon_width;
}

void CTrafficMonitorMediaItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    CDC* pDC = CDC::FromHandle(static_cast<HDC>(hDC));
    if (pDC == nullptr)
    {
        return;
    }

    const media::SettingData settings = g_plugin.GetSettingsSnapshot();
    const MediaTitleSnapshot snapshot = g_plugin.GetMediaSnapshot();
    const bool show_timeline = settings.show_progress && snapshot.has_timeline;
    const int padding = g_plugin.DPI(kHorizontalPadding96Dpi);
    const int icon_size = settings.show_status_icon
        ? g_plugin.DPI(kStatusIconSize96Dpi)
        : 0;
    const int icon_gap = settings.show_status_icon
        ? g_plugin.DPI(kStatusIconGap96Dpi)
        : 0;
    const int progress_height = show_timeline
        ? (std::max)(1, g_plugin.DPI(kProgressHeight96Dpi))
        : 0;
    const int progress_gap = show_timeline
        ? g_plugin.DPI(kProgressTopGap96Dpi)
        : 0;
    const int content_height = (std::max)(0, h - progress_height - progress_gap);

    if (settings.show_status_icon)
    {
        const media::MediaStatusIcon status_icon = media::SelectMediaStatusIcon(
            snapshot.state,
            snapshot.playback_state);
        const HICON icon = g_plugin.GetIcon(SelectStatusIconResource(status_icon, dark_mode));
        const int drawn_icon_size = (std::min)(icon_size, content_height);
        if (icon != nullptr && drawn_icon_size > 0)
        {
            const int icon_x = x + padding + ((icon_size - drawn_icon_size) / 2);
            const int icon_y = y + ((content_height - drawn_icon_size) / 2);
            DrawIconEx(
                pDC->GetSafeHdc(),
                icon_x,
                icon_y,
                icon,
                drawn_icon_size,
                drawn_icon_size,
                0,
                nullptr,
                DI_NORMAL);
        }
    }

    const media::TitleLineLayout lines = media::SelectTitleLineLayout(
        snapshot.state,
        snapshot.title,
        snapshot.artist,
        snapshot.source_app_id,
        settings.show_artist_on_second_line);
    const std::wstring primary(lines.primary);
    const std::wstring secondary(lines.secondary);
    const int text_x = x + padding + icon_size + icon_gap;
    const int text_width = (std::max)(
        0,
        w - padding * 2 - icon_size - icon_gap);
    CRect text_rect(CPoint(text_x, y), CSize(text_width, content_height));

    const COLORREF text_color = dark_mode ? RGB(245, 245, 245) : RGB(32, 32, 32);
    const COLORREF old_text_color = pDC->SetTextColor(text_color);
    const int old_background_mode = pDC->SetBkMode(TRANSPARENT);
    if (text_width > 0 && content_height > 0)
    {
        if (lines.split_lines)
        {
            const int first_line_height = content_height / 2;
            CRect primary_rect(text_rect);
            primary_rect.bottom = primary_rect.top + first_line_height;
            CRect secondary_rect(text_rect);
            secondary_rect.top = primary_rect.bottom;
            pDC->DrawText(
                primary.c_str(),
                primary_rect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            pDC->DrawText(
                secondary.c_str(),
                secondary_rect,
                DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        else
        {
            TEXTMETRIC text_metrics{};
            const int line_height = pDC->GetTextMetrics(&text_metrics)
                ? text_metrics.tmHeight
                : content_height;
            const int maximum_text_height = media::CalculateTwoLineTextHeight(
                content_height,
                line_height);
            CRect measured_rect(0, 0, text_width, 0);
            pDC->DrawText(
                primary.c_str(),
                measured_rect,
                DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
            const int drawn_text_height = (std::min)(maximum_text_height, measured_rect.Height());
            text_rect.top += (content_height - drawn_text_height) / 2;
            text_rect.bottom = text_rect.top + drawn_text_height;
            pDC->DrawText(
                primary.c_str(),
                text_rect,
                DT_WORDBREAK | DT_EDITCONTROL | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
    }

    pDC->SetBkMode(old_background_mode);
    pDC->SetTextColor(old_text_color);

    if (show_timeline && w > 0 && progress_height > 0)
    {
        const double progress = std::clamp(snapshot.progress_fraction, 0.0, 1.0);
        const int progress_width = static_cast<int>(std::lround(static_cast<double>(w) * progress));
        const int progress_y = y + h - progress_height;
        pDC->FillSolidRect(x, progress_y, w, progress_height,
            dark_mode ? RGB(88, 88, 88) : RGB(205, 205, 205));
        if (progress_width > 0)
        {
            pDC->FillSolidRect(x, progress_y, progress_width, progress_height,
                dark_mode ? RGB(80, 170, 255) : RGB(0, 100, 210));
        }
    }
}

int CTrafficMonitorMediaItem::OnMouseEvent(
    MouseEventType type,
    int,
    int,
    void*,
    int flag)
{
    if ((flag & MF_TASKBAR_WND) == 0)
    {
        return 0;
    }

    const MediaTitleSnapshot snapshot = g_plugin.GetMediaSnapshot();
    if (type == MT_WHEEL_UP || type == MT_WHEEL_DOWN)
    {
        if (!snapshot.can_switch_session)
        {
            return 0;
        }

        const media::SessionSwitchDirection direction = type == MT_WHEEL_UP
            ? media::SessionSwitchDirection::Previous
            : media::SessionSwitchDirection::Next;
        g_plugin.RequestSwitchSession(direction);
        return 1;
    }

    const media::SettingData settings = g_plugin.GetSettingsSnapshot();
    media::MediaControlAction action = media::MediaControlAction::None;

    switch (type)
    {
    case MT_LCLICKED:
        action = settings.input.left_click;
        g_plugin.RequestSingleClick(action);
        return action != media::MediaControlAction::None ? 1 : 0;
    case MT_DBCLICKED:
        action = settings.input.left_double_click;
        g_plugin.RequestDoubleClick(action);
        return action != media::MediaControlAction::None
            || settings.input.left_click != media::MediaControlAction::None
            ? 1
            : 0;
    case MT_RCLICKED:
        action = settings.input.right_click;
        g_plugin.RequestImmediateAction(action);
        return action != media::MediaControlAction::None ? 1 : 0;
    default:
        return 0;
    }
}
