# TrafficMonitor 当前媒体插件 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 从零创建一个 TrafficMonitor C++ 插件 DLL，在任务栏显示当前媒体标题、进度、播放控制、多会话切换和系统/播放器优先的歌词行。

**Architecture:** 插件 DLL 实现 `ITMPlugin` 与 `IPluginItem`，UI 层通过 `IPluginDrawer` 自绘，媒体层用后台线程封装 Windows GSMTC，并向绘制层提供线程安全快照。纯逻辑拆成无 WinRT/无 TrafficMonitor 依赖的模块，便于控制台测试项目验证。

**Tech Stack:** Visual Studio 2022、C++20、Win32 DLL、C++/WinRT、Windows SDK 10.0.17763+、TrafficMonitor `PluginInterface.h`、无外部依赖控制台测试。

---

## 1. 目标目录结构

所有文件先创建在 `TMP-media` 内，不在当前目录根部直接铺文件。

```text
TMP-media/
├─ DESIGN.md
├─ PLAN.md
├─ README.md
├─ TrafficMonitorMedia.sln
├─ src/
│  ├─ TrafficMonitorMediaPlugin/
│  │  ├─ TrafficMonitorMediaPlugin.vcxproj
│  │  ├─ TrafficMonitorMediaPlugin.vcxproj.filters
│  │  ├─ dllmain.cpp
│  │  ├─ exports.cpp
│  │  ├─ resource.h
│  │  ├─ TrafficMonitorMediaPlugin.rc
│  │  ├─ include/
│  │  │  ├─ PluginInterface.h
│  │  │  ├─ MediaPlugin.h
│  │  │  ├─ MediaItem.h
│  │  │  ├─ PluginConfig.h
│  │  │  ├─ ConfigStore.h
│  │  │  ├─ MediaSnapshot.h
│  │  │  ├─ IMediaSessionService.h
│  │  │  ├─ GsmtcMediaSessionService.h
│  │  │  ├─ MediaCommandQueue.h
│  │  │  ├─ MediaLayout.h
│  │  │  ├─ MediaItemRenderer.h
│  │  │  ├─ InteractionController.h
│  │  │  ├─ ILyricsProvider.h
│  │  │  └─ SystemMetadataLyricsProvider.h
│  │  └─ src/
│  │     ├─ MediaPlugin.cpp
│  │     ├─ MediaItem.cpp
│  │     ├─ ConfigStore.cpp
│  │     ├─ GsmtcMediaSessionService.cpp
│  │     ├─ MediaCommandQueue.cpp
│  │     ├─ MediaLayout.cpp
│  │     ├─ MediaItemRenderer.cpp
│  │     ├─ InteractionController.cpp
│  │     └─ SystemMetadataLyricsProvider.cpp
│  └─ MediaPluginCoreTests/
│     ├─ MediaPluginCoreTests.vcxproj
│     ├─ MediaPluginCoreTests.vcxproj.filters
│     └─ main.cpp
└─ package/
   ├─ install.md
   └─ TrafficMonitorMediaPlugin.sample.ini
```

## 2. 任务拆分

### Task 1：创建项目工作区

**Files:**

- Create: `TMP-media/README.md`
- Create: `TMP-media/TrafficMonitorMedia.sln`
- Create: `TMP-media/src/TrafficMonitorMediaPlugin/TrafficMonitorMediaPlugin.vcxproj`
- Create: `TMP-media/src/MediaPluginCoreTests/MediaPluginCoreTests.vcxproj`

**Steps:**

- [ ] 在 `D:\projects\cplusplus` 下确认只存在 `TMP-media` 作为本插件工作目录。
- [ ] 初始化 `TMP-media` 为独立 Git 仓库：`git init`。
- [ ] 使用 Visual Studio 2022 创建空解决方案 `TrafficMonitorMedia.sln`。
- [ ] 添加 Win32 Dynamic-Link Library 项目 `TrafficMonitorMediaPlugin`，配置 Debug/Release、Win32/x64。
- [ ] 添加 Console Application 项目 `MediaPluginCoreTests`，用于无外部依赖的纯逻辑测试。
- [ ] 将两个项目都设置为 Unicode、C++20、WarningLevel4。
- [ ] 在 README 写明项目目标、构建前置条件、安装到 TrafficMonitor `plugins` 目录的方法。
- [ ] Commit：`chore: create TrafficMonitor media plugin workspace`。

**Acceptance:**

- `TMP-media` 是独立目录。
- 解决方案能打开。
- 两个空项目在 Debug/x64 下能构建。

### Task 2：引入 TrafficMonitor 插件接口与最小 DLL 导出

**Files:**

- Create: `src/TrafficMonitorMediaPlugin/include/PluginInterface.h`
- Create: `src/TrafficMonitorMediaPlugin/include/MediaPlugin.h`
- Create: `src/TrafficMonitorMediaPlugin/src/MediaPlugin.cpp`
- Create: `src/TrafficMonitorMediaPlugin/dllmain.cpp`
- Create: `src/TrafficMonitorMediaPlugin/exports.cpp`

**Steps:**

- [ ] 从 TrafficMonitor 官方仓库复制 `include/PluginInterface.h` 到项目 include 目录，保持原文件内容不改动。
- [ ] 创建 `MediaPlugin`，继承 `ITMPlugin`，先只实现必要函数：`GetItem`、`DataRequired`、`GetInfo`。
- [ ] `GetInfo` 返回固定插件信息：名称 `Current Media`，描述 `Show current media in TrafficMonitor taskbar window`，作者为项目作者，版本 `0.1.0`。
- [ ] 创建 `exports.cpp`，导出 C ABI 函数：

```cpp
#include "MediaPlugin.h"

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &MediaPlugin::Instance();
}
```

- [ ] 在 `.vcxproj` 中确保 DLL 导出符号不会被 C++ 名字修饰影响；首选 `extern "C" __declspec(dllexport)`。
- [ ] 构建 DLL 后用 `dumpbin /exports TrafficMonitorMediaPlugin.dll` 验证存在 `TMPluginGetInstance`。
- [ ] Commit：`feat: add minimal TrafficMonitor plugin export`。

**Acceptance:**

- DLL 能构建。
- 导出表包含 `TMPluginGetInstance`。
- PluginTester 能发现插件基本信息，即使还没有显示项。

### Task 3：实现插件显示项骨架

**Files:**

- Create: `include/MediaItem.h`
- Create: `src/MediaItem.cpp`
- Modify: `include/MediaPlugin.h`
- Modify: `src/MediaPlugin.cpp`

**Steps:**

- [ ] 创建 `MediaItem : public IPluginItem`。
- [ ] `GetItemName()` 返回 `L"当前媒体"`。
- [ ] `GetItemId()` 返回稳定 ID：`L"current_media"`。
- [ ] `IsCustomDraw()` 返回 `true`。
- [ ] `GetItemWidth()` 返回 240，作为 96 DPI 下最小宽度。
- [ ] `GetItemWidthEx(void* hDC)` 首版返回 240，后续由配置与文本宽度改进。
- [ ] `DrawItemEx(IPluginDrawer*, int, int, int, int, bool)` 绘制占位文本 `未播放` 并返回 `true`。
- [ ] `OnMouseEvent(...)` 首版只返回 0，不拦截右键菜单。
- [ ] `MediaPlugin::GetItem(0)` 返回 `MediaItem`，其他 index 返回 `nullptr`。
- [ ] Commit：`feat: add custom-drawn media item skeleton`。

**Acceptance:**

- PluginTester 预览区显示 `未播放`。
- 指定显示宽度、深色背景模式下无崩溃。

### Task 4：建立纯逻辑数据模型与测试

**Files:**

- Create: `include/MediaSnapshot.h`
- Create: `include/PluginConfig.h`
- Modify: `src/MediaPluginCoreTests/main.cpp`

**Steps:**

- [ ] 定义 `PlaybackState`、`MediaSessionSnapshot`、`MediaSnapshot`、`PluginConfig`。
- [ ] 给 `PluginConfig` 设置默认值：最小宽度 240、最大宽度 460、进度条高度 3、切换按钮宽度 28、显示艺术家、显示歌词、空状态文本 `未播放`。
- [ ] 在测试项目中写 `assert` 测试：默认配置值正确。
- [ ] 写 `assert` 测试：空快照时 active session 不存在。
- [ ] 写 `assert` 测试：activeIndex 超出 sessions 长度时访问函数返回空。
- [ ] 构建并运行测试项目。
- [ ] Commit：`test: add media snapshot and config model tests`。

**Acceptance:**

- `MediaPluginCoreTests.exe` 运行退出码为 0。
- 数据模型不依赖 WinRT、不依赖 TrafficMonitor 接口。

### Task 5：实现布局计算与渲染模型

**Files:**

- Create: `include/MediaLayout.h`
- Create: `src/MediaLayout.cpp`
- Create: `include/MediaItemRenderer.h`
- Create: `src/MediaItemRenderer.cpp`
- Modify: `src/MediaPluginCoreTests/main.cpp`
- Modify: `src/MediaItem.cpp`

**Steps:**

- [ ] 定义 `RectI { int x; int y; int w; int h; }`。
- [ ] 定义 `MediaLayoutResult`，包含 `titleRect`、`lyricsRect`、`progressTrackRect`、`progressFillRect`、`switchButtonRect`、`hasLyrics`、`hasSwitchButton`。
- [ ] 实现 `ComputeMediaLayout(bounds, config, snapshot)`。
- [ ] 规则：多个 session 时显示切换按钮；有歌词且高度 >= 30 时显示歌词行；进度条贴底；标题区域避开切换按钮。
- [ ] 测试：单会话无歌词时不显示切换按钮、不显示歌词行。
- [ ] 测试：三会话时显示切换按钮。
- [ ] 测试：duration 100s、position 25s 时进度填充宽度为轨道宽度 25%。
- [ ] `MediaItemRenderer` 使用 `IPluginDrawer::DrawWindowText`、`FillRect`、`DrawRectOutLine` 绘制标题、歌词、进度条和按钮。
- [ ] 将 `MediaItem::DrawItemEx` 改为调用 renderer。
- [ ] Commit：`feat: add media item layout and renderer`。

**Acceptance:**

- 纯布局测试全部通过。
- PluginTester 中占位 UI 显示进度条区域，不越界。

### Task 6：实现配置读写

**Files:**

- Create: `include/ConfigStore.h`
- Create: `src/ConfigStore.cpp`
- Create: `package/TrafficMonitorMediaPlugin.sample.ini`
- Modify: `src/MediaPlugin.cpp`
- Modify: `src/MediaPluginCoreTests/main.cpp`

**Steps:**

- [ ] `ConfigStore` 使用 Win32 `GetPrivateProfileIntW`、`GetPrivateProfileStringW`、`WritePrivateProfileStringW` 读写 ini。
- [ ] 配置路径从 `ITrafficMonitor::GetPluginConfigDir()` 获取，文件名固定为 `TrafficMonitorMediaPlugin.ini`。
- [ ] `MediaPlugin::OnInitialize(ITrafficMonitor*)` 保存主程序指针并加载配置。
- [ ] 如果配置文件不存在，写入默认配置。
- [ ] 测试：临时 ini 中 `ShowArtist=0` 能读为 false。
- [ ] 测试：缺失字段使用默认值。
- [ ] Commit：`feat: add plugin config store`。

**Acceptance:**

- 首次加载后能生成 ini。
- 修改 ini 后重启插件可生效。

### Task 7：定义媒体服务抽象与假实现

**Files:**

- Create: `include/IMediaSessionService.h`
- Create: `include/MediaCommandQueue.h`
- Create: `src/MediaCommandQueue.cpp`
- Modify: `src/MediaPlugin.cpp`
- Modify: `src/MediaPluginCoreTests/main.cpp`

**Steps:**

- [ ] 定义 `IMediaSessionService`：`Start()`、`Stop()`、`GetSnapshot()`、`SelectNextSession()`、`TogglePlayPause()`、`SkipNext()`。
- [ ] 命令函数返回 `void`，内部异步执行；失败消息写入 snapshot 的 `lastError`。
- [ ] 创建测试用 fake service，返回固定标题 `Test Song`。
- [ ] `MediaPlugin::DataRequired()` 只触发轻量刷新或读取 service 快照，不执行阻塞 WinRT。
- [ ] `MediaItem` 从 `MediaPlugin` 或注入引用中获取当前快照。
- [ ] 测试：`SelectNextSession()` 在三会话中 0→1→2→0。
- [ ] Commit：`feat: add media session service abstraction`。

**Acceptance:**

- 不引入 WinRT 的情况下，插件可用 fake snapshot 绘制标题和多会话按钮。
- 测试覆盖多会话循环。

### Task 8：实现 GSMTC 媒体服务

**Files:**

- Create: `include/GsmtcMediaSessionService.h`
- Create: `src/GsmtcMediaSessionService.cpp`
- Modify: `TrafficMonitorMediaPlugin.vcxproj`

**Steps:**

- [ ] 在项目中启用 C++/WinRT 头文件包含，链接所需 Windows SDK 库。
- [ ] 后台线程调用 `winrt::init_apartment()`。
- [ ] 调用 `GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get()` 初始化 manager。
- [ ] 读取 `GetSessions()` 和 `GetCurrentSession()`。
- [ ] 每个 session 读取：`SourceAppUserModelId()`、`TryGetMediaPropertiesAsync().get()`、`GetPlaybackInfo()`、`GetTimelineProperties()`。
- [ ] 映射媒体属性：Title、Artist、AlbumTitle、Subtitle。
- [ ] 映射时间线：StartTime、EndTime、Position、LastUpdatedTime。
- [ ] 映射控制能力：从 playback controls 中读取是否可 pause/play/toggle/skip next。
- [ ] 订阅 `CurrentSessionChanged`、`SessionsChanged`、`MediaPropertiesChanged`、`PlaybackInfoChanged`、`TimelinePropertiesChanged`，事件触发后刷新缓存快照。
- [ ] 所有 WinRT 异常捕获为 `lastError`，不抛出到 TrafficMonitor。
- [ ] Commit：`feat: integrate GSMTC media session service`。

**Acceptance:**

- 播放浏览器或音乐客户端媒体时，PluginTester/TrafficMonitor 中显示真实标题。
- 暂停、播放、切歌时标题/状态/进度会刷新。
- 无 GSMTC 权限或 API 不可用时插件不崩溃。

### Task 9：实现标题、进度与 Tooltip

**Files:**

- Modify: `src/MediaItemRenderer.cpp`
- Modify: `src/MediaItem.cpp`
- Modify: `src/MediaPlugin.cpp`

**Steps:**

- [ ] 标题格式化：`showArtist && artist 非空` 时显示 `标题 - 艺术家`，否则显示标题。
- [ ] 无标题但有来源应用时显示来源应用名。
- [ ] 无 session 时显示配置中的 `emptyText` 或空白。
- [ ] 进度条仅当 `duration > 0` 时显示。
- [ ] 播放中进度可基于 `capturedAt` 做轻量推算，并 clamp 到 0–100%。
- [ ] `ITMPlugin::GetTooltipInfo()` 返回完整媒体信息：来源、标题、艺术家、状态、进度、会话序号。
- [ ] Commit：`feat: render media title progress and tooltip`。

**Acceptance:**

- P1 标题显示完成。
- P2 进度条显示完成。
- Tooltip 可看完整信息。

### Task 10：实现鼠标交互控制

**Files:**

- Create: `include/InteractionController.h`
- Create: `src/InteractionController.cpp`
- Modify: `src/MediaItem.cpp`
- Modify: `src/MediaPluginCoreTests/main.cpp`

**Steps:**

- [ ] `InteractionController` 保存最近一次布局结果，用于命中测试。
- [ ] 标题区域命中：`MT_LCLICKED` 记录待执行单击。
- [ ] 使用 `GetDoubleClickTime() + 30ms` 延迟后执行 `TogglePlayPause()`。
- [ ] 标题区域命中：`MT_DBCLICKED` 取消待执行单击，执行 `SkipNext()`。
- [ ] 切换按钮命中：`MT_LCLICKED` 执行 `SelectNextSession()`，并返回 1 阻止主程序额外处理。
- [ ] 右键返回 0，让 TrafficMonitor 弹出原右键菜单。
- [ ] 测试：单击等待后触发 toggle。
- [ ] 测试：双击取消 toggle，只触发 skip next。
- [ ] 测试：按钮区域单击触发 switch。
- [ ] Commit：`feat: add mouse interactions for media controls`。

**Acceptance:**

- P2 单击暂停/播放完成。
- P2 双击下一首完成，且双击不误触发单击。
- P3 切换按钮基本交互完成。

### Task 11：实现多会话展示与切换细节

**Files:**

- Modify: `src/GsmtcMediaSessionService.cpp`
- Modify: `src/MediaItemRenderer.cpp`
- Modify: `src/MediaPluginCoreTests/main.cpp`

**Steps:**

- [ ] `GsmtcMediaSessionService` 为每个 session 生成稳定 `sessionId`，优先使用 `SourceAppUserModelId + title + artist` 组合。
- [ ] 保存用户手动选择的 sessionId。
- [ ] sessions 刷新后优先恢复手动选择；找不到则选择系统 current session；再找不到则选择第一个 session。
- [ ] 多会话时按钮显示 `›` 或 `2/3`，宽度使用配置 `SwitchButtonWidth`。
- [ ] Tooltip 增加 `当前会话 2/3` 和来源应用。
- [ ] 测试：选中会话消失后回退到 current session。
- [ ] 测试：current session 不在列表时回退到第一个 session。
- [ ] Commit：`feat: support multiple media sessions`。

**Acceptance:**

- P3 多媒体切换完成。
- 浏览器视频和音乐客户端同时播放/暂停时可切换显示与控制目标。

### Task 12：实现系统/播放器优先歌词行

**Files:**

- Create: `include/ILyricsProvider.h`
- Create: `include/SystemMetadataLyricsProvider.h`
- Create: `src/SystemMetadataLyricsProvider.cpp`
- Modify: `src/GsmtcMediaSessionService.cpp`
- Modify: `src/MediaItemRenderer.cpp`
- Modify: `src/MediaPluginCoreTests/main.cpp`

**Steps:**

- [ ] 定义 `ILyricsProvider::GetCurrentLine(const MediaSessionSnapshot&)`。
- [ ] `SystemMetadataLyricsProvider` 在 `UseSystemSubtitle=1` 且 `subtitle` 非空时返回 subtitle。
- [ ] 对 subtitle 做清理：去除首尾空白，替换 CR/LF 为空格，最大长度限制为 160 字符。
- [ ] `GsmtcMediaSessionService` 将 `MediaProperties.Subtitle()` 写入 `MediaSessionSnapshot::subtitle`。
- [ ] renderer 在 `config.showLyrics` 且 lyricLine 非空时绘制歌词行。
- [ ] 测试：subtitle 为空时不显示歌词行。
- [ ] 测试：subtitle 含换行时输出单行清理结果。
- [ ] Commit：`feat: show system metadata lyrics line`。

**Acceptance:**

- P4 首版完成：播放器暴露 subtitle/文本时显示歌词行。
- 播放器不暴露歌词时不显示占位，不影响标题和进度。

### Task 13：打包、安装说明与手动验证

**Files:**

- Create: `package/install.md`
- Modify: `README.md`
- Modify: `package/TrafficMonitorMediaPlugin.sample.ini`
- Modify: `TrafficMonitorMediaPlugin.rc`

**Steps:**

- [ ] 添加 DLL 版本资源：`0.1.0`。
- [ ] Release/x86 和 Release/x64 都构建通过。
- [ ] `package/install.md` 写明：选择与 TrafficMonitor 架构匹配的 DLL，复制到 TrafficMonitor 主程序目录下的 `plugins` 目录，重启 TrafficMonitor，在“插件管理”和任务栏显示项中启用。
- [ ] 文档写明 Windows 10 1809+ 要求。
- [ ] 文档写明纯插件任务栏绝对定位限制。
- [ ] 文档写明歌词依赖系统/播放器元数据。
- [ ] 手动验证 PluginTester：浅色、深色、双行、指定显示宽度、单击、双击、右键。
- [ ] 手动验证 TrafficMonitor：浏览器媒体、音乐客户端、本地播放器。
- [ ] Commit：`docs: add packaging and install instructions`。

**Acceptance:**

- 用户能按文档安装插件。
- P1–P4 的已实现范围和限制写清楚。

## 3. 测试矩阵

| 场景 | 预期 |
| --- | --- |
| 无媒体会话 | 显示 `未播放` 或空白，无进度条，无切换按钮 |
| 单个媒体有标题/艺术家 | 显示 `标题 - 艺术家` |
| 单个媒体无艺术家 | 只显示标题 |
| 媒体 duration 为 0 | 隐藏进度条 |
| 媒体 position 超过 duration | 进度 clamp 到 100% |
| 多个媒体会话 | 显示切换按钮，点击循环切换 |
| 单击标题 | 执行暂停/播放 |
| 双击标题 | 执行下一首，不执行暂停/播放 |
| 右键插件项 | TrafficMonitor 原右键菜单仍可弹出 |
| subtitle 可用 | 显示歌词/字幕行 |
| subtitle 不可用 | 不显示歌词行 |
| GSMTC 初始化失败 | 插件不崩溃，显示不可用/空状态，Tooltip 有错误信息 |

## 4. 构建与验证命令

在 Developer PowerShell for VS 中执行：

```powershell
cd D:\projects\cplusplus\TMP-media
msbuild .\TrafficMonitorMedia.sln /m /p:Configuration=Debug /p:Platform=x64
.\src\x64\Debug\MediaPluginCoreTests.exe
msbuild .\TrafficMonitorMedia.sln /m /p:Configuration=Release /p:Platform=x64
msbuild .\TrafficMonitorMedia.sln /m /p:Configuration=Release /p:Platform=Win32
```

导出验证：

```powershell
dumpbin /exports .\src\x64\Release\TrafficMonitorMediaPlugin.dll | findstr TMPluginGetInstance
```

手动验证：

```text
1. 将 Release DLL 复制到 PluginTester.exe 同目录，启动 PluginTester。
2. 在“选择插件”中选择 Current Media。
3. 勾选深色背景、双行显示、指定显示宽度，检查绘制效果。
4. 播放浏览器/音乐客户端媒体，检查标题和进度。
5. 单击标题暂停/播放，双击标题下一首。
6. 同时打开两个媒体来源，点击切换按钮检查 P3。
7. 将 DLL 复制到 TrafficMonitor/plugins，重启 TrafficMonitor 并启用显示项。
```

## 5. 实施顺序

推荐按 P1→P2→P3→P4 分阶段合并：

1. Task 1–3：项目可加载，显示占位项。
2. Task 4–9：P1/P2 的标题与进度完成。
3. Task 10：P2 的点击控制完成。
4. Task 11：P3 多会话切换完成。
5. Task 12：P4 系统/播放器优先歌词完成。
6. Task 13：发布打包与实机验证完成。

每个 Task 完成后都运行已有测试并提交一次，避免把 WinRT、绘制、交互问题混在同一个大改动里。
