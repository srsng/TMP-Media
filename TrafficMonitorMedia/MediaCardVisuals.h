#pragma once

namespace media
{
    struct MediaCardPixelRect
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

    using MediaCardIconRect = MediaCardPixelRect;

    enum class MediaCardButtonIcon
    {
        ChevronLeft,
        ChevronRight,
        SkipPrevious,
        Play,
        Pause,
        SkipNext,
    };

    [[nodiscard]] constexpr MediaCardIconRect CalculateMediaCardIconRect(
        MediaCardPixelRect button,
        int icon_width,
        int icon_height,
        int optical_offset_x,
        int optical_offset_y) noexcept
    {
        if (button.Width() <= 0 || button.Height() <= 0 || icon_width <= 0 || icon_height <= 0)
        {
            return {};
        }

        const int left = button.left + (button.Width() - icon_width) / 2 + optical_offset_x;
        const int top = button.top + (button.Height() - icon_height) / 2 + optical_offset_y;
        return {
            left,
            top,
            left + icon_width,
            top + icon_height,
        };
    }
}
