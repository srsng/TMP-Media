#pragma once

#include "MediaSettings.h"

#include <cstddef>

namespace media
{
    struct MediaItemRect
    {
        int left{};
        int top{};
        int right{};
        int bottom{};
    };

    class MediaItemTaskbarRectCache
    {
    public:
        constexpr void RecordDrawRect(MediaItemRect rect, bool drawing_taskbar_window) noexcept
        {
            m_last_draw_rect = rect;
            m_has_last_draw_rect = true;
            if (drawing_taskbar_window)
            {
                m_taskbar_draw_rect = rect;
                m_has_taskbar_draw_rect = true;
            }
        }

        [[nodiscard]] constexpr bool HasRectForHitTest() const noexcept
        {
            return m_has_taskbar_draw_rect || m_has_last_draw_rect;
        }

        [[nodiscard]] constexpr bool HasTaskbarRectForHitTest() const noexcept
        {
            return m_has_taskbar_draw_rect;
        }

        [[nodiscard]] constexpr MediaItemRect GetRectForHitTest() const noexcept
        {
            return m_has_taskbar_draw_rect ? m_taskbar_draw_rect : m_last_draw_rect;
        }

    private:
        MediaItemRect m_last_draw_rect{};
        MediaItemRect m_taskbar_draw_rect{};
        bool m_has_last_draw_rect{};
        bool m_has_taskbar_draw_rect{};
    };

    enum class MediaItemHitRegion
    {
        Icon,
        Title,
    };

    [[nodiscard]] constexpr std::size_t MediaItemHitRegionIndex(
        MediaItemHitRegion region) noexcept
    {
        return region == MediaItemHitRegion::Icon ? 0U : 1U;
    }

    [[nodiscard]] constexpr MediaItemHitRegion ResolveMediaItemHitRegion(
        MediaItemRect item_rect,
        bool point_uses_window_coordinates,
        bool show_status_icon,
        int horizontal_padding,
        int icon_size,
        int point_x,
        int point_y) noexcept
    {
        const int local_x = point_uses_window_coordinates
            ? point_x - item_rect.left
            : point_x;
        const int local_y = point_uses_window_coordinates
            ? point_y - item_rect.top
            : point_y;
        const int item_width = item_rect.right - item_rect.left;
        const int item_height = item_rect.bottom - item_rect.top;

        if (!show_status_icon
            || local_x < 0
            || local_x >= item_width
            || local_y < 0
            || local_y >= item_height)
        {
            return MediaItemHitRegion::Title;
        }

        const int icon_left = horizontal_padding;
        const int icon_right = icon_left + icon_size;
        return local_x >= icon_left && local_x < icon_right
            ? MediaItemHitRegion::Icon
            : MediaItemHitRegion::Title;
    }

    [[nodiscard]] constexpr MediaControlAction SelectLeftClickAction(
        const InputBindings& bindings,
        MediaItemHitRegion region) noexcept
    {
        return region == MediaItemHitRegion::Icon
            ? bindings.icon_left_click
            : bindings.title_left_click;
    }

    [[nodiscard]] constexpr MediaControlAction SelectLeftDoubleClickAction(
        const InputBindings& bindings,
        MediaItemHitRegion region) noexcept
    {
        return region == MediaItemHitRegion::Icon
            ? bindings.icon_left_double_click
            : bindings.title_left_double_click;
    }
}
