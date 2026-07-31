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
        OpenMediaCard,
    };

    enum class WheelAction
    {
        None,
        SwitchTrack,
        SwitchMediaSession,
        AdjustSystemVolume,
    };

    inline constexpr int kMinimumTitleWidth = 100;
    inline constexpr int kDefaultMinTitleWidth = 100;
    inline constexpr int kDefaultMaxTitleWidth = 400;
    inline constexpr int kMaximumTitleWidth = 1000;
    inline constexpr int kMinimumSystemVolumeStepPercent = 1;
    inline constexpr int kDefaultSystemVolumeStepPercent = 4;
    inline constexpr int kMaximumSystemVolumeStepPercent = 50;

    struct InputBindings
    {
        MediaControlAction icon_left_click{ MediaControlAction::TogglePlayPause };
        MediaControlAction icon_left_double_click{ MediaControlAction::None };
        MediaControlAction title_left_click{ MediaControlAction::OpenMediaCard };
        MediaControlAction title_left_double_click{ MediaControlAction::None };
        MediaControlAction right_click{ MediaControlAction::None };
        WheelAction wheel{ WheelAction::SwitchMediaSession };

        constexpr bool operator==(const InputBindings&) const = default;
    };

    [[nodiscard]] constexpr InputBindings BuildInputBindingFallbacks(
        InputBindings defaults,
        MediaControlAction legacy_left_click,
        MediaControlAction legacy_left_double_click) noexcept
    {
        defaults.title_left_click = legacy_left_click;
        defaults.title_left_double_click = legacy_left_double_click;
        return defaults;
    }

    struct SettingData
    {
        bool show_progress{ true };
        bool show_status_icon{ true };
        bool show_artist_on_second_line{ true };
        bool show_cover_background{ false };
        bool smooth_cover_scaling{ true };
        int min_title_width{ kDefaultMinTitleWidth };
        int max_title_width{ kDefaultMaxTitleWidth };
        int system_volume_step_percent{ kDefaultSystemVolumeStepPercent };
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
        case MediaControlAction::OpenMediaCard:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] constexpr bool IsValidWheelAction(WheelAction action) noexcept
    {
        switch (action)
        {
        case WheelAction::None:
        case WheelAction::SwitchTrack:
        case WheelAction::SwitchMediaSession:
        case WheelAction::AdjustSystemVolume:
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

    [[nodiscard]] constexpr int ClampSystemVolumeStepPercent(int step_percent) noexcept
    {
        if (step_percent < kMinimumSystemVolumeStepPercent)
        {
            return kMinimumSystemVolumeStepPercent;
        }
        if (step_percent > kMaximumSystemVolumeStepPercent)
        {
            return kMaximumSystemVolumeStepPercent;
        }
        return step_percent;
    }

    [[nodiscard]] constexpr MediaControlAction NormalizeAction(
        MediaControlAction action,
        MediaControlAction fallback) noexcept
    {
        return IsValidAction(action) ? action : fallback;
    }

    [[nodiscard]] constexpr WheelAction NormalizeWheelAction(
        WheelAction action,
        WheelAction fallback) noexcept
    {
        return IsValidWheelAction(action) ? action : fallback;
    }

    [[nodiscard]] constexpr SettingData NormalizeSettings(SettingData settings) noexcept
    {
        const SettingData defaults;
        settings.min_title_width = ClampTitleWidth(settings.min_title_width);
        settings.max_title_width = ClampTitleWidth(settings.max_title_width);
        if (settings.min_title_width > settings.max_title_width)
        {
            settings.min_title_width = settings.max_title_width;
        }
        settings.system_volume_step_percent = ClampSystemVolumeStepPercent(
            settings.system_volume_step_percent);
        settings.input.icon_left_click = NormalizeAction(
            settings.input.icon_left_click,
            defaults.input.icon_left_click);
        settings.input.icon_left_double_click = NormalizeAction(
            settings.input.icon_left_double_click,
            defaults.input.icon_left_double_click);
        settings.input.title_left_click = NormalizeAction(
            settings.input.title_left_click,
            defaults.input.title_left_click);
        settings.input.title_left_double_click = NormalizeAction(
            settings.input.title_left_double_click,
            defaults.input.title_left_double_click);
        settings.input.right_click = NormalizeAction(settings.input.right_click, defaults.input.right_click);
        settings.input.wheel = NormalizeWheelAction(settings.input.wheel, defaults.input.wheel);
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
        case MediaControlAction::OpenMediaCard:
            return L"open_media_card";
        case MediaControlAction::None:
        default:
            return L"none";
        }
    }

    [[nodiscard]] constexpr std::wstring_view ToConfigValue(WheelAction action) noexcept
    {
        switch (action)
        {
        case WheelAction::SwitchTrack:
            return L"switch_track";
        case WheelAction::SwitchMediaSession:
            return L"switch_media_session";
        case WheelAction::AdjustSystemVolume:
            return L"adjust_system_volume";
        case WheelAction::None:
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
        if (value == L"open_media_card")
        {
            return MediaControlAction::OpenMediaCard;
        }
        return fallback;
    }

    [[nodiscard]] constexpr WheelAction ParseConfigValue(
        std::wstring_view value,
        WheelAction fallback) noexcept
    {
        if (value == L"none")
        {
            return WheelAction::None;
        }
        if (value == L"switch_track")
        {
            return WheelAction::SwitchTrack;
        }
        if (value == L"switch_media_session")
        {
            return WheelAction::SwitchMediaSession;
        }
        if (value == L"adjust_system_volume")
        {
            return WheelAction::AdjustSystemVolume;
        }
        return fallback;
    }
}
