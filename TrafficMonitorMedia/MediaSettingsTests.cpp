#include "pch.h"
#include "MediaSettings.h"

using media::MediaControlAction;

constexpr media::SettingData kDefaultSettings{};
static_assert(kDefaultSettings.show_progress);
static_assert(kDefaultSettings.show_artist_on_second_line);
static_assert(kDefaultSettings.max_title_width == media::kDefaultMaxTitleWidth);
static_assert(kDefaultSettings.input.left_click == MediaControlAction::TogglePlayPause);
static_assert(kDefaultSettings.input.left_double_click == MediaControlAction::SkipNext);
static_assert(kDefaultSettings.input.right_click == MediaControlAction::None);
static_assert(kDefaultSettings.input.wheel_up == MediaControlAction::None);
static_assert(kDefaultSettings.input.wheel_down == MediaControlAction::None);

constexpr media::SettingData kRepeatedBindings{
    true,
    true,
    400,
    {
        MediaControlAction::TogglePlayPause,
        MediaControlAction::TogglePlayPause,
        MediaControlAction::TogglePlayPause,
        MediaControlAction::TogglePlayPause,
        MediaControlAction::TogglePlayPause,
    },
};
static_assert(kRepeatedBindings.input.left_click == kRepeatedBindings.input.wheel_down);

static_assert(media::ClampTitleWidth(20) == media::kMinimumTitleWidth);
static_assert(media::ClampTitleWidth(400) == 400);
static_assert(media::ClampTitleWidth(2000) == media::kMaximumTitleWidth);

static_assert(media::ToConfigValue(MediaControlAction::None) == L"none");
static_assert(media::ToConfigValue(MediaControlAction::TogglePlayPause) == L"toggle_play_pause");
static_assert(media::ToConfigValue(MediaControlAction::SkipPrevious) == L"skip_previous");
static_assert(media::ToConfigValue(MediaControlAction::SkipNext) == L"skip_next");
static_assert(media::ParseConfigValue(L"toggle_play_pause", MediaControlAction::None)
    == MediaControlAction::TogglePlayPause);
static_assert(media::ParseConfigValue(L"skip_previous", MediaControlAction::None)
    == MediaControlAction::SkipPrevious);
static_assert(media::ParseConfigValue(L"unknown", MediaControlAction::SkipNext)
    == MediaControlAction::SkipNext);

constexpr media::SettingData kNormalized = media::NormalizeSettings({
    true,
    true,
    5000,
    {
        static_cast<MediaControlAction>(99),
        MediaControlAction::SkipNext,
        MediaControlAction::None,
        MediaControlAction::TogglePlayPause,
        MediaControlAction::SkipPrevious,
    },
});
static_assert(kNormalized.max_title_width == media::kMaximumTitleWidth);
static_assert(kNormalized.input.left_click == kDefaultSettings.input.left_click);
static_assert(kNormalized.input.left_double_click == MediaControlAction::SkipNext);
static_assert(kNormalized.input.wheel_down == MediaControlAction::SkipPrevious);
static_assert(kNormalized != kDefaultSettings);
