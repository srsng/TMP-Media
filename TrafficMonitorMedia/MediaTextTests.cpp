#include "pch.h"
#include "MediaText.h"
#include "MediaSessionSelection.h"
#include "MediaCardInteractionState.h"
#include "MediaCardWindowBehavior.h"
#include "MediaCardVisuals.h"

#include <array>

// 纯文本选择逻辑不依赖 TrafficMonitor 或 GSMTC；这些编译期断言是自动回归检查。

// 卡片参与正常激活生命周期；仅动画帧禁止重复激活。
static_assert((media::kMediaCardWindowExtendedStyle & WS_EX_NOACTIVATE) == 0);
static_assert(media::kMediaCardShowCommand == SW_SHOW);
static_assert((media::kMediaCardAnimationPositionFlags & SWP_NOACTIVATE) != 0);
static_assert((media::kMediaCardAnimationPositionFlags & SWP_NOOWNERZORDER) != 0);

constexpr media::MediaCardActivationLifecycle kInactiveBeforeActivation{
    false, false, media::MediaCardLifecyclePhase::Opening };
static_assert(!kInactiveBeforeActivation.ShouldPostDeactivateDismiss(
    media::MediaCardActivationEvent::Inactive));

constexpr media::MediaCardActivationLifecycle kActiveOpeningCard{
    true, false, media::MediaCardLifecyclePhase::Opening };
static_assert(kActiveOpeningCard.ShouldMarkActivated(media::MediaCardActivationEvent::Active));
static_assert(kActiveOpeningCard.ShouldMarkActivated(media::MediaCardActivationEvent::ClickActive));
static_assert(kActiveOpeningCard.ShouldPostDeactivateDismiss(
    media::MediaCardActivationEvent::Inactive));
static_assert(kActiveOpeningCard.ShouldDismissAfterDeferredDeactivate(true, false));

constexpr media::MediaCardActivationLifecycle kDismissAlreadyPosted{
    true, true, media::MediaCardLifecyclePhase::Open };
static_assert(!kDismissAlreadyPosted.ShouldPostDeactivateDismiss(
    media::MediaCardActivationEvent::Inactive));

constexpr media::MediaCardActivationLifecycle kReactivatedCard{
    true, false, media::MediaCardLifecyclePhase::Open };
static_assert(!kReactivatedCard.ShouldDismissAfterDeferredDeactivate(true, true));

constexpr media::MediaCardActivationLifecycle kClosingCardActivation{
    true, false, media::MediaCardLifecyclePhase::Closing };
static_assert(!kClosingCardActivation.ShouldMarkActivated(
    media::MediaCardActivationEvent::Active));
static_assert(!kClosingCardActivation.ShouldPostDeactivateDismiss(
    media::MediaCardActivationEvent::Inactive));
static_assert(!kClosingCardActivation.ShouldDismissAfterDeferredDeactivate(true, false));

constexpr media::MediaCardActivationLifecycle kClosedCardActivation{
    true, false, media::MediaCardLifecyclePhase::Closed };
static_assert(!kClosedCardActivation.ShouldPostDeactivateDismiss(
    media::MediaCardActivationEvent::Inactive));
static_assert(!kClosedCardActivation.ShouldDismissAfterDeferredDeactivate(false, false));

// 仅任务栏根窗口可作为媒体卡片 owner，避免绑定到宿主内部或临时窗口。
static_assert(media::IsTrustedMediaCardOwnerClass(L"Shell_TrayWnd"));
static_assert(media::IsTrustedMediaCardOwnerClass(L"Shell_SecondaryTrayWnd"));
static_assert(!media::IsTrustedMediaCardOwnerClass(L"TaskbarCreated"));
static_assert(!media::IsTrustedMediaCardOwnerClass(L"TrafficMonitor"));

// 私有延迟消息必须属于当前窗口代际，避免旧 HWND 消息影响新卡片。
static_assert(media::IsCurrentMediaCardMessageGeneration(7, 7));
static_assert(!media::IsCurrentMediaCardMessageGeneration(6, 7));

// 收到激活消息还不够；卡片必须实际成为前台窗口才算激活验证成功。
static_assert(!media::IsMediaCardActivationVerified(false, false));
static_assert(!media::IsMediaCardActivationVerified(true, false));
static_assert(media::IsMediaCardActivationVerified(true, true));

// 入场动画与前台激活都完成后，窗口状态才允许进入 Open。
static_assert(!media::ShouldCompleteMediaCardOpening(false, false));
static_assert(!media::ShouldCompleteMediaCardOpening(false, true));
static_assert(!media::ShouldCompleteMediaCardOpening(true, false));
static_assert(media::ShouldCompleteMediaCardOpening(true, true));

// 私有失活消息投递失败时必须安排异步备用关闭，而不是遗留失活卡片。
static_assert(!media::ShouldScheduleMediaCardDeactivateFallback(true));
static_assert(media::ShouldScheduleMediaCardDeactivateFallback(false));

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
static_assert(media::ClampTimelinePosition(5, 10, 20) == 10);
static_assert(media::ClampTimelinePosition(25, 10, 20) == 20);
static_assert(media::ClampTimelinePosition(15, 10, 20) == 15);
static_assert(media::PositionFromProgressFraction(0.25, 10, 50) == 20);
static_assert(media::PositionFromProgressFraction(-1.0, 10, 50) == 10);
static_assert(media::PositionFromProgressFraction(2.0, 10, 50) == 50);
constexpr media::PopupPlacement kPopupAboveAnchor = media::CalculatePopupPlacement(
    900, 1040, 380, 236, 8, media::PixelRect{ 0, 0, 1920, 1080 });
static_assert(kPopupAboveAnchor.x == 900);
static_assert(kPopupAboveAnchor.y == 796);
constexpr media::PopupPlacement kPopupClampedToWorkArea = media::CalculatePopupPlacement(
    1880, 10, 380, 236, 8, media::PixelRect{ 0, 0, 1920, 1080 });
static_assert(kPopupClampedToWorkArea.x == 1540);
static_assert(kPopupClampedToWorkArea.y == 18);

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
constexpr media::DoubleClickDispatch kOpenMediaCardDoubleClick =
    media::ResolveDoubleClickDispatch(MediaControlAction::OpenMediaCard);
static_assert(kOpenMediaCardDoubleClick.media_service_action == MediaControlAction::OpenMediaCard);
static_assert(kOpenMediaCardDoubleClick.open_media_card);
constexpr media::DoubleClickDispatch kPlaybackDoubleClick =
    media::ResolveDoubleClickDispatch(MediaControlAction::TogglePlayPause);
static_assert(kPlaybackDoubleClick.media_service_action == MediaControlAction::TogglePlayPause);
static_assert(!kPlaybackDoubleClick.open_media_card);
constexpr media::MediaCardIconRect kSmallControlIcon = media::CalculateMediaCardIconRect(
    media::MediaCardPixelRect{ 100, 50, 130, 80 },
    18,
    18,
    0,
    0);
static_assert(kSmallControlIcon.left == 106);
static_assert(kSmallControlIcon.top == 56);
static_assert(kSmallControlIcon.right == 124);
static_assert(kSmallControlIcon.bottom == 74);

constexpr media::MediaCardIconRect kPlayIconWithOpticalOffset = media::CalculateMediaCardIconRect(
    media::MediaCardPixelRect{ 200, 100, 238, 138 },
    20,
    20,
    1,
    0);
static_assert(kPlayIconWithOpticalOffset.left == 210);
static_assert(kPlayIconWithOpticalOffset.top == 109);
static_assert(kPlayIconWithOpticalOffset.right == 230);
static_assert(kPlayIconWithOpticalOffset.bottom == 129);

constexpr media::MediaCardIconRect kHeaderChevronIcon = media::CalculateMediaCardIconRect(
    media::MediaCardPixelRect{ 300, 40, 326, 66 },
    16,
    16,
    0,
    0);
static_assert(kHeaderChevronIcon.left == 305);
static_assert(kHeaderChevronIcon.top == 45);
static_assert(kHeaderChevronIcon.right == 321);
static_assert(kHeaderChevronIcon.bottom == 61);

static_assert(!media::ShouldScheduleMediaCardOpen(100, 101));
static_assert(media::ShouldScheduleMediaCardOpen(101, 101));
static_assert(media::CalculateMediaCardOpenSuppressionDeadline(100, 500) == 600);
static_assert(media::CalculateMediaCardSingleClickConfirmationDelay(500, false) == 0);
static_assert(media::CalculateMediaCardSingleClickConfirmationDelay(200, true) == 200);
static_assert(media::CalculateMediaCardSingleClickConfirmationDelay(300, true) == 300);
static_assert(media::CalculateMediaCardSingleClickConfirmationDelay(500, true) == 300);

constexpr media::MediaCardAnimationFrame kOpeningStart =
    media::CalculateMediaCardAnimationFrame(
        media::MediaCardAnimationPhase::Opening, 0.0, 8, 5);
static_assert(kOpeningStart.alpha == 0);
static_assert(kOpeningStart.offset_y == 8);
static_assert(media::kMediaCardOpeningVerticalDirectionY == 1);
static_assert(media::kMediaCardClosingVerticalDirectionY == 1);

constexpr media::MediaCardAnimationFrame kOpeningEnd =
    media::CalculateMediaCardAnimationFrame(
        media::MediaCardAnimationPhase::Opening, 1.0, 8, 5);
static_assert(kOpeningEnd.alpha == 255);
static_assert(kOpeningEnd.offset_y == 0);

constexpr media::MediaCardAnimationFrame kClosingStart =
    media::CalculateMediaCardAnimationFrame(
        media::MediaCardAnimationPhase::Closing, 0.0, 8, 5);
static_assert(kClosingStart.alpha == 255);
static_assert(kClosingStart.offset_y == 0);

constexpr media::MediaCardAnimationFrame kClosingEnd =
    media::CalculateMediaCardAnimationFrame(
        media::MediaCardAnimationPhase::Closing, 1.0, 8, 5);
static_assert(kClosingEnd.alpha == 0);
static_assert(kClosingEnd.offset_y == 5);

constexpr media::MediaCardAnimationFrame kClampedOpening =
    media::CalculateMediaCardAnimationFrame(
        media::MediaCardAnimationPhase::Opening, 2.0, 8, 5);
static_assert(kClampedOpening.alpha == 255);
static_assert(kClampedOpening.offset_y == 0);

constexpr media::MediaCardLifecycleState kVisibleCardLifecycle{ false, true };
static_assert(kVisibleCardLifecycle.ShouldHandleUnexpectedCardDestroy());
constexpr media::MediaCardLifecycleState kClosingCardLifecycle{ true, true };
static_assert(!kClosingCardLifecycle.ShouldHandleUnexpectedCardDestroy());

constexpr media::SeekRequest kSeekForSession22{ 22, 45'000'000 };
static_assert(media::IsSeekRequestForSession(kSeekForSession22, 22));
static_assert(!media::IsSeekRequestForSession(kSeekForSession22, 33));
static_assert(!media::IsSeekRequestForSession(
    media::SeekRequest{ media::kNoSessionIdentity, 45'000'000 },
    media::kNoSessionIdentity));

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
