#include "pch.h"
#include "TrafficMonitorMediaItem.h"
#include "TrafficMonitorMedia.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <gdiplus.h>

namespace
{
    constexpr int kHorizontalPadding96Dpi = 8;
    constexpr int kStatusIconSize96Dpi = 16;
    constexpr int kStatusIconGap96Dpi = 4;
    constexpr int kProgressHeight96Dpi = 2;
    constexpr BYTE kCoverOverlayAlpha = 136;

    class ScopedMemoryDc final
    {
    public:
        explicit ScopedMemoryDc(HDC compatible_dc) noexcept
            : m_dc(CreateCompatibleDC(compatible_dc))
        {
        }

        ~ScopedMemoryDc()
        {
            if (m_dc != nullptr)
            {
                DeleteDC(m_dc);
            }
        }

        ScopedMemoryDc(const ScopedMemoryDc&) = delete;
        ScopedMemoryDc& operator=(const ScopedMemoryDc&) = delete;

        [[nodiscard]] HDC Get() const noexcept { return m_dc; }

    private:
        HDC m_dc{};
    };

    class ScopedSelectedObject final
    {
    public:
        ScopedSelectedObject(HDC dc, HGDIOBJ object) noexcept
            : m_dc(dc)
            , m_old_object(dc != nullptr && object != nullptr ? SelectObject(dc, object) : nullptr)
        {
        }

        ~ScopedSelectedObject()
        {
            if (m_dc != nullptr && m_old_object != nullptr && m_old_object != HGDI_ERROR)
            {
                SelectObject(m_dc, m_old_object);
            }
        }

        ScopedSelectedObject(const ScopedSelectedObject&) = delete;
        ScopedSelectedObject& operator=(const ScopedSelectedObject&) = delete;

        [[nodiscard]] bool IsValid() const noexcept
        {
            return m_old_object != nullptr && m_old_object != HGDI_ERROR;
        }

    private:
        HDC m_dc{};
        HGDIOBJ m_old_object{};
    };

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

    class GdiplusSession final
    {
    public:
        GdiplusSession() noexcept
        {
            Gdiplus::GdiplusStartupInput input;
            if (Gdiplus::GdiplusStartup(&m_token, &input, nullptr) != Gdiplus::Ok)
            {
                m_token = 0;
            }
        }

        ~GdiplusSession()
        {
            if (m_token != 0)
            {
                Gdiplus::GdiplusShutdown(m_token);
            }
        }

        GdiplusSession(const GdiplusSession&) = delete;
        GdiplusSession& operator=(const GdiplusSession&) = delete;

        [[nodiscard]] bool Available() const noexcept { return m_token != 0; }

    private:
        ULONG_PTR m_token{};
    };

    bool DrawSmoothCoverImage(
        HDC target_dc,
        const media::MediaCoverImage& cover,
        const media::CoverCropRect& crop,
        int x,
        int y,
        int width,
        int height)
    {
        static GdiplusSession gdiplus;
        if (!gdiplus.Available())
        {
            return false;
        }

        Gdiplus::Bitmap bitmap(cover.Bitmap(), nullptr);
        if (bitmap.GetLastStatus() != Gdiplus::Ok)
        {
            return false;
        }

        Gdiplus::Graphics graphics(target_dc);
        if (graphics.GetLastStatus() != Gdiplus::Ok)
        {
            return false;
        }

        graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        Gdiplus::ImageAttributes attributes;
        attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        const Gdiplus::Rect destination(x, y, width, height);
        return graphics.DrawImage(
            &bitmap,
            destination,
            crop.x,
            crop.y,
            crop.width,
            crop.height,
            Gdiplus::UnitPixel,
            &attributes) == Gdiplus::Ok;
    }

    bool DrawCoverBackground(
        HDC target_dc,
        const media::MediaCoverImage& cover,
        int x,
        int y,
        int width,
        int height,
        bool smooth_scaling)
    {
        if (target_dc == nullptr || cover.Bitmap() == nullptr || width <= 0 || height <= 0)
        {
            return false;
        }

        const media::CoverCropRect crop = media::CalculateCoverCrop(
            cover.Width(),
            cover.Height(),
            width,
            height);
        if (crop.Empty())
        {
            return false;
        }

        BOOL cover_drawn = FALSE;
        if (smooth_scaling)
        {
            cover_drawn = DrawSmoothCoverImage(target_dc, cover, crop, x, y, width, height)
                ? TRUE
                : FALSE;
        }

        if (!cover_drawn)
        {
            ScopedMemoryDc source_dc(target_dc);
            if (source_dc.Get() == nullptr)
            {
                return false;
            }
            ScopedSelectedObject source_bitmap(source_dc.Get(), cover.Bitmap());
            if (!source_bitmap.IsValid())
            {
                return false;
            }

            const int old_stretch_mode = SetStretchBltMode(target_dc, HALFTONE);
            POINT old_brush_origin{};
            SetBrushOrgEx(target_dc, x, y, &old_brush_origin);
            cover_drawn = StretchBlt(
                target_dc,
                x,
                y,
                width,
                height,
                source_dc.Get(),
                crop.x,
                crop.y,
                crop.width,
                crop.height,
                SRCCOPY);
            SetBrushOrgEx(target_dc, old_brush_origin.x, old_brush_origin.y, nullptr);
            if (old_stretch_mode != 0)
            {
                SetStretchBltMode(target_dc, old_stretch_mode);
            }
            if (!cover_drawn)
            {
                return false;
            }
        }

        ScopedMemoryDc overlay_dc(target_dc);
        if (overlay_dc.Get() == nullptr)
        {
            return true;
        }

        BITMAPINFO overlay_info{};
        overlay_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        overlay_info.bmiHeader.biWidth = 1;
        overlay_info.bmiHeader.biHeight = -1;
        overlay_info.bmiHeader.biPlanes = 1;
        overlay_info.bmiHeader.biBitCount = 32;
        overlay_info.bmiHeader.biCompression = BI_RGB;

        void* overlay_pixels{};
        HBITMAP overlay_bitmap = CreateDIBSection(
            overlay_dc.Get(),
            &overlay_info,
            DIB_RGB_COLORS,
            &overlay_pixels,
            nullptr,
            0);
        if (overlay_bitmap == nullptr || overlay_pixels == nullptr)
        {
            if (overlay_bitmap != nullptr)
            {
                DeleteObject(overlay_bitmap);
            }
            return true;
        }

        *static_cast<std::uint32_t*>(overlay_pixels) = 0;
        {
            ScopedSelectedObject selected_overlay(overlay_dc.Get(), overlay_bitmap);
            if (selected_overlay.IsValid())
            {
                BLENDFUNCTION blend{};
                blend.BlendOp = AC_SRC_OVER;
                blend.SourceConstantAlpha = kCoverOverlayAlpha;
                AlphaBlend(
                    target_dc,
                    x,
                    y,
                    width,
                    height,
                    overlay_dc.Get(),
                    0,
                    0,
                    1,
                    1,
                    blend);
            }
        }
        DeleteObject(overlay_bitmap);
        return true;
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
        return g_plugin.DPI(settings.min_title_width) + status_icon_width;
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
    const int minimum_text_width = g_plugin.DPI(settings.min_title_width);
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
    const bool has_cover_background = settings.show_cover_background
        && snapshot.cover
        && w > 0
        && h > 0
        && DrawCoverBackground(pDC->GetSafeHdc(), *snapshot.cover, x, y, w, h, settings.smooth_cover_scaling);
    const bool effective_dark_mode = dark_mode || has_cover_background;
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
    const int content_height = (std::max)(0, h);

    if (show_timeline && w > 0 && progress_height > 0)
    {
        const double progress = std::clamp(snapshot.progress_fraction, 0.0, 1.0);
        const int progress_width = static_cast<int>(std::lround(static_cast<double>(w) * progress));
        const int progress_y = y + h - progress_height;
        pDC->FillSolidRect(x, progress_y, w, progress_height,
            effective_dark_mode ? RGB(88, 88, 88) : RGB(205, 205, 205));
        if (progress_width > 0)
        {
            pDC->FillSolidRect(x, progress_y, progress_width, progress_height,
                effective_dark_mode ? RGB(80, 170, 255) : RGB(0, 100, 210));
        }
    }

    if (settings.show_status_icon)
    {
        const media::MediaStatusIcon status_icon = media::SelectMediaStatusIcon(
            snapshot.state,
            snapshot.playback_state);
        const HICON icon = g_plugin.GetIcon(SelectStatusIconResource(status_icon, effective_dark_mode));
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

    const COLORREF text_color = effective_dark_mode ? RGB(245, 245, 245) : RGB(32, 32, 32);
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
}

int CTrafficMonitorMediaItem::OnMouseEvent(
    MouseEventType type,
    int x,
    int y,
    void* hWnd,
    int flag)
{
    if ((flag & MF_TASKBAR_WND) == 0)
    {
        return 0;
    }

    const MediaTitleSnapshot snapshot = g_plugin.GetMediaSnapshot();
    const media::SettingData settings = g_plugin.GetSettingsSnapshot();
    if (type == MT_WHEEL_UP || type == MT_WHEEL_DOWN)
    {
        switch (settings.input.wheel)
        {
        case media::WheelAction::None:
            return 0;
        case media::WheelAction::SwitchTrack:
        {
            const media::MediaControlAction wheel_action = type == MT_WHEEL_UP
                ? media::MediaControlAction::SkipPrevious
                : media::MediaControlAction::SkipNext;
            g_plugin.RequestImmediateAction(wheel_action);
            return 1;
        }
        case media::WheelAction::SwitchMediaSession:
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
        case media::WheelAction::AdjustSystemVolume:
        {
            const float delta = static_cast<float>(settings.system_volume_step_percent) / 100.0F;
            g_plugin.RequestAdjustSystemVolume(type == MT_WHEEL_UP ? delta : -delta);
            return 1;
        }
        default:
            return 0;
        }
    }

    media::MediaControlAction action = media::MediaControlAction::None;

    switch (type)
    {
    case MT_LCLICKED:
        action = settings.input.left_click;
        if (action == media::MediaControlAction::OpenMediaCard)
        {
            g_plugin.ScheduleOpenMediaCard(static_cast<HWND>(hWnd), x, y);
            return 1;
        }
        g_plugin.RequestSingleClick(action);
        return action != media::MediaControlAction::None ? 1 : 0;
    case MT_DBCLICKED:
    {
        g_plugin.SuppressScheduledMediaCardOpenAfterDoubleClick();
        action = settings.input.left_double_click;
        const media::DoubleClickDispatch dispatch = media::ResolveDoubleClickDispatch(action);
        g_plugin.RequestDoubleClick(dispatch.media_service_action);
        if (dispatch.open_media_card)
        {
            g_plugin.OpenMediaCard(static_cast<HWND>(hWnd), x, y);
        }
        return action != media::MediaControlAction::None
            || settings.input.left_click != media::MediaControlAction::None
            ? 1
            : 0;
    }
    case MT_RCLICKED:
        action = settings.input.right_click;
        if (action == media::MediaControlAction::OpenMediaCard)
        {
            g_plugin.OpenMediaCard(static_cast<HWND>(hWnd), x, y);
        }
        else
        {
            g_plugin.RequestImmediateAction(action);
        }
        return action != media::MediaControlAction::None ? 1 : 0;
    default:
        return 0;
    }
}
