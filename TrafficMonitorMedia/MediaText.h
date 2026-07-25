#pragma once

#include <string_view>

namespace media
{
    enum class MediaTitleState
    {
        Loading,
        NoSession,
        Ready,
        Error,
    };

    inline constexpr std::wstring_view kLoadingMediaText{ L"正在获取媒体…" };
    inline constexpr std::wstring_view kNoMediaText{ L"未检测到媒体" };
    inline constexpr std::wstring_view kUnavailableMediaText{ L"媒体不可用" };

    [[nodiscard]] constexpr std::wstring_view SelectDisplayText(
        MediaTitleState state,
        std::wstring_view title,
        std::wstring_view source_app_id) noexcept
    {
        if (!title.empty())
        {
            return title;
        }
        if (!source_app_id.empty())
        {
            return source_app_id;
        }

        switch (state)
        {
        case MediaTitleState::Loading:
            return kLoadingMediaText;
        case MediaTitleState::Error:
            return kUnavailableMediaText;
        case MediaTitleState::NoSession:
        case MediaTitleState::Ready:
        default:
            return kNoMediaText;
        }
    }
}
