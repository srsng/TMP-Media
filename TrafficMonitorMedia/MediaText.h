#pragma once

#include "MediaSettings.h"

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

    enum class MediaPlaybackState
    {
        Unknown,
        Playing,
        Paused,
    };

    enum class MediaStatusIcon
    {
        NoMedia,
        Playing,
        Paused,
    };

    [[nodiscard]] constexpr MediaStatusIcon SelectMediaStatusIcon(
        MediaTitleState title_state,
        MediaPlaybackState playback_state) noexcept
    {
        if (title_state != MediaTitleState::Ready)
        {
            return MediaStatusIcon::NoMedia;
        }

        switch (playback_state)
        {
        case MediaPlaybackState::Playing:
            return MediaStatusIcon::Paused;
        case MediaPlaybackState::Paused:
            return MediaStatusIcon::Playing;
        case MediaPlaybackState::Unknown:
        default:
            return MediaStatusIcon::NoMedia;
        }
    }

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

    struct TitleLineLayout
    {
        std::wstring_view primary;
        std::wstring_view secondary;
        bool split_lines{};
    };

    [[nodiscard]] constexpr TitleLineLayout SelectTitleLineLayout(
        MediaTitleState state,
        std::wstring_view title,
        std::wstring_view artist,
        std::wstring_view source_app_id,
        bool show_artist_on_second_line) noexcept
    {
        const std::wstring_view primary = SelectDisplayText(state, title, source_app_id);
        if (state == MediaTitleState::Ready && show_artist_on_second_line && !artist.empty())
        {
            return { primary, artist, true };
        }
        return { primary, {}, false };
    }
    [[nodiscard]] constexpr int CalculateTwoLineTextHeight(
        int available_height,
        int line_height) noexcept
    {
        if (available_height <= 0 || line_height <= 0)
        {
            return 0;
        }
        if (line_height >= (available_height + 1) / 2)
        {
            return available_height;
        }
        return line_height * 2;
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

    [[nodiscard]] constexpr bool ShouldScheduleSingleClick(
        MediaControlAction action,
        bool suppression_active) noexcept
    {
        return action != MediaControlAction::None && !suppression_active;
    }

    // 双击动作（包括“无操作”）优先于已经到期的单击动作。
    [[nodiscard]] constexpr MediaControlAction ResolveClickAction(
        bool has_double_click,
        MediaControlAction double_click_action,
        bool has_matured_single_click,
        MediaControlAction single_click_action) noexcept
    {
        if (has_double_click)
        {
            return double_click_action;
        }
        return has_matured_single_click
            ? single_click_action
            : MediaControlAction::None;
    }
}
