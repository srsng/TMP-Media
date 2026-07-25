#include "pch.h"
#include "TrafficMonitorMediaItem.h"
#include "TrafficMonitorMedia.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int kHorizontalPadding96Dpi = 8;
    constexpr int kMinimumWidth96Dpi = 100;
    constexpr int kMaximumWidth96Dpi = 400;
    constexpr int kProgressHeight96Dpi = 2;
    constexpr int kProgressTopGap96Dpi = 2;
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

int CTrafficMonitorMediaItem::GetItemWidthEx(void* hDC) const
{
    CDC* pDC = CDC::FromHandle(static_cast<HDC>(hDC));
    if (pDC == nullptr)
    {
        return g_plugin.DPI(kMinimumWidth96Dpi);
    }

    const std::wstring text = g_plugin.GetMediaDisplayText();
    const int padding = g_plugin.DPI(kHorizontalPadding96Dpi);
    const int minimum_width = g_plugin.DPI(kMinimumWidth96Dpi);
    const int maximum_width = g_plugin.DPI(kMaximumWidth96Dpi);
    const int requested_width = pDC->GetTextExtent(text.c_str()).cx + padding * 2;
    return std::clamp(requested_width, minimum_width, maximum_width);
}

void CTrafficMonitorMediaItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    CDC* pDC = CDC::FromHandle(static_cast<HDC>(hDC));
    if (pDC == nullptr)
    {
        return;
    }

    const bool has_timeline = g_plugin.HasMediaTimeline();
    const int padding = g_plugin.DPI(kHorizontalPadding96Dpi);
    const int progress_height = has_timeline
        ? (std::max)(1, g_plugin.DPI(kProgressHeight96Dpi))
        : 0;
    const int progress_gap = has_timeline
        ? g_plugin.DPI(kProgressTopGap96Dpi)
        : 0;
    const int text_height = (std::max)(0, h - progress_height - progress_gap);

    const std::wstring text = g_plugin.GetMediaDisplayText();
    CRect text_rect(CPoint(x + padding, y), CSize((std::max)(0, w - padding * 2), text_height));

    const COLORREF old_text_color = pDC->SetTextColor(dark_mode ? RGB(245, 245, 245) : RGB(32, 32, 32));
    const int old_background_mode = pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(
        text.c_str(),
        text_rect,
        DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    pDC->SetBkMode(old_background_mode);
    pDC->SetTextColor(old_text_color);

    if (has_timeline && w > 0 && progress_height > 0)
    {
        const double progress = std::clamp(g_plugin.GetMediaProgressFraction(), 0.0, 1.0);
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

    switch (type)
    {
    case MT_LCLICKED:
        g_plugin.RequestTogglePlayPause();
        return 1;
    case MT_DBCLICKED:
        g_plugin.RequestSkipNext();
        return 1;
    default:
        return 0;
    }
}
