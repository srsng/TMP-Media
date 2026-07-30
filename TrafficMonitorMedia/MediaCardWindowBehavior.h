#pragma once

#include <windows.h>

namespace media
{
    inline constexpr DWORD kMediaCardWindowExtendedStyle =
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED;
    inline constexpr int kMediaCardShowCommand = SW_SHOW;
    // 正 y 轴方向为屏幕下方：卡片从目标位置下方上移进入，并向下淡出关闭。
    inline constexpr int kMediaCardOpeningVerticalDirectionY = 1;
    inline constexpr int kMediaCardClosingVerticalDirectionY = 1;
    inline constexpr UINT kMediaCardAnimationPositionFlags =
        SWP_NOACTIVATE | SWP_NOOWNERZORDER;
}
