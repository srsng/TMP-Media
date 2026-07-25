# TrafficMonitor 当前媒体插件设计文档

## 1. 背景与依据

目标是在 TrafficMonitor 的任务栏窗口中显示当前系统媒体信息，并逐步实现 P1–P4：媒体标题、进度、播放控制、多媒体会话切换和歌词显示。

主要依据：

- TrafficMonitor 插件开发指南：插件应创建为 C++ 动态库，包含 `PluginInterface.h`，导出 `TMPluginGetInstance()`，实现 `ITMPlugin` 和 `IPluginItem`，编译后复制到 TrafficMonitor `plugins` 目录加载。
- TrafficMonitor `PluginInterface.h`：当前 API 版本为 8，支持 `IPluginItem::DrawItemEx(IPluginDrawer*)`、`GetItemWidthEx(void*)`、`OnMouseEvent(...)`、`ITMPlugin::OnInitialize(ITrafficMonitor*)`、`ITrafficMonitor::GetPluginConfigDir()`、`ShowNotifyMessage()`、`GetTaskbarWindowHwnd()` 等接口。
- TrafficMonitor 插件测试器文档：PluginTester 可加载当前目录或自定义目录中的插件 DLL，预览绘制效果，并触发 `ShowOptionsDialog`、`OnMouseEvent` 等接口，适合插件调试。
- Microsoft GSMTC 文档：`GlobalSystemMediaTransportControlsSessionManager` 可获取系统媒体会话，`GetCurrentSession()`/`GetSessions()` 获取会话，`TryGetMediaPropertiesAsync()` 获取标题等媒体属性，`GetTimelineProperties()` 获取播放时间线，`TryTogglePlayPauseAsync()`/`TrySkipNextAsync()` 控制播放。

参考链接：

- <https://github.com/zhongyang219/TrafficMonitor/wiki/%E6%8F%92%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97>
- <https://raw.githubusercontent.com/zhongyang219/TrafficMonitor/master/include/PluginInterface.h>
- <https://github.com/zhongyang219/TrafficMonitorPlugins/wiki/%E6%8F%92%E4%BB%B6%E6%B5%8B%E8%AF%95%E5%99%A8>
- <https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmanager>
- <https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssession>
- <https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessiontimelineproperties>
- <https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmediaproperties>

## 2. 范围与默认决策

### 2.1 功能范围

- P1：在 TrafficMonitor 任务栏窗口显示当前播放媒体的标题。
- P1：渲染位置尽量满足“任务栏居中时靠任务栏左侧、任务栏靠左时视觉居中”的意图。
- P2：标题底部显示当前播放进度。
- P2：单击标题暂停/播放，双击标题下一首。
- P3：多个媒体会话同时存在时提供切换按钮。
- P4：支持歌词显示，首版使用“系统/播放器优先”策略。

### 2.2 非目标

- 首版不修改 TrafficMonitor 主程序源码。
- 首版不通过插件强行移动 TrafficMonitor 任务栏窗口或 Windows 任务栏窗口。
- 首版不接入联网歌词源。
- 首版不做播放器专有协议适配，例如 Spotify、网易云、QQ 音乐的私有歌词接口。

### 2.3 任务栏位置策略

插件接口能可靠控制的是“插件显示项自身绘制区域”，不能保证控制 TrafficMonitor 任务栏窗口整体在 Windows 任务栏中的绝对位置。

因此首版策略为：

- 当 Windows 11 任务栏图标居中时，建议用户在 TrafficMonitor 中启用任务栏窗口靠左显示相关设置，让插件显示项位于任务栏左侧区域。
- 当 Windows 任务栏图标靠左时，插件在自身显示项区域内将标题文本居中绘制。
- 如果未来必须实现真正的“屏幕/任务栏居中”，应作为 TrafficMonitor 主程序增强或单独窗口 Hook 方案评估，不纳入首版插件。

## 3. 用户体验设计

### 3.1 默认展示

插件只提供一个显示项：`当前媒体`。

默认单行任务栏高度下展示：

```text
[ 标题 - 艺术家                         ][切换]
──────────────────────────────────────────────
```

- 标题优先使用媒体属性 `Title`。
- 艺术家存在时格式为 `标题 - 艺术家`，可配置为只显示标题。
- 文本超出宽度时由绘制区域裁剪，Tooltip 提供完整标题、艺术家、来源应用、播放状态和进度。
- 无媒体时显示 `未播放`，也可配置为空白。

### 3.2 双行或较高显示区域

当 TrafficMonitor 任务栏窗口高度足够，或用户开启歌词显示时：

```text
[ 标题 - 艺术家                         ][切换]
[ 当前歌词或系统字幕文本                  ]
──────────────────────────────────────────────
```

- 歌词行颜色透明度低于标题行。
- 没有歌词时不显示空行，标题垂直居中。
- 进度条始终贴近显示项底部。

### 3.3 交互

- 单击标题区域：调用当前选中媒体会话的 `TryTogglePlayPauseAsync()`。
- 双击标题区域：调用当前选中媒体会话的 `TrySkipNextAsync()`。
- 单击切换按钮：在可用媒体会话中循环切换当前插件控制的会话。
- 鼠标悬停 Tooltip：显示当前会话详情，例如 `Spotify · 1/3 · 正在播放 · 01:23 / 04:15`。
- 右键：不拦截，返回 0，让 TrafficMonitor 继续显示主程序右键菜单。

### 3.4 单击与双击冲突处理

TrafficMonitor 会分别上报单击和双击事件。为避免双击时先触发暂停再下一首，插件内部使用延迟单击策略：

- 收到 `MT_LCLICKED` 后，不立即执行暂停/播放，而是记录一个待执行单击。
- 延迟 `GetDoubleClickTime() + 30ms` 后，如果期间没有收到 `MT_DBCLICKED`，再执行 `TryTogglePlayPauseAsync()`。
- 收到 `MT_DBCLICKED` 时取消待执行单击，并立即执行 `TrySkipNextAsync()`。

## 4. 技术架构

### 4.1 模块划分

```text
TrafficMonitorMediaPlugin.dll
├─ Plugin 层
│  ├─ MediaPlugin：实现 ITMPlugin，提供插件信息、初始化、数据刷新、配置入口
│  └─ MediaItem：实现 IPluginItem，提供自绘、宽度计算、鼠标事件
├─ Media 层
│  ├─ IMediaSessionService：媒体会话抽象接口
│  ├─ GsmtcMediaSessionService：GSMTC 实现
│  ├─ MediaSnapshot：UI 可读取的不可变快照
│  └─ MediaCommandQueue：异步播放控制命令队列
├─ Render 层
│  ├─ MediaItemRenderer：把 MediaSnapshot 绘制到 IPluginDrawer
│  ├─ MediaLayout：根据矩形与配置计算标题区、歌词区、进度条、按钮区
│  └─ ThemeColors：深浅色下的文字、进度、按钮颜色
├─ Lyrics 层
│  ├─ ILyricsProvider：歌词提供器接口
│  └─ SystemMetadataLyricsProvider：从系统/播放器暴露的元数据中取可用文本
├─ Config 层
│  ├─ PluginConfig：插件配置结构
│  └─ ConfigStore：读写 TrafficMonitor 插件配置目录下的 ini/json 文件
└─ Tests
   └─ 使用无外部依赖的控制台测试项目覆盖纯逻辑
```

### 4.2 数据流

```text
TrafficMonitor 定时调用 DataRequired()
        ↓
MediaPlugin 请求 MediaSessionService 刷新快照，或读取后台缓存
        ↓
MediaItem::DrawItemEx() 获取最新 MediaSnapshot
        ↓
MediaLayout 计算区域，MediaItemRenderer 绘制标题/歌词/进度/按钮
        ↓
OnMouseEvent() 根据命中测试投递播放控制或切换会话命令
```

### 4.3 线程模型

- TrafficMonitor UI/绘制线程不得执行阻塞 WinRT 调用。
- `GsmtcMediaSessionService` 在后台线程初始化 C++/WinRT apartment，并维护 GSMTC manager 与事件订阅。
- UI 线程只读取受 `std::mutex` 或 `std::shared_mutex` 保护的 `MediaSnapshot` 副本。
- 控制命令通过队列投递到后台线程执行，执行失败时缓存错误消息，下一次 UI 刷新或鼠标操作后通过 `ShowNotifyMessage()` 轻量提示。

### 4.4 核心数据结构

```cpp
enum class PlaybackState {
    Unknown,
    Closed,
    Opened,
    Changing,
    Stopped,
    Playing,
    Paused
};

struct MediaSessionSnapshot {
    std::wstring sessionId;
    std::wstring sourceAppUserModelId;
    std::wstring displaySourceName;
    std::wstring title;
    std::wstring artist;
    std::wstring albumTitle;
    std::wstring subtitle;
    std::wstring lyricLine;
    PlaybackState playbackState{PlaybackState::Unknown};
    std::chrono::milliseconds position{0};
    std::chrono::milliseconds duration{0};
    bool canTogglePlayPause{false};
    bool canSkipNext{false};
};

struct MediaSnapshot {
    std::vector<MediaSessionSnapshot> sessions;
    size_t activeIndex{0};
    std::chrono::steady_clock::time_point capturedAt{};
    std::wstring lastError;
};

struct PluginConfig {
    int minWidth96Dpi{240};
    int maxWidth96Dpi{460};
    int switchButtonWidth96Dpi{28};
    int progressHeight96Dpi{3};
    bool showArtist{true};
    bool showLyrics{true};
    bool showEmptyText{true};
    std::wstring emptyText{L"未播放"};
};
```

## 5. GSMTC 设计

### 5.1 初始化

- 使用 C++/WinRT 访问 `Windows::Media::Control`。
- 后台线程调用 `GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get()` 获取 manager。
- 初始化失败时，插件保留可加载状态，显示 `媒体不可用` 或空白，并在 Tooltip 中展示原因。

### 5.2 会话维护

- 监听 `CurrentSessionChanged` 和 `SessionsChanged`。
- 对当前 active session 监听：
  - `MediaPropertiesChanged`
  - `PlaybackInfoChanged`
  - `TimelinePropertiesChanged`
- 当会话列表变化时：
  - 优先保持用户手动选中的 `sessionId`。
  - 该会话消失时回退到系统当前会话。
  - 再无当前会话时选择 sessions[0]。

### 5.3 媒体属性

- `Title` → 标题。
- `Artist` → 艺术家。
- `AlbumTitle` → Tooltip 扩展信息。
- `Subtitle` → 歌词候选文本；只有开启“系统元数据显示为歌词”时用于歌词行。
- `SourceAppUserModelId` → 来源应用标识，显示时做简单友好化：保留最后一段或配置映射。

### 5.4 进度计算

- 从 `GetTimelineProperties()` 获取 `StartTime`、`EndTime`、`Position`、`LastUpdatedTime`。
- `duration = EndTime - StartTime`，仅当 duration > 0 时显示进度条。
- 播放状态为 Playing 时，可用 `LastUpdatedTime` 与当前时间做轻量推算；如果推算复杂或出现异常，使用最近一次 `Position`。
- 进度比例 clamp 到 `[0.0, 1.0]`。

## 6. 歌词设计

首版采用“系统/播放器优先”。这不是联网歌词功能，也不保证所有播放器都有歌词。

```cpp
class ILyricsProvider {
public:
    virtual ~ILyricsProvider() = default;
    virtual std::wstring GetCurrentLine(const MediaSessionSnapshot& session) = 0;
};
```

首个实现：`SystemMetadataLyricsProvider`。

- 输入：`MediaSessionSnapshot::subtitle` 及未来可合法获取的系统媒体扩展文本。
- 输出：当前歌词行文本。
- 默认策略：如果 `subtitle` 非空且配置允许，就显示为歌词/字幕行。
- 若 `subtitle` 为空，不显示歌词行。

后续可扩展：

- `LocalLrcLyricsProvider`：基于本地 `.lrc` 文件与标题/艺术家匹配。
- `NetworkLyricsProvider`：接入第三方歌词源，但需单独处理版权、网络失败、缓存与用户授权。

## 7. 配置设计

配置文件位置：`ITrafficMonitor::GetPluginConfigDir()` 下的 `TrafficMonitorMediaPlugin.ini`。

配置项：

```ini
[Display]
MinWidth=240
MaxWidth=460
ShowArtist=1
ShowLyrics=1
ShowEmptyText=1
EmptyText=未播放
ProgressHeight=3
SwitchButtonWidth=28

[Behavior]
SingleClickAction=TogglePlayPause
DoubleClickAction=SkipNext
RememberSelectedSession=1

[Lyrics]
UseSystemSubtitle=1
```

首版可以先实现文件配置，不强制实现设置对话框。若实现 `ShowOptionsDialog()`，只提供上述配置的简单 Win32 对话框，不引入 MFC 依赖。

## 8. 错误处理与降级

- GSMTC 初始化失败：插件仍加载，显示空状态，Tooltip 展示初始化失败原因。
- 当前播放器不暴露标题：显示来源应用名或空状态。
- 当前播放器不支持暂停/下一首：命令返回失败时不崩溃，必要时调用 `ShowNotifyMessage()`。
- 多会话列表为空：隐藏切换按钮。
- 进度无有效 duration：隐藏进度条。
- 歌词不可用：隐藏歌词行。

## 9. 验收标准

### P1

- TrafficMonitor 插件管理中能看到插件名称、版本、作者。
- 任务栏显示项可启用。
- 播放媒体时显示标题；无媒体时显示配置的空状态。
- Windows 11 任务栏居中时，配合 TrafficMonitor 设置可放到任务栏左侧区域；任务栏靠左时，文本在插件显示区域内居中。

### P2

- 有有效时长时底部显示播放进度。
- 播放/暂停/切歌后进度和状态更新。
- 单击标题触发暂停/播放。
- 双击标题触发下一首，且不会先执行单击暂停。

### P3

- 同时存在多个媒体会话时显示切换按钮。
- 点击按钮后标题、进度、Tooltip 和控制目标切换到下一会话。
- 会话消失后自动回退到有效会话。

### P4

- 开启歌词显示且播放器暴露系统字幕/文本时显示歌词行。
- 不暴露歌词时不显示无意义占位，不影响 P1–P3。

## 10. 已知限制

- 插件接口无法保证控制 TrafficMonitor 任务栏窗口整体在屏幕中的绝对位置。
- GSMTC 要求 Windows 10 1809 或更新版本；更低版本只能显示不可用状态。
- GSMTC 中 `Subtitle` 不等价于通用同步歌词，歌词效果依赖播放器实现。
- 某些播放器可能不允许外部控制暂停或下一首。
