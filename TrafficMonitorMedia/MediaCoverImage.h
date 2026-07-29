#pragma once

#include <cstdint>
#include <memory>

#include <windows.h>
#include <winrt/Windows.Storage.Streams.h>

namespace media
{
    struct CoverCropRect
    {
        int x{};
        int y{};
        int width{};
        int height{};

        [[nodiscard]] constexpr bool Empty() const noexcept
        {
            return width <= 0 || height <= 0;
        }
    };

    [[nodiscard]] constexpr CoverCropRect CalculateCoverCrop(
        int source_width,
        int source_height,
        int target_width,
        int target_height) noexcept
    {
        if (source_width <= 0 || source_height <= 0 || target_width <= 0 || target_height <= 0)
        {
            return {};
        }

        const std::int64_t source_scaled_height =
            static_cast<std::int64_t>(source_width) * target_height;
        const std::int64_t target_scaled_height =
            static_cast<std::int64_t>(source_height) * target_width;

        if (source_scaled_height > target_scaled_height)
        {
            const int crop_width = static_cast<int>(
                static_cast<std::int64_t>(source_height) * target_width / target_height);
            return {
                .x = (source_width - crop_width) / 2,
                .y = 0,
                .width = crop_width,
                .height = source_height,
            };
        }

        const int crop_height = static_cast<int>(
            static_cast<std::int64_t>(source_width) * target_height / target_width);
        return {
            .x = 0,
            .y = (source_height - crop_height) / 2,
            .width = source_width,
            .height = crop_height,
        };
    }

    struct CoverFitRect
    {
        int x{};
        int y{};
        int width{};
        int height{};

        [[nodiscard]] constexpr bool Empty() const noexcept
        {
            return width <= 0 || height <= 0;
        }
    };

    [[nodiscard]] constexpr CoverFitRect CalculateCoverFitRect(
        int source_width,
        int source_height,
        int target_width,
        int target_height) noexcept
    {
        if (source_width <= 0 || source_height <= 0 || target_width <= 0 || target_height <= 0)
        {
            return {};
        }

        const std::int64_t scaled_width =
            static_cast<std::int64_t>(source_width) * target_height;
        const std::int64_t scaled_height =
            static_cast<std::int64_t>(source_height) * target_width;

        if (scaled_width > scaled_height)
        {
            const int height = static_cast<int>(
                static_cast<std::int64_t>(source_height) * target_width / source_width);
            return {
                .x = 0,
                .y = (target_height - height) / 2,
                .width = target_width,
                .height = height > 0 ? height : 1,
            };
        }

        const int width = static_cast<int>(
            static_cast<std::int64_t>(source_width) * target_height / source_height);
        return {
            .x = (target_width - width) / 2,
            .y = 0,
            .width = width > 0 ? width : 1,
            .height = target_height,
        };
    }

    class MediaCoverImage final
    {
    public:
        MediaCoverImage(HBITMAP bitmap, int width, int height) noexcept;
        ~MediaCoverImage();

        MediaCoverImage(const MediaCoverImage&) = delete;
        MediaCoverImage& operator=(const MediaCoverImage&) = delete;

        [[nodiscard]] HBITMAP Bitmap() const noexcept { return m_bitmap; }
        [[nodiscard]] int Width() const noexcept { return m_width; }
        [[nodiscard]] int Height() const noexcept { return m_height; }

    private:
        HBITMAP m_bitmap{};
        int m_width{};
        int m_height{};
    };

    [[nodiscard]] std::shared_ptr<const MediaCoverImage> DecodeMediaCover(
        const winrt::Windows::Storage::Streams::IRandomAccessStreamReference& reference) noexcept;
}