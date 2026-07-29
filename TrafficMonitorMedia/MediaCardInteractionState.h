#pragma once

#include <cstdint>

namespace media
{
    [[nodiscard]] constexpr bool ShouldScheduleMediaCardOpen(
        std::uint64_t now_milliseconds,
        std::uint64_t suppression_deadline_milliseconds) noexcept
    {
        return now_milliseconds >= suppression_deadline_milliseconds;
    }

    [[nodiscard]] constexpr std::uint64_t CalculateMediaCardOpenSuppressionDeadline(
        std::uint64_t now_milliseconds,
        std::uint64_t double_click_interval_milliseconds) noexcept
    {
        return now_milliseconds + double_click_interval_milliseconds;
    }

    struct MediaCardLifecycleState
    {
        bool close_in_progress{};
        bool card_marked_visible{};

        [[nodiscard]] constexpr bool ShouldHandleUnexpectedCardDestroy() const noexcept
        {
            return card_marked_visible && !close_in_progress;
        }
    };
}
