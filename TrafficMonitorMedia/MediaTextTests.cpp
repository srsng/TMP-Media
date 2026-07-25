#include "pch.h"
#include "MediaText.h"

// 纯文本选择逻辑不依赖 TrafficMonitor 或 GSMTC；这些编译期断言是自动回归检查。
static_assert(media::SelectDisplayText(media::MediaTitleState::Loading, L"", L"") == media::kLoadingMediaText);
static_assert(media::SelectDisplayText(media::MediaTitleState::Ready, L"Song", L"Player") == L"Song");
static_assert(media::SelectDisplayText(media::MediaTitleState::Ready, L"", L"Player") == L"Player");
static_assert(media::SelectDisplayText(media::MediaTitleState::NoSession, L"", L"") == media::kNoMediaText);
static_assert(media::SelectDisplayText(media::MediaTitleState::Error, L"", L"") == media::kUnavailableMediaText);

constexpr media::TitleLineLayout kTitleAndArtist = media::SelectTitleLineLayout(
    media::MediaTitleState::Ready,
    L"Song",
    L"Artist",
    L"Player",
    true);
static_assert(kTitleAndArtist.primary == L"Song");
static_assert(kTitleAndArtist.secondary == L"Artist");
static_assert(kTitleAndArtist.split_lines);

constexpr media::TitleLineLayout kTitleOnlyByOption = media::SelectTitleLineLayout(
    media::MediaTitleState::Ready,
    L"Song",
    L"Artist",
    L"Player",
    false);
static_assert(kTitleOnlyByOption.primary == L"Song");
static_assert(kTitleOnlyByOption.secondary.empty());
static_assert(!kTitleOnlyByOption.split_lines);

constexpr media::TitleLineLayout kTitleOnlyWithoutArtist = media::SelectTitleLineLayout(
    media::MediaTitleState::Ready,
    L"Song",
    L"",
    L"Player",
    true);
static_assert(kTitleOnlyWithoutArtist.primary == L"Song");
static_assert(kTitleOnlyWithoutArtist.secondary.empty());
static_assert(!kTitleOnlyWithoutArtist.split_lines);

constexpr media::TitleLineLayout kNoMediaTwoLineTitle = media::SelectTitleLineLayout(
    media::MediaTitleState::NoSession,
    L"",
    L"",
    L"",
    true);
static_assert(kNoMediaTwoLineTitle.primary == media::kNoMediaText);
static_assert(kNoMediaTwoLineTitle.secondary.empty());
static_assert(!kNoMediaTwoLineTitle.split_lines);

constexpr media::TitleLineLayout kNoMediaIgnoresStaleArtist = media::SelectTitleLineLayout(
    media::MediaTitleState::NoSession,
    L"",
    L"Stale Artist",
    L"",
    true);
static_assert(kNoMediaIgnoresStaleArtist.primary == media::kNoMediaText);
static_assert(kNoMediaIgnoresStaleArtist.secondary.empty());
static_assert(!kNoMediaIgnoresStaleArtist.split_lines);
static_assert(media::CalculateTwoLineTextHeight(100, 16) == 32);
static_assert(media::CalculateTwoLineTextHeight(20, 16) == 20);
static_assert(media::CalculateTwoLineTextHeight(20, 0) == 0);
static_assert(media::CalculateProgressFraction(0, 0, 100) == 0.0);
static_assert(media::CalculateProgressFraction(25, 0, 100) == 0.25);
static_assert(media::CalculateProgressFraction(40, 10, 130) == 0.25);
static_assert(media::CalculateProgressFraction(-1, 0, 100) == 0.0);
static_assert(media::CalculateProgressFraction(150, 0, 100) == 1.0);
static_assert(media::CalculateProgressFraction(10, 10, 10) == 0.0);

// 图标表达点击后将执行的动作：播放中显示暂停，暂停时显示播放。
static_assert(media::SelectMediaStatusIcon(
    media::MediaTitleState::Loading,
    media::MediaPlaybackState::Playing) == media::MediaStatusIcon::NoMedia);
static_assert(media::SelectMediaStatusIcon(
    media::MediaTitleState::Ready,
    media::MediaPlaybackState::Playing) == media::MediaStatusIcon::Paused);
static_assert(media::SelectMediaStatusIcon(
    media::MediaTitleState::Ready,
    media::MediaPlaybackState::Paused) == media::MediaStatusIcon::Playing);
static_assert(media::SelectMediaStatusIcon(
    media::MediaTitleState::Ready,
    media::MediaPlaybackState::Unknown) == media::MediaStatusIcon::NoMedia);

using media::MediaControlAction;
static_assert(media::ResolveClickAction(
    false,
    MediaControlAction::SkipNext,
    false,
    MediaControlAction::TogglePlayPause) == MediaControlAction::None);
static_assert(media::ResolveClickAction(
    false,
    MediaControlAction::None,
    true,
    MediaControlAction::SkipPrevious) == MediaControlAction::SkipPrevious);
static_assert(media::ResolveClickAction(
    true,
    MediaControlAction::TogglePlayPause,
    false,
    MediaControlAction::None) == MediaControlAction::TogglePlayPause);
static_assert(media::ResolveClickAction(
    true,
    MediaControlAction::None,
    true,
    MediaControlAction::SkipNext) == MediaControlAction::None);
static_assert(media::ShouldScheduleSingleClick(MediaControlAction::SkipNext, false));
static_assert(media::ShouldScheduleSingleClick(MediaControlAction::SkipPrevious, false));
static_assert(!media::ShouldScheduleSingleClick(MediaControlAction::None, false));
static_assert(!media::ShouldScheduleSingleClick(MediaControlAction::TogglePlayPause, true));
