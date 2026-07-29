#pragma once

namespace media
{
    struct LayoutRect
    {
        int left{};
        int top{};
        int right{};
        int bottom{};

        [[nodiscard]] constexpr int Width() const noexcept
        {
            return right - left;
        }

        [[nodiscard]] constexpr int Height() const noexcept
        {
            return bottom - top;
        }
    };

    enum class HorizontalAnchor
    {
        Left,
        Right,
        Stretch,
    };

    [[nodiscard]] constexpr LayoutRect IntersectLayoutRect(
        LayoutRect rect,
        LayoutRect clip) noexcept
    {
        const int left = rect.left > clip.left ? rect.left : clip.left;
        const int top = rect.top > clip.top ? rect.top : clip.top;
        const int right = rect.right < clip.right ? rect.right : clip.right;
        const int bottom = rect.bottom < clip.bottom ? rect.bottom : clip.bottom;
        if (right <= left || bottom <= top)
        {
            return { left, top, left, top };
        }
        return { left, top, right, bottom };
    }

    [[nodiscard]] constexpr LayoutRect CalculateAnchoredControlRect(
        LayoutRect initial_rect,
        int initial_client_width,
        int client_width,
        int scroll_offset,
        HorizontalAnchor anchor) noexcept
    {
        const int width_delta = client_width - initial_client_width;
        if (anchor == HorizontalAnchor::Right)
        {
            initial_rect.left += width_delta;
            initial_rect.right += width_delta;
        }
        else if (anchor == HorizontalAnchor::Stretch)
        {
            initial_rect.right += width_delta;
        }

        initial_rect.top -= scroll_offset;
        initial_rect.bottom -= scroll_offset;
        return initial_rect;
    }

    [[nodiscard]] constexpr int CalculateMaxScrollOffset(
        int content_height,
        int viewport_height) noexcept
    {
        return content_height > viewport_height
            ? content_height - viewport_height
            : 0;
    }

    [[nodiscard]] constexpr int ClampScrollOffset(
        int requested_offset,
        int maximum_offset) noexcept
    {
        if (requested_offset < 0)
        {
            return 0;
        }
        if (requested_offset > maximum_offset)
        {
            return maximum_offset;
        }
        return requested_offset;
    }
}
