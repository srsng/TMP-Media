#include "pch.h"
#include "OptionsLayout.h"

constexpr media::LayoutRect kLeftControl{ 17, 24, 127, 38 };
constexpr media::LayoutRect kLeftAtWideWidth = media::CalculateAnchoredControlRect(
    kLeftControl,
    330,
    430,
    0,
    media::HorizontalAnchor::Left);
static_assert(kLeftAtWideWidth.left == 17);
static_assert(kLeftAtWideWidth.right == 127);

constexpr media::LayoutRect kRightControl{ 218, 24, 318, 38 };
constexpr media::LayoutRect kRightAtWideWidth = media::CalculateAnchoredControlRect(
    kRightControl,
    330,
    430,
    0,
    media::HorizontalAnchor::Right);
static_assert(kRightAtWideWidth.left == 318);
static_assert(kRightAtWideWidth.right == 418);
static_assert(kRightAtWideWidth.Width() == kRightControl.Width());

constexpr media::LayoutRect kGroup{ 7, 7, 323, 72 };
constexpr media::LayoutRect kStretchedGroup = media::CalculateAnchoredControlRect(
    kGroup,
    330,
    430,
    0,
    media::HorizontalAnchor::Stretch);
static_assert(kStretchedGroup.left == 7);
static_assert(kStretchedGroup.right == 423);

constexpr media::LayoutRect kEdit{ 228, 51, 263, 65 };
constexpr media::LayoutRect kSpin{ 263, 51, 273, 65 };
constexpr media::LayoutRect kWideEdit = media::CalculateAnchoredControlRect(
    kEdit,
    330,
    430,
    0,
    media::HorizontalAnchor::Right);
constexpr media::LayoutRect kWideSpin = media::CalculateAnchoredControlRect(
    kSpin,
    330,
    430,
    0,
    media::HorizontalAnchor::Right);
static_assert(kWideEdit.right == kWideSpin.left);
static_assert(kWideEdit.Width() == kEdit.Width());
static_assert(kWideSpin.Width() == kSpin.Width());

constexpr media::LayoutRect kCombo{ 63, 91, 153, 105 };
constexpr media::LayoutRect kWideCombo = media::CalculateAnchoredControlRect(
    kCombo,
    330,
    530,
    0,
    media::HorizontalAnchor::Left);
static_assert(kWideCombo.Width() == kCombo.Width());

constexpr media::LayoutRect kScrolledControl = media::CalculateAnchoredControlRect(
    kCombo,
    330,
    330,
    32,
    media::HorizontalAnchor::Left);
static_assert(kScrolledControl.top == kCombo.top - 32);
static_assert(kScrolledControl.bottom == kCombo.bottom - 32);

static_assert(media::CalculateMaxScrollOffset(161, 161) == 0);
static_assert(media::CalculateMaxScrollOffset(161, 121) == 40);
static_assert(media::ClampScrollOffset(-1, 40) == 0);
static_assert(media::ClampScrollOffset(18, 40) == 18);
static_assert(media::ClampScrollOffset(90, 40) == 40);

constexpr media::LayoutRect kPartiallyVisibleControl{ 12, 100, 240, 150 };
constexpr media::LayoutRect kContentViewport{ 0, 0, 330, 120 };
constexpr media::LayoutRect kPartialClip = media::IntersectLayoutRect(
    kPartiallyVisibleControl,
    kContentViewport);
static_assert(kPartialClip.left == 12);
static_assert(kPartialClip.top == 100);
static_assert(kPartialClip.right == 240);
static_assert(kPartialClip.bottom == 120);

constexpr media::LayoutRect kHiddenClip = media::IntersectLayoutRect(
    media::LayoutRect{ 12, 130, 240, 160 },
    kContentViewport);
static_assert(kHiddenClip.Width() == 0);
static_assert(kHiddenClip.Height() == 0);
