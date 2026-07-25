#pragma once

#include <string_view>

namespace media
{
    enum class MediaControlAction
    {
        None,
        TogglePlayPause,
        SkipPrevious,
        SkipNext,
    };

    inline constexpr int kMinimumTitleWidth = 100;
    inline constexpr int kDefaultMaxTitleWidth = 400;
    inline constexpr int kMaximumTitleWidth = 1000;

    struct InputBindings
    {
        MediaControlAction left_click{ MediaControlAction::TogglePlayPause };
        MediaControlAction left_double_click{ MediaControlAction::SkipNext };
        MediaControlAction right_click{ MediaControlAction::None };
        MediaControlAction wheel_up{ MediaControlAction::None };
        MediaControlAction wheel_down{ MediaControlAction::None };

        constexpr bool operator==(const InputBindings&) const = default;
    };

    struct SettingData
    {
        bool show_progress{ true };
        bool show_artist_on_second_line{ true };
        int max_title_width{ kDefaultMaxTitleWidth };
        InputBindings input{};

        constexpr bool operator==(const SettingData&) const = default;
    };

    [[nodiscard]] constexpr bool IsValidAction(MediaControlAction action) noexcept
    {
        switch (action)
        {
        case MediaControlAction::None:
        case MediaControlAction::TogglePlayPause:
        case MediaControlAction::SkipPrevious:
        case MediaControlAction::SkipNext:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] constexpr int ClampTitleWidth(int width) noexcept
    {
        if (width < kMinimumTitleWidth)
        {
            return kMinimumTitleWidth;
        }
        if (width > kMaximumTitleWidth)
        {
            return kMaximumTitleWidth;
        }
        return width;
    }

    [[nodiscard]] constexpr MediaControlAction NormalizeAction(
        MediaControlAction action,
        MediaControlAction fallback) noexcept
    {
        return IsValidAction(action) ? action : fallback;
    }

    [[nodiscard]] constexpr SettingData NormalizeSettings(SettingData settings) noexcept
    {
        const SettingData defaults;
        settings.max_title_width = ClampTitleWidth(settings.max_title_width);
        settings.input.left_click = NormalizeAction(settings.input.left_click, defaults.input.left_click);
        settings.input.left_double_click = NormalizeAction(
            settings.input.left_double_click,
            defaults.input.left_double_click);
        settings.input.right_click = NormalizeAction(settings.input.right_click, defaults.input.right_click);
        settings.input.wheel_up = NormalizeAction(settings.input.wheel_up, defaults.input.wheel_up);
        settings.input.wheel_down = NormalizeAction(settings.input.wheel_down, defaults.input.wheel_down);
        return settings;
    }

    [[nodiscard]] constexpr std::wstring_view ToConfigValue(MediaControlAction action) noexcept
    {
        switch (action)
        {
        case MediaControlAction::TogglePlayPause:
            return L"toggle_play_pause";
        case MediaControlAction::SkipPrevious:
            return L"skip_previous";
        case MediaControlAction::SkipNext:
            return L"skip_next";
        case MediaControlAction::None:
        default:
            return L"none";
        }
    }

    [[nodiscard]] constexpr MediaControlAction ParseConfigValue(
        std::wstring_view value,
        MediaControlAction fallback) noexcept
    {
        if (value == L"none")
        {
            return MediaControlAction::None;
        }
        if (value == L"toggle_play_pause")
        {
            return MediaControlAction::TogglePlayPause;
        }
        if (value == L"skip_previous")
        {
            return MediaControlAction::SkipPrevious;
        }
        if (value == L"skip_next")
        {
            return MediaControlAction::SkipNext;
        }
        return fallback;
    }
}
