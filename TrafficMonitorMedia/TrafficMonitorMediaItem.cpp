#include "pch.h"
#include "TrafficMonitorMediaItem.h"
#include "TrafficMonitorMedia.h"

#include <algorithm>

namespace
{
    constexpr int kHorizontalPadding96Dpi = 8;
    constexpr int kMinimumWidth96Dpi = 100;
    constexpr int kMaximumWidth96Dpi = 400;
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

    const std::wstring text = g_plugin.GetMediaDisplayText();
    const int padding = g_plugin.DPI(kHorizontalPadding96Dpi);
    CRect text_rect(CPoint(x + padding, y), CSize((std::max)(0, w - padding * 2), h));

    const COLORREF old_text_color = pDC->SetTextColor(dark_mode ? RGB(245, 245, 245) : RGB(32, 32, 32));
    const int old_background_mode = pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(
        text.c_str(),
        text_rect,
        DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    pDC->SetBkMode(old_background_mode);
    pDC->SetTextColor(old_text_color);
}
