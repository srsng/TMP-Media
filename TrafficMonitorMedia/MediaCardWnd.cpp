#include "pch.h"
#include "MediaCardWnd.h"

#include "MediaCardWindowManager.h"
#include "TrafficMonitorMedia.h"

#include <algorithm>
#include <cmath>

#include <gdiplus.h>

namespace
{
    constexpr UINT_PTR kRefreshTimerId = 1;
    constexpr UINT kRefreshIntervalMilliseconds = 250;

    int Scale(int value)
    {
        return g_plugin.DPI(value);
    }

    CString FormatTime(std::int64_t ticks)
    {
        constexpr std::int64_t kTicksPerSecond = 10'000'000;
        const std::int64_t total_seconds = (std::max<std::int64_t>)(0, ticks / kTicksPerSecond);
        const std::int64_t hours = total_seconds / 3600;
        const std::int64_t minutes = (total_seconds / 60) % 60;
        const std::int64_t seconds = total_seconds % 60;
        CString text;
        if (hours > 0)
        {
            text.Format(L"%lld:%02lld:%02lld", hours, minutes, seconds);
        }
        else
        {
            text.Format(L"%lld:%02lld", total_seconds / 60, seconds);
        }
        return text;
    }

    void DrawCenteredText(CDC& dc, const wchar_t* text, const CRect& rect, COLORREF color)
    {
        const COLORREF old_color = dc.SetTextColor(color);
        const int old_mode = dc.SetBkMode(TRANSPARENT);
        CRect draw_rect(rect);
        dc.DrawText(text, draw_rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        dc.SetBkMode(old_mode);
        dc.SetTextColor(old_color);
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

        [[nodiscard]] bool Available() const noexcept
        {
            return m_token != 0;
        }

    private:
        ULONG_PTR m_token{};
    };

    GdiplusSession& GetGdiplusSession()
    {
        static GdiplusSession session;
        return session;
    }

    Gdiplus::Color ToGdiplusColor(COLORREF color)
    {
        return Gdiplus::Color(
            255,
            GetRValue(color),
            GetGValue(color),
            GetBValue(color));
    }

    void ConfigureVectorGraphics(Gdiplus::Graphics& graphics)
    {
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    }

    bool FillSmoothEllipse(HDC target_dc, const CRect& rect, COLORREF color)
    {
        if (target_dc == nullptr || rect.IsRectEmpty() || !GetGdiplusSession().Available())
        {
            return false;
        }

        Gdiplus::Graphics graphics(target_dc);
        if (graphics.GetLastStatus() != Gdiplus::Ok)
        {
            return false;
        }
        ConfigureVectorGraphics(graphics);

        Gdiplus::SolidBrush brush(ToGdiplusColor(color));
        const Gdiplus::RectF bounds(
            static_cast<Gdiplus::REAL>(rect.left) + 0.5F,
            static_cast<Gdiplus::REAL>(rect.top) + 0.5F,
            static_cast<Gdiplus::REAL>((std::max)(0, rect.Width() - 1)),
            static_cast<Gdiplus::REAL>((std::max)(0, rect.Height() - 1)));
        return graphics.FillEllipse(&brush, bounds) == Gdiplus::Ok;
    }

    Gdiplus::RectF ToRectF(const media::MediaCardIconRect& rect)
    {
        return Gdiplus::RectF(
            static_cast<Gdiplus::REAL>(rect.left),
            static_cast<Gdiplus::REAL>(rect.top),
            static_cast<Gdiplus::REAL>(rect.Width()),
            static_cast<Gdiplus::REAL>(rect.Height()));
    }

    void DrawVectorIcon(
        Gdiplus::Graphics& graphics,
        media::MediaCardButtonIcon icon,
        const Gdiplus::RectF& bounds,
        const Gdiplus::Color& color)
    {
        const auto x = [&bounds](Gdiplus::REAL fraction)
        {
            return bounds.X + bounds.Width * fraction;
        };
        const auto y = [&bounds](Gdiplus::REAL fraction)
        {
            return bounds.Y + bounds.Height * fraction;
        };

        Gdiplus::SolidBrush brush(color);
        switch (icon)
        {
        case media::MediaCardButtonIcon::ChevronLeft:
        case media::MediaCardButtonIcon::ChevronRight:
        {
            const bool left = icon == media::MediaCardButtonIcon::ChevronLeft;
            const Gdiplus::REAL outer = left ? 0.64F : 0.36F;
            const Gdiplus::REAL inner = left ? 0.38F : 0.62F;
            Gdiplus::PointF points[] = {
                { x(outer), y(0.20F) },
                { x(inner), y(0.50F) },
                { x(outer), y(0.80F) },
            };
            Gdiplus::Pen pen(color, (std::max)(1.5F, bounds.Width * 0.13F));
            pen.SetStartCap(Gdiplus::LineCapRound);
            pen.SetEndCap(Gdiplus::LineCapRound);
            pen.SetLineJoin(Gdiplus::LineJoinRound);
            graphics.DrawLines(&pen, points, static_cast<INT>(std::size(points)));
            break;
        }
        case media::MediaCardButtonIcon::SkipPrevious:
        {
            graphics.FillRectangle(
                &brush,
                Gdiplus::RectF(x(0.18F), y(0.20F), bounds.Width * 0.12F, bounds.Height * 0.60F));
            Gdiplus::PointF points[] = {
                { x(0.38F), y(0.50F) },
                { x(0.82F), y(0.20F) },
                { x(0.82F), y(0.80F) },
            };
            graphics.FillPolygon(&brush, points, static_cast<INT>(std::size(points)));
            break;
        }
        case media::MediaCardButtonIcon::SkipNext:
        {
            Gdiplus::PointF points[] = {
                { x(0.18F), y(0.20F) },
                { x(0.62F), y(0.50F) },
                { x(0.18F), y(0.80F) },
            };
            graphics.FillPolygon(&brush, points, static_cast<INT>(std::size(points)));
            graphics.FillRectangle(
                &brush,
                Gdiplus::RectF(x(0.70F), y(0.20F), bounds.Width * 0.12F, bounds.Height * 0.60F));
            break;
        }
        case media::MediaCardButtonIcon::Play:
        {
            Gdiplus::PointF points[] = {
                { x(0.25F), y(0.16F) },
                { x(0.82F), y(0.50F) },
                { x(0.25F), y(0.84F) },
            };
            graphics.FillPolygon(&brush, points, static_cast<INT>(std::size(points)));
            break;
        }
        case media::MediaCardButtonIcon::Pause:
            graphics.FillRectangle(
                &brush,
                Gdiplus::RectF(x(0.22F), y(0.17F), bounds.Width * 0.20F, bounds.Height * 0.66F));
            graphics.FillRectangle(
                &brush,
                Gdiplus::RectF(x(0.58F), y(0.17F), bounds.Width * 0.20F, bounds.Height * 0.66F));
            break;
        }
    }

    bool DrawSmoothButton(
        HDC target_dc,
        const CRect& rect,
        media::MediaCardButtonIcon icon,
        COLORREF background,
        COLORREF foreground)
    {
        if (target_dc == nullptr || rect.IsRectEmpty() || !GetGdiplusSession().Available())
        {
            return false;
        }

        Gdiplus::Graphics graphics(target_dc);
        if (graphics.GetLastStatus() != Gdiplus::Ok)
        {
            return false;
        }
        ConfigureVectorGraphics(graphics);

        Gdiplus::SolidBrush background_brush(ToGdiplusColor(background));
        const Gdiplus::RectF circle(
            static_cast<Gdiplus::REAL>(rect.left) + 0.5F,
            static_cast<Gdiplus::REAL>(rect.top) + 0.5F,
            static_cast<Gdiplus::REAL>((std::max)(0, rect.Width() - 1)),
            static_cast<Gdiplus::REAL>((std::max)(0, rect.Height() - 1)));
        graphics.FillEllipse(&background_brush, circle);

        const bool is_header_icon = icon == media::MediaCardButtonIcon::ChevronLeft
            || icon == media::MediaCardButtonIcon::ChevronRight;
        const bool is_primary_icon = icon == media::MediaCardButtonIcon::Play
            || icon == media::MediaCardButtonIcon::Pause;
        const int icon_size = Scale(is_header_icon ? 16 : (is_primary_icon ? 20 : 18));
        const int optical_offset_x = icon == media::MediaCardButtonIcon::Play ? Scale(1) : 0;
        const media::MediaCardIconRect icon_rect = media::CalculateMediaCardIconRect(
            media::MediaCardPixelRect{ rect.left, rect.top, rect.right, rect.bottom },
            icon_size,
            icon_size,
            optical_offset_x,
            0);
        DrawVectorIcon(graphics, icon, ToRectF(icon_rect), ToGdiplusColor(foreground));
        return true;
    }
}

BEGIN_MESSAGE_MAP(CMediaCardWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_NCDESTROY()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_TIMER()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

CMediaCardWnd::CMediaCardWnd(CMediaCardWindowManager* manager)
    : m_manager(manager)
{
}

BOOL CMediaCardWnd::Create(CWnd* owner, const CRect& rect)
{
    const CString class_name = AfxRegisterWndClass(
        CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
        LoadCursor(nullptr, IDC_ARROW),
        nullptr,
        nullptr);
    return CreateEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        class_name,
        L"",
        WS_POPUP | WS_BORDER,
        rect,
        owner,
        0);
}

void CMediaCardWnd::ApplyAnimationFrame(BYTE alpha, const CRect& rect)
{
    if (!GetSafeHwnd())
    {
        return;
    }

    SetLayeredWindowAttributes(0, alpha, LWA_ALPHA);
    SetWindowPos(
        &wndTop,
        rect.left,
        rect.top,
        rect.Width(),
        rect.Height(),
        SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void CMediaCardWnd::SetInteractionEnabled(bool enabled) noexcept
{
    m_interaction_enabled = enabled;
}

int CMediaCardWnd::OnCreate(LPCREATESTRUCT create_struct)
{
    if (CWnd::OnCreate(create_struct) == -1)
    {
        return -1;
    }
    SetTimer(kRefreshTimerId, kRefreshIntervalMilliseconds, nullptr);
    return 0;
}

void CMediaCardWnd::OnClose()
{
    if (m_manager != nullptr)
    {
        m_manager->Close();
        return;
    }
    CWnd::OnClose();
}

void CMediaCardWnd::OnDestroy()
{
    KillTimer(kRefreshTimerId);
    CancelSeekPreview();
    CWnd::OnDestroy();
}

void CMediaCardWnd::OnNcDestroy()
{
    CWnd::OnNcDestroy();
    if (m_manager != nullptr)
    {
        m_manager->OnCardDestroyed();
    }
}

void CMediaCardWnd::RefreshSnapshot()
{
    const MediaTitleSnapshot snapshot = g_plugin.GetMediaSnapshot();
    const bool media_changed = snapshot.source_app_id != m_snapshot.source_app_id
        || snapshot.title != m_snapshot.title
        || snapshot.artist != m_snapshot.artist;
    m_snapshot = snapshot;
    if (media_changed && m_dragging_progress)
    {
        CancelSeekPreview();
    }
    if (GetSafeHwnd())
    {
        Invalidate(FALSE);
    }
}

void CMediaCardWnd::OnTimer(UINT_PTR timer_id)
{
    if (timer_id == kRefreshTimerId)
    {
        RefreshSnapshot();
        return;
    }
    CWnd::OnTimer(timer_id);
}

void CMediaCardWnd::OnKeyDown(UINT character, UINT repeat_count, UINT flags)
{
    if (!m_interaction_enabled)
    {
        return;
    }
    if (character == VK_ESCAPE && m_manager != nullptr)
    {
        m_manager->Close();
        return;
    }
    CWnd::OnKeyDown(character, repeat_count, flags);
}

BOOL CMediaCardWnd::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CMediaCardWnd::OnPaint()
{
    CPaintDC paint_dc(this);
    CRect client;
    GetClientRect(&client);
    if (client.IsRectEmpty())
    {
        return;
    }

    CDC memory_dc;
    memory_dc.CreateCompatibleDC(&paint_dc);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(&paint_dc, client.Width(), client.Height());
    CBitmap* old_bitmap = memory_dc.SelectObject(&bitmap);
    DrawCard(memory_dc, client);
    paint_dc.BitBlt(0, 0, client.Width(), client.Height(), &memory_dc, 0, 0, SRCCOPY);
    memory_dc.SelectObject(old_bitmap);
}

CMediaCardWnd::Layout CMediaCardWnd::CalculateLayout(const CRect& client) const
{
    const int padding = Scale(16);
    const int header_height = Scale(30);
    const int footer_height = Scale(42);
    const int gap = Scale(12);
    const int button = Scale(30);
    const int play_button = Scale(38);
    const int switch_button = Scale(26);

    Layout layout;
    const int content_left = client.left + padding;
    const int content_right = client.right - padding;
    const int header_top = client.top + padding;
    const int header_bottom = header_top + header_height;
    const int footer_bottom = client.bottom - padding;
    const int footer_top = footer_bottom - footer_height;
    const int body_top = header_bottom + gap;
    const int body_bottom = footer_top - gap;

    if (m_snapshot.can_switch_session)
    {
        layout.switch_next = CRect(
            content_right - switch_button,
            header_top + (header_height - switch_button) / 2,
            content_right,
            header_top + (header_height + switch_button) / 2);
        layout.switch_previous = layout.switch_next;
        layout.switch_previous.OffsetRect(-switch_button - Scale(4), 0);
    }

    int text_left = content_left;
    const int body_height = (std::max)(0, body_bottom - body_top);
    if (m_snapshot.cover && body_height > 0)
    {
        const int cover_size = (std::min)(Scale(124), body_height);
        layout.cover = CRect(content_left, body_top, content_left + cover_size, body_top + cover_size);
        text_left = layout.cover.right + Scale(16);
    }

    const int text_right = content_right;
    layout.title = CRect(text_left, body_top, text_right, body_top + Scale(28));
    layout.artist = CRect(text_left, layout.title.bottom + Scale(2), text_right, layout.title.bottom + Scale(24));

    const int controls_width = button * 2 + play_button + Scale(20);
    const int controls_left = text_left + (std::max)(0, (text_right - text_left - controls_width) / 2);
    const int controls_top = body_bottom - play_button;
    layout.previous = CRect(
        controls_left,
        controls_top + (play_button - button) / 2,
        controls_left + button,
        controls_top + (play_button + button) / 2);
    layout.play_pause = CRect(
        layout.previous.right + Scale(10),
        controls_top,
        layout.previous.right + Scale(10) + play_button,
        controls_top + play_button);
    layout.next = CRect(
        layout.play_pause.right + Scale(10),
        controls_top + (play_button - button) / 2,
        layout.play_pause.right + Scale(10) + button,
        controls_top + (play_button + button) / 2);

    layout.elapsed = CRect(content_left, footer_top, content_left + Scale(62), footer_top + Scale(20));
    layout.duration = CRect(content_right - Scale(62), footer_top, content_right, footer_top + Scale(20));
    layout.progress = CRect(content_left, footer_bottom - Scale(12), content_right, footer_bottom);
    return layout;
}

void CMediaCardWnd::DrawCard(CDC& dc, const CRect& client)
{
    dc.FillSolidRect(client, RGB(32, 32, 32));
    CPen border_pen(PS_SOLID, 1, RGB(68, 68, 68));
    CPen* old_pen = dc.SelectObject(&border_pen);
    dc.MoveTo(client.left, client.top);
    dc.LineTo(client.right - 1, client.top);
    dc.LineTo(client.right - 1, client.bottom - 1);
    dc.LineTo(client.left, client.bottom - 1);
    dc.LineTo(client.left, client.top);
    dc.SelectObject(old_pen);

    m_layout = CalculateLayout(client);
    const int old_mode = dc.SetBkMode(TRANSPARENT);
    const COLORREF old_color = dc.SetTextColor(RGB(242, 242, 242));

    CFont title_font;
    title_font.CreateFont(
        -Scale(18), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    CFont normal_font;
    normal_font.CreateFont(
        -Scale(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    CFont small_font;
    small_font.CreateFont(
        -Scale(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    CFont* old_font = dc.SelectObject(&normal_font);
    CRect source_rect(client.left + Scale(16), client.top + Scale(16), client.right - Scale(96), client.top + Scale(46));
    const std::wstring source = !m_snapshot.source_app_name.empty()
        ? m_snapshot.source_app_name
        : (!m_snapshot.source_app_id.empty() ? m_snapshot.source_app_id : L"当前媒体会话");
    dc.DrawText(source.c_str(), source_rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    if (!m_layout.cover.IsRectEmpty())
    {
        DrawCover(dc, m_layout.cover);
    }

    dc.SelectObject(&title_font);
    const std::wstring title = !m_snapshot.title.empty()
        ? m_snapshot.title
        : std::wstring(media::SelectDisplayText(m_snapshot.state, L"", m_snapshot.source_app_id));
    CRect title_rect(m_layout.title);
    dc.DrawText(title.c_str(), title_rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    dc.SelectObject(&normal_font);
    dc.SetTextColor(RGB(180, 180, 180));
    CRect artist_rect(m_layout.artist);
    dc.DrawText(
        m_snapshot.artist.c_str(),
        artist_rect,
        DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

    DrawButton(
        dc,
        m_layout.switch_previous,
        media::MediaCardButtonIcon::ChevronLeft,
        m_snapshot.can_switch_session,
        false);
    DrawButton(
        dc,
        m_layout.switch_next,
        media::MediaCardButtonIcon::ChevronRight,
        m_snapshot.can_switch_session,
        false);
    DrawButton(
        dc,
        m_layout.previous,
        media::MediaCardButtonIcon::SkipPrevious,
        m_snapshot.can_skip_previous,
        false);
    DrawButton(
        dc,
        m_layout.play_pause,
        m_snapshot.playback_state == media::MediaPlaybackState::Playing
            ? media::MediaCardButtonIcon::Pause
            : media::MediaCardButtonIcon::Play,
        m_snapshot.can_play_pause,
        true);
    DrawButton(
        dc,
        m_layout.next,
        media::MediaCardButtonIcon::SkipNext,
        m_snapshot.can_skip_next,
        false);

    if (m_snapshot.has_timeline)
    {
        const double fraction = std::clamp(
            m_dragging_progress ? m_preview_fraction : m_snapshot.progress_fraction,
            0.0,
            1.0);
        const int track_y = m_layout.progress.CenterPoint().y;
        const int track_height = (std::max)(2, Scale(4));
        const int progress_width = static_cast<int>(std::lround(m_layout.progress.Width() * fraction));
        dc.FillSolidRect(
            m_layout.progress.left,
            track_y - track_height / 2,
            m_layout.progress.Width(),
            track_height,
            RGB(92, 92, 92));
        if (progress_width > 0)
        {
            dc.FillSolidRect(
                m_layout.progress.left,
                track_y - track_height / 2,
                progress_width,
                track_height,
                RGB(0, 120, 215));
        }
        if (m_snapshot.can_seek)
        {
            const int knob_radius = Scale(5);
            const int knob_x = m_layout.progress.left + progress_width;
            const CRect knob_rect(
                knob_x - knob_radius,
                track_y - knob_radius,
                knob_x + knob_radius + 1,
                track_y + knob_radius + 1);
            if (!FillSmoothEllipse(dc.GetSafeHdc(), knob_rect, RGB(245, 245, 245)))
            {
                CBrush knob_brush(RGB(245, 245, 245));
                CBrush* old_brush = dc.SelectObject(&knob_brush);
                CPen* old_knob_pen = static_cast<CPen*>(dc.SelectStockObject(NULL_PEN));
                dc.Ellipse(knob_rect);
                dc.SelectObject(old_knob_pen);
                dc.SelectObject(old_brush);
            }
        }

        const std::int64_t preview_position = media::PositionFromProgressFraction(
            fraction,
            m_snapshot.timeline_start_ticks,
            m_snapshot.timeline_end_ticks);
        dc.SelectObject(&small_font);
        DrawCenteredText(dc, FormatTime(preview_position - m_snapshot.timeline_start_ticks), m_layout.elapsed, RGB(170, 170, 170));
        DrawCenteredText(dc, FormatTime(m_snapshot.timeline_end_ticks - m_snapshot.timeline_start_ticks), m_layout.duration, RGB(170, 170, 170));
    }

    dc.SelectObject(old_font);
    dc.SetTextColor(old_color);
    dc.SetBkMode(old_mode);
}

void CMediaCardWnd::DrawCover(CDC& dc, const CRect& rect) const
{
    if (!m_snapshot.cover || m_snapshot.cover->Bitmap() == nullptr || rect.IsRectEmpty())
    {
        return;
    }

    const media::CoverFitRect fit = media::CalculateCoverFitRect(
        m_snapshot.cover->Width(),
        m_snapshot.cover->Height(),
        rect.Width(),
        rect.Height());
    if (fit.Empty())
    {
        return;
    }

    CDC source_dc;
    source_dc.CreateCompatibleDC(&dc);
    HGDIOBJ old_bitmap = source_dc.SelectObject(m_snapshot.cover->Bitmap());
    const int old_mode = dc.SetStretchBltMode(HALFTONE);
    POINT old_brush_origin{};
    SetBrushOrgEx(dc.GetSafeHdc(), rect.left + fit.x, rect.top + fit.y, &old_brush_origin);
    dc.StretchBlt(
        rect.left + fit.x,
        rect.top + fit.y,
        fit.width,
        fit.height,
        &source_dc,
        0,
        0,
        m_snapshot.cover->Width(),
        m_snapshot.cover->Height(),
        SRCCOPY);
    SetBrushOrgEx(dc.GetSafeHdc(), old_brush_origin.x, old_brush_origin.y, nullptr);
    dc.SetStretchBltMode(old_mode);
    source_dc.SelectObject(old_bitmap);
}

void CMediaCardWnd::DrawButton(
    CDC& dc,
    const CRect& rect,
    media::MediaCardButtonIcon icon,
    bool enabled,
    bool primary) const
{
    if (rect.IsRectEmpty())
    {
        return;
    }

    const COLORREF background = enabled
        ? (primary ? RGB(0, 120, 215) : RGB(58, 58, 58))
        : RGB(43, 43, 43);
    const COLORREF foreground = enabled ? RGB(248, 248, 248) : RGB(105, 105, 105);
    if (DrawSmoothButton(dc.GetSafeHdc(), rect, icon, background, foreground))
    {
        return;
    }

    CBrush brush(background);
    CBrush* old_brush = dc.SelectObject(&brush);
    CPen* old_pen = static_cast<CPen*>(dc.SelectStockObject(NULL_PEN));
    dc.Ellipse(rect);
    dc.SelectObject(old_pen);
    dc.SelectObject(old_brush);
}

double CMediaCardWnd::ProgressFractionAt(CPoint point) const
{
    if (m_layout.progress.Width() <= 0)
    {
        return 0.0;
    }
    return std::clamp(
        static_cast<double>(point.x - m_layout.progress.left) / m_layout.progress.Width(),
        0.0,
        1.0);
}

void CMediaCardWnd::OnLButtonDown(UINT flags, CPoint point)
{
    if (!m_interaction_enabled)
    {
        return;
    }
    SetFocus();
    if (m_layout.switch_previous.PtInRect(point) && m_snapshot.can_switch_session)
    {
        g_plugin.RequestSwitchSession(media::SessionSwitchDirection::Previous);
        RefreshSnapshot();
        return;
    }
    if (m_layout.switch_next.PtInRect(point) && m_snapshot.can_switch_session)
    {
        g_plugin.RequestSwitchSession(media::SessionSwitchDirection::Next);
        RefreshSnapshot();
        return;
    }
    if (m_layout.previous.PtInRect(point) && m_snapshot.can_skip_previous)
    {
        g_plugin.RequestImmediateAction(media::MediaControlAction::SkipPrevious);
        return;
    }
    if (m_layout.play_pause.PtInRect(point) && m_snapshot.can_play_pause)
    {
        g_plugin.RequestImmediateAction(media::MediaControlAction::TogglePlayPause);
        return;
    }
    if (m_layout.next.PtInRect(point) && m_snapshot.can_skip_next)
    {
        g_plugin.RequestImmediateAction(media::MediaControlAction::SkipNext);
        return;
    }
    if (m_layout.progress.PtInRect(point) && m_snapshot.has_timeline && m_snapshot.can_seek)
    {
        m_dragging_progress = true;
        m_preview_fraction = ProgressFractionAt(point);
        SetCapture();
        Invalidate(FALSE);
        return;
    }
    CWnd::OnLButtonDown(flags, point);
}

void CMediaCardWnd::OnMouseMove(UINT flags, CPoint point)
{
    if (!m_interaction_enabled)
    {
        return;
    }
    if (m_dragging_progress)
    {
        m_preview_fraction = ProgressFractionAt(point);
        Invalidate(FALSE);
        return;
    }
    CWnd::OnMouseMove(flags, point);
}

void CMediaCardWnd::OnLButtonUp(UINT flags, CPoint point)
{
    if (!m_interaction_enabled)
    {
        return;
    }
    if (m_dragging_progress)
    {
        m_preview_fraction = ProgressFractionAt(point);
        const double fraction = m_preview_fraction;
        m_dragging_progress = false;
        if (GetCapture() == this)
        {
            ReleaseCapture();
        }
        SubmitSeek(fraction);
        Invalidate(FALSE);
        return;
    }
    CWnd::OnLButtonUp(flags, point);
}

void CMediaCardWnd::OnCaptureChanged(CWnd* window)
{
    if (m_dragging_progress)
    {
        CancelSeekPreview();
        Invalidate(FALSE);
    }
    CWnd::OnCaptureChanged(window);
}

void CMediaCardWnd::SubmitSeek(double fraction)
{
    if (!m_snapshot.has_timeline || !m_snapshot.can_seek)
    {
        return;
    }
    g_plugin.RequestSeekToPosition(
        m_snapshot.session_identity,
        media::PositionFromProgressFraction(
            fraction,
            m_snapshot.timeline_start_ticks,
            m_snapshot.timeline_end_ticks));
}

void CMediaCardWnd::CancelSeekPreview()
{
    m_dragging_progress = false;
    m_preview_fraction = 0.0;
    if (GetCapture() == this)
    {
        ReleaseCapture();
    }
}
