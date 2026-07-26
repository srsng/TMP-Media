#include "pch.h"
#include "MediaText.h"
#include "MediaSessionSelection.h"

#include <array>

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
constexpr std::array<media::SessionIdentity, 0> kNoSessions{};
constexpr std::array<media::SessionIdentity, 3> kThreeSessions{ 11, 22, 33 };
constexpr std::array<media::SessionIdentity, 1> kOneSession{ 11 };

constexpr media::SessionSelection kNoSessionSelection = media::ResolveSessionSelection(
    kNoSessions,
    media::kNoSessionIdentity,
    media::kNoSessionIdentity,
    false);
static_assert(kNoSessionSelection.selected_index == media::kNoSessionIndex);
static_assert(!kNoSessionSelection.manual_selection);

constexpr media::SessionSelection kFollowSystemSelection = media::ResolveSessionSelection(
    kThreeSessions,
    22,
    33,
    false);
static_assert(kFollowSystemSelection.selected_index == 1);
static_assert(!kFollowSystemSelection.manual_selection);

constexpr media::SessionSelection kKeepManualSelection = media::ResolveSessionSelection(
    kThreeSessions,
    11,
    33,
    true);
static_assert(kKeepManualSelection.selected_index == 2);
static_assert(kKeepManualSelection.manual_selection);

constexpr media::SessionSelection kFallbackToSystemSelection = media::ResolveSessionSelection(
    kThreeSessions,
    22,
    44,
    true);
static_assert(kFallbackToSystemSelection.selected_index == 1);
static_assert(!kFallbackToSystemSelection.manual_selection);

constexpr media::SessionSelection kFallbackToFirstSelection = media::ResolveSessionSelection(
    kThreeSessions,
    44,
    55,
    true);
static_assert(kFallbackToFirstSelection.selected_index == 0);
static_assert(!kFallbackToFirstSelection.manual_selection);

constexpr media::SessionSelection kNextSelection = media::SelectNextSession(
    kThreeSessions,
    11,
    22,
    true);
static_assert(kNextSelection.selected_index == 2);
static_assert(kNextSelection.manual_selection);

constexpr media::SessionSelection kWrappedSelection = media::SelectNextSession(
    kThreeSessions,
    11,
    33,
    true);
static_assert(kWrappedSelection.selected_index == 0);
static_assert(kWrappedSelection.manual_selection);

constexpr media::SessionSelection kPreviousSelection = media::SelectPreviousSession(
    kThreeSessions,
    11,
    22,
    true);
static_assert(kPreviousSelection.selected_index == 0);
static_assert(kPreviousSelection.manual_selection);

constexpr media::SessionSelection kPreviousWrappedSelection = media::SelectPreviousSession(
    kThreeSessions,
    11,
    11,
    true);
static_assert(kPreviousWrappedSelection.selected_index == 2);
static_assert(kPreviousWrappedSelection.manual_selection);
constexpr media::SessionSelection kSingleSelectionCannotSwitch = media::SelectNextSession(
    kOneSession,
    11,
    11,
    false);
static_assert(kSingleSelectionCannotSwitch.selected_index == 0);
static_assert(!kSingleSelectionCannotSwitch.manual_selection);
