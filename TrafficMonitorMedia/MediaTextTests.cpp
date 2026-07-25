#include "pch.h"
#include "MediaText.h"

// 纯文本选择逻辑不依赖 TrafficMonitor 或 GSMTC；这些编译期断言是自动回归检查。
static_assert(media::SelectDisplayText(media::MediaTitleState::Loading, L"", L"") == media::kLoadingMediaText);
static_assert(media::SelectDisplayText(media::MediaTitleState::Ready, L"Song", L"Player") == L"Song");
static_assert(media::SelectDisplayText(media::MediaTitleState::Ready, L"", L"Player") == L"Player");
static_assert(media::SelectDisplayText(media::MediaTitleState::NoSession, L"", L"") == media::kNoMediaText);
static_assert(media::SelectDisplayText(media::MediaTitleState::Error, L"", L"") == media::kUnavailableMediaText);

static_assert(media::CalculateProgressFraction(0, 0, 100) == 0.0);
static_assert(media::CalculateProgressFraction(25, 0, 100) == 0.25);
static_assert(media::CalculateProgressFraction(40, 10, 130) == 0.25);
static_assert(media::CalculateProgressFraction(-1, 0, 100) == 0.0);
static_assert(media::CalculateProgressFraction(150, 0, 100) == 1.0);
static_assert(media::CalculateProgressFraction(10, 10, 10) == 0.0);

static_assert(media::SelectMediaStatusIcon(
    media::MediaTitleState::Loading,
    media::MediaPlaybackState::Playing) == media::MediaStatusIcon::NoMedia);
static_assert(media::SelectMediaStatusIcon(
    media::MediaTitleState::Ready,
    media::MediaPlaybackState::Playing) == media::MediaStatusIcon::Playing);
static_assert(media::SelectMediaStatusIcon(
    media::MediaTitleState::Ready,
    media::MediaPlaybackState::Paused) == media::MediaStatusIcon::Paused);
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
    MediaControlAction::SkipNext) == MediaControlAction::SkipNext);
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
static_assert(!media::ShouldScheduleSingleClick(MediaControlAction::None, false));
static_assert(!media::ShouldScheduleSingleClick(MediaControlAction::TogglePlayPause, true));
