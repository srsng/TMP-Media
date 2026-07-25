#include "pch.h"
#include "MediaText.h"

// 纯文本选择逻辑不依赖 TrafficMonitor 或 GSMTC；这些编译期断言是自动回归检查。
static_assert(media::SelectDisplayText(media::MediaTitleState::Loading, L"", L"") == media::kLoadingMediaText);
static_assert(media::SelectDisplayText(media::MediaTitleState::Ready, L"Song", L"Player") == L"Song");
static_assert(media::SelectDisplayText(media::MediaTitleState::Ready, L"", L"Player") == L"Player");
static_assert(media::SelectDisplayText(media::MediaTitleState::NoSession, L"", L"") == media::kNoMediaText);
static_assert(media::SelectDisplayText(media::MediaTitleState::Error, L"", L"") == media::kUnavailableMediaText);
