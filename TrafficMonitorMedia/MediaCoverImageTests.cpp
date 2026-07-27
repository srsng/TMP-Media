#include "pch.h"
#include "MediaCoverImage.h"

constexpr media::CoverCropRect kSquareCrop = media::CalculateCoverCrop(400, 400, 100, 50);
static_assert(kSquareCrop.x == 0);
static_assert(kSquareCrop.y == 100);
static_assert(kSquareCrop.width == 400);
static_assert(kSquareCrop.height == 200);

constexpr media::CoverCropRect kWideCrop = media::CalculateCoverCrop(600, 300, 100, 100);
static_assert(kWideCrop.x == 150);
static_assert(kWideCrop.y == 0);
static_assert(kWideCrop.width == 300);
static_assert(kWideCrop.height == 300);

constexpr media::CoverCropRect kTallCrop = media::CalculateCoverCrop(300, 600, 200, 100);
static_assert(kTallCrop.x == 0);
static_assert(kTallCrop.y == 225);
static_assert(kTallCrop.width == 300);
static_assert(kTallCrop.height == 150);

constexpr media::CoverCropRect kInvalidCrop = media::CalculateCoverCrop(0, 600, 200, 100);
static_assert(kInvalidCrop.Empty());