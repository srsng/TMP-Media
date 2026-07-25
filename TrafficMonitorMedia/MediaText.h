#pragma once

#include <cstdint>
#include <string_view>

namespace media
{
    enum class MediaTitleState
    {
        Loading,
        NoSession,
        Ready,
        Error,
    };

    enum class MediaControlAction
    {
        None,
        TogglePlayPause,
        SkipNext,
    };

    inline constexpr std::wstring_view kLoadingMediaText{ L"正在获取媒体…" };
    inline constexpr std::wstring_view kNoMediaText{ L"未检测到媒体" };
    inline constexpr std::wstring_view kUnavailableMediaText{ L"媒体不可用" };

    [[nodiscard]] constexpr std::wstring_view SelectDisplayText(
        MediaTitleState state,
        std::wstring_view title,
        std::wstring_view source_app_id) noexcept
    {
        if (!title.empty())
        {
            return title;
        }
        if (!source_app_id.empty())
        {
            return source_app_id;
        }

        switch (state)
        {
        case MediaTitleState::Loading:
            return kLoadingMediaText;
        case MediaTitleState::Error:
            return kUnavailableMediaText;
        case MediaTitleState::NoSession:
        case MediaTitleState::Ready:
        default:
            return kNoMediaText;
        }
    }

    [[nodiscard]] constexpr double CalculateProgressFraction(
        std::int64_t position,
        std::int64_t start,
        std::int64_t end) noexcept
    {
        if (end <= start || position <= start)
        {
            return 0.0;
        }
        if (position >= end)
        {
            return 1.0;
        }
        return static_cast<double>(position - start) / static_cast<double>(end - start);
    }

    [[nodiscard]] constexpr bool ShouldScheduleSingleClick(bool suppression_active) noexcept
    {
        return !suppression_active;
    }

    // 双击优先于已经到期的单击，避免“下一首”额外触发播放/暂停。
    [[nodiscard]] constexpr MediaControlAction ResolveClickAction(
        bool has_double_click,
        bool has_matured_single_click) noexcept
    {
        if (has_double_click)
        {
            return MediaControlAction::SkipNext;
        }
        return has_matured_single_click
            ? MediaControlAction::TogglePlayPause
            : MediaControlAction::None;
    }
}