#include "pch.h"
#include "MediaSettings.h"

using media::MediaControlAction;
using media::WheelAction;

constexpr media::SettingData kDefaultSettings{};
static_assert(kDefaultSettings.show_progress);
static_assert(kDefaultSettings.show_status_icon);
static_assert(kDefaultSettings.show_artist_on_second_line);
static_assert(!kDefaultSettings.show_cover_background);
static_assert(kDefaultSettings.smooth_cover_scaling);
static_assert(kDefaultSettings.min_title_width == media::kDefaultMinTitleWidth);
static_assert(kDefaultSettings.max_title_width == media::kDefaultMaxTitleWidth);
static_assert(kDefaultSettings.system_volume_step_percent == media::kDefaultSystemVolumeStepPercent);
static_assert(media::kDefaultSystemVolumeStepPercent == 4);
static_assert(kDefaultSettings.input.left_click == MediaControlAction::OpenMediaCard);
static_assert(kDefaultSettings.input.left_double_click == MediaControlAction::TogglePlayPause);
static_assert(kDefaultSettings.input.right_click == MediaControlAction::None);
static_assert(kDefaultSettings.input.wheel == WheelAction::SwitchMediaSession);

constexpr media::SettingData kRepeatedBindings{
    true,
    true,
    true,
    false,
    true,
    100,
    400,
    5,
    {
        MediaControlAction::TogglePlayPause,
        MediaControlAction::TogglePlayPause,
        MediaControlAction::TogglePlayPause,
        WheelAction::SwitchTrack,
    },
};
static_assert(kRepeatedBindings.input.left_click == kRepeatedBindings.input.right_click);
static_assert(kRepeatedBindings.input.wheel == WheelAction::SwitchTrack);

static_assert(media::ClampTitleWidth(20) == media::kMinimumTitleWidth);
static_assert(media::ClampTitleWidth(400) == 400);
static_assert(media::ClampTitleWidth(2000) == media::kMaximumTitleWidth);

constexpr media::SettingData kInvertedTitleWidths = media::NormalizeSettings({
    true,
    true,
    true,
    false,
    true,
    700,
    300,
    4,
    {},
});
static_assert(kInvertedTitleWidths.min_title_width == 300);
static_assert(kInvertedTitleWidths.max_title_width == 300);
static_assert(media::ClampSystemVolumeStepPercent(0) == media::kMinimumSystemVolumeStepPercent);
static_assert(media::ClampSystemVolumeStepPercent(5) == 5);
static_assert(media::ClampSystemVolumeStepPercent(99) == media::kMaximumSystemVolumeStepPercent);

static_assert(media::ToConfigValue(MediaControlAction::None) == L"none");
static_assert(media::ToConfigValue(MediaControlAction::TogglePlayPause) == L"toggle_play_pause");
static_assert(media::ToConfigValue(MediaControlAction::SkipPrevious) == L"skip_previous");
static_assert(media::ToConfigValue(MediaControlAction::SkipNext) == L"skip_next");
static_assert(media::ToConfigValue(MediaControlAction::OpenMediaCard) == L"open_media_card");
static_assert(media::ParseConfigValue(L"toggle_play_pause", MediaControlAction::None)
    == MediaControlAction::TogglePlayPause);
static_assert(media::ParseConfigValue(L"skip_previous", MediaControlAction::None)
    == MediaControlAction::SkipPrevious);
static_assert(media::ParseConfigValue(L"open_media_card", MediaControlAction::None)
    == MediaControlAction::OpenMediaCard);
static_assert(media::ParseConfigValue(L"unknown", MediaControlAction::SkipNext)
    == MediaControlAction::SkipNext);

static_assert(media::ToConfigValue(WheelAction::None) == L"none");
static_assert(media::ToConfigValue(WheelAction::SwitchTrack) == L"switch_track");
static_assert(media::ToConfigValue(WheelAction::SwitchMediaSession) == L"switch_media_session");
static_assert(media::ToConfigValue(WheelAction::AdjustSystemVolume) == L"adjust_system_volume");
static_assert(media::ParseConfigValue(L"switch_track", WheelAction::None)
    == WheelAction::SwitchTrack);
static_assert(media::ParseConfigValue(L"switch_media_session", WheelAction::None)
    == WheelAction::SwitchMediaSession);
static_assert(media::ParseConfigValue(L"adjust_system_volume", WheelAction::None)
    == WheelAction::AdjustSystemVolume);
static_assert(media::ParseConfigValue(L"unknown", WheelAction::SwitchTrack)
    == WheelAction::SwitchTrack);

constexpr media::SettingData kNormalized = media::NormalizeSettings({
    true,
    false,
    true,
    true,
    false,
    20,
    5000,
    99,
    {
        static_cast<MediaControlAction>(99),
        MediaControlAction::TogglePlayPause,
        MediaControlAction::None,
        static_cast<WheelAction>(99),
    },
});
static_assert(!kNormalized.show_status_icon);
static_assert(kNormalized.show_cover_background);
static_assert(!kNormalized.smooth_cover_scaling);
static_assert(kNormalized.min_title_width == media::kMinimumTitleWidth);
static_assert(kNormalized.max_title_width == media::kMaximumTitleWidth);
static_assert(kNormalized.system_volume_step_percent == media::kMaximumSystemVolumeStepPercent);
static_assert(kNormalized.input.left_click == kDefaultSettings.input.left_click);
static_assert(kNormalized.input.left_double_click == MediaControlAction::TogglePlayPause);
static_assert(kNormalized.input.wheel == kDefaultSettings.input.wheel);
static_assert(kNormalized != kDefaultSettings);
