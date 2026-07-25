#include "pch.h"
#include "MediaText.h"

// 纯文本选择逻辑不依赖 TrafficMonitor 或 GSMTC；这些编译期断言是自动回归检查。
static_assert(media::SelectDisplayText(media::MediaTitleState::Loading, L"", L"") == media::kLoadingMediaText);
static_assert(media::SelectDisplayText(media::MediaTitleState::Ready, L"Song", L"Player") == L"Song");
static_assert(media::SelectDisplayText(media::MediaTitleState::Ready, L"", L"Player") == L"Player");
static_assert(media::SelectDisplayText(media::MediaTitleState::NoSession, L"", L"") == media::kNoMediaText);
static_assert(media::SelectDisplayText(media::MediaTitleState::Error, L"", L"") == media::kUnavailableMediaText);
// P2：进度和单/双击分流必须可在不依赖 GSMTC 的情况下回归验证。
static_assert(media::CalculateProgressFraction(0, 0, 100) == 0.0);
static_assert(media::CalculateProgressFraction(25, 0, 100) == 0.25);
static_assert(media::CalculateProgressFraction(40, 10, 130) == 0.25);
static_assert(media::CalculateProgressFraction(-1, 0, 100) == 0.0);
static_assert(media::CalculateProgressFraction(150, 0, 100) == 1.0);
static_assert(media::CalculateProgressFraction(10, 10, 10) == 0.0);

static_assert(media::ResolveClickAction(false, false) == media::MediaControlAction::None);
static_assert(media::ResolveClickAction(false, true) == media::MediaControlAction::TogglePlayPause);
static_assert(media::ResolveClickAction(true, false) == media::MediaControlAction::SkipNext);
static_assert(media::ResolveClickAction(true, true) == media::MediaControlAction::SkipNext);
static_assert(media::ShouldScheduleSingleClick(false));
static_assert(!media::ShouldScheduleSingleClick(true));
