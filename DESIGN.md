# TrafficMonitor 当前媒体插件设计文档

## 1. 背景与依据

目标是在 TrafficMonitor 的任务栏窗口中显示当前系统媒体信息，并逐步实现 P1–P4：媒体标题、进度、播放控制、多媒体会话切换和歌词显示。

主要依据：

- TrafficMonitor 插件开发指南：插件应创建为 C++ 动态库，包含 `PluginInterface.h`，导出 `TMPluginGetInstance()`，实现 `ITMPlugin` 和 `IPluginItem`，编译后复制到 TrafficMonitor `plugins` 目录加载。
- TrafficMonitor 官方 `PluginInterface.h`：API 版本 8 支持 `IPluginItem::DrawItemEx(IPluginDrawer*)`、`IsDoubleLineExclusive()`、`GetItemWidthEx(void*)`、`OnMouseEvent(...)` 等接口；`ITMPlugin::GetItem(index)` 明确允许同一个插件 DLL 连续返回多个 `IPluginItem` 显示项。
- 当前仓库内复制的 `include/PluginInterface.h` 仍是 API 版本 7，缺少 `DrawItemEx()` 和 `IsDoubleLineExclusive()`；实现独占双行前必须从官方 TrafficMonitor 仓库完整同步 API 8 头文件，不能手写或只拼接单个虚函数。
- TrafficMonitor 插件测试器文档：PluginTester 可加载当前目录或自定义目录中的插件 DLL，预览绘制效果，并触发 `ShowOptionsDialog`、`OnMouseEvent` 等接口，适合插件调试。
- Microsoft GSMTC 文档：`GlobalSystemMediaTransportControlsSessionManager` 可获取系统媒体会话，`GetCurrentSession()`/`GetSessions()` 获取会话，`TryGetMediaPropertiesAsync()` 获取标题等媒体属性，`GetTimelineProperties()` 获取播放时间线，`TryTogglePlayPauseAsync()`/`TrySkipPreviousAsync()`/`TrySkipNextAsync()` 控制播放。

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

- P1：在 TrafficMonitor 任务栏窗口显示当前播放媒体的标题；标题显示项始终独占任务栏双行，不与其他显示项共列。
- P1：默认第二行显示艺术家；选项中可取消，取消后标题自身最多换行两行。
- P2：标题显示项底部显示当前播放进度。
- P2：单击标题暂停/播放，双击标题下一首。
- P3：多个媒体会话同时存在时，通过任务栏标题区域的滚轮向上/向下循环切换上一/下一会话。
- P4：支持歌词显示，首版使用“系统/播放器优先”策略；歌词作为第二个可独立启用和排序的显示项，也独占一列双行。
- 延后需求（当前技术不支持）：根据 Windows 任务栏对齐方式自由控制插件整体渲染位置——任务栏图标居中时靠任务栏左侧，任务栏图标靠左时整体居中。

### 2.2 非目标

- 首版不修改 TrafficMonitor 主程序源码。
- 首版不通过插件强行移动 TrafficMonitor 任务栏窗口或 Windows 任务栏窗口。
- 首版不接入联网歌词源。
- 首版不做播放器专有协议适配，例如 Spotify、网易云、QQ 音乐的私有歌词接口。

## 3. 用户体验设计

### 3.1 显示项划分

插件按阶段提供两个相互独立的 TrafficMonitor 显示项：

| 索引 | 显示名称 | 稳定 Item ID | 布局 |
|---:|---|---|---|
| `0` | 当前媒体 | `TrafficMonitorMediaTitle` | 始终独占双行，P1/P2/P3 使用 |
| `1` | 当前歌词 | `TrafficMonitorMediaLyrics` | 始终独占双行，P4 提供 |

`ITMPlugin::GetItem(index)` 依次返回标题项和歌词项，索引 `2` 起返回 `nullptr`。TrafficMonitor 使用各自唯一的 `GetItemId()` 保存启用状态和排序，因此用户可以在主程序的显示项目设置中分别启用、禁用和移动标题列与歌词列；插件选项不重复提供显示项启用开关。

### 3.2 标题项：始终独占双行

标题项通过 API 8 的 `IPluginItem::IsDoubleLineExclusive()` 返回 `1`，让 TrafficMonitor 分配完整双行高度，并避免其他内置项或插件项占用同一列。

默认开启选项“第二行显示艺术家”：

```text
[状态] 标题
       艺术家
──────────────── 进度
```

- 第一行显示标题，第二行显示艺术家，两行分别单行省略。
- 当前媒体没有艺术家时，自动回退为标题最多换行两行，不保留空白第二行。
- 取消“第二行显示艺术家”后，标题直接占据完整文本列，自动换行且最多显示两行，超出区域后省略。
- 不提供“单双行显示”开关；标题项无论上述选项如何设置，都始终独占双行。
- 状态图标位于文本列左侧，并相对扣除进度条后的完整内容区域垂直居中。
- 进度条始终贴近标题项底部，不改变第二行文本的语义。
- 无媒体、读取中或读取失败时仍占据完整双行列，状态文本在可用文本区域内垂直居中。
- Tooltip 继续提供完整标题、艺术家、来源应用、播放状态和进度。

### 3.3 歌词项：独立一列

P4 不再把歌词塞入标题项第二行，而是新增独立的 `当前歌词` 显示项：

```text
当前歌词可在完整双行内换行
────────────────────────
```

- 歌词项也通过 `IsDoubleLineExclusive()` 独占一列双行；标题项和歌词项同时启用时共占两列，而不是共用一列。
- 歌词文本使用完整列宽和两行高度，不绘制标题状态图标或标题进度条。
- 当前歌词最多显示两行，超出后省略；无可用歌词时显示稳定的“无歌词”状态，用户也可直接在 TrafficMonitor 中禁用歌词显示项。
- 两个显示项共享同一个媒体快照、当前会话选择和歌词提供器，不各自启动后台线程。
- `ITMPlugin::GetTooltipInfo()` 是插件级接口，不是显示项级接口；标题项和歌词项无法提供不同的宿主 Tooltip，后续 Tooltip 需合并媒体与歌词信息或继续只显示媒体信息。

### 3.4 交互

- 单击标题区域：调用当前选中媒体会话的 `TryTogglePlayPauseAsync()`。
- 双击标题区域：调用当前选中媒体会话的 `TrySkipNextAsync()`。
- 多会话时滚轮向上切换上一会话、滚轮向下切换下一会话，首尾循环；单会话或无会话时不消费滚轮事件。
- 鼠标悬停 Tooltip：显示当前媒体标题和艺术家，不添加会话序号。
- 右键：不拦截，返回 0，让 TrafficMonitor 继续显示主程序右键菜单。

### 3.5 单击与双击冲突处理

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
│  ├─ MediaTitleItem：实现标题列的 IPluginItem、自绘、宽度计算和鼠标事件
│  └─ MediaLyricsItem：实现独立歌词列的 IPluginItem、自绘和宽度计算
├─ Media 层
│  ├─ IMediaSessionService：媒体会话抽象接口
│  ├─ GsmtcMediaSessionService：GSMTC 实现
│  ├─ MediaSnapshot：UI 可读取的不可变快照
│  └─ MediaCommandQueue：异步播放控制命令队列
├─ Render 层
│  ├─ MediaTitleItemRenderer：绘制双行标题/艺术家、状态图标、进度条和按钮
│  ├─ MediaLyricsItemRenderer：在独立双行列中绘制当前歌词
│  ├─ MediaLayout：根据各显示项矩形与配置计算文本、进度条和按钮区域
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
MediaTitleItem / MediaLyricsItem 获取同一份最新 MediaSnapshot
        ↓
各自布局和渲染器分别绘制标题列或歌词列
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
    bool showArtistOnSecondLine{true};
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

首版采用“系统/播放器优先”。这不是联网歌词功能，也不保证所有播放器都有歌词。歌词通过索引 `1` 的独立 `MediaLyricsItem` 暴露，由 TrafficMonitor 主程序单独控制启用和排序；标题项的第二行只用于艺术家或标题换行，不再承载歌词。

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
- 若没有可用歌词，歌词提供器返回空结果，由独立歌词项统一显示“无歌词”。

后续可扩展：

- `LocalLrcLyricsProvider`：基于本地 `.lrc` 文件与标题/艺术家匹配。
- `NetworkLyricsProvider`：接入第三方歌词源，但需单独处理版权、网络失败、缓存与用户授权。

## 7. 配置设计

配置文件位置：`ITrafficMonitor::GetPluginConfigDir()` 下的 `TrafficMonitorMedia.ini`。

配置项沿用当前实现使用的小写节名和稳定键名；歌词项是否启用由 TrafficMonitor 的显示项目设置保存，不在插件 INI 中重复设置：

```ini
[display]
show_progress=1
max_title_width=400
show_artist_on_second_line=1

[input]
left_click=toggle_play_pause
left_double_click=skip_next
right_click=none
wheel_up=none
wheel_down=none

; wheel_up / wheel_down 仅为旧配置兼容保留，不再参与事件执行

[lyrics]
use_system_subtitle=1
```

当前插件已经提供 MFC 选项窗口；“第二行显示艺术家”必须在该窗口暴露并持久化。左键单击、左键双击和右键动作可配置；滚轮固定用于多媒体会话切换，选项页不再提供滚轮动作下拉框。标题项与歌词项是否出现在任务栏中仍由 TrafficMonitor 的显示项目设置控制。

## 8. 错误处理与降级

- GSMTC 初始化失败：插件仍加载，显示空状态，Tooltip 展示初始化失败原因。
- 当前播放器不暴露标题：显示来源应用名或空状态。
- 当前播放器不支持播放/暂停、上一首或下一首：命令返回失败时不崩溃，必要时调用 `ShowNotifyMessage()`。
- 无媒体会话：显示无媒体状态，滚轮事件不由插件消费。
- 进度无有效 duration：隐藏进度条。
- 歌词不可用：独立歌词项显示“无歌词”；用户可在 TrafficMonitor 的显示项目设置中关闭该项。

## 9. 验收标准

### P1

- TrafficMonitor 插件管理中能看到插件名称、版本、作者。
- TrafficMonitor 中可独立启用标题显示项。
- 标题项始终独占完整双行，不与其他显示项共列。
- 默认第一行显示标题、第二行显示艺术家；没有艺术家时标题自动占满两行。
- 关闭“第二行显示艺术家”后，标题最多换行两行并占满文本列。
- 播放媒体时显示标题；无媒体时显示配置的空状态。

### P2

- 有有效时长时底部显示播放进度。
- 播放/暂停/切歌后进度和状态更新。
- 单击标题触发暂停/播放。
- 双击标题触发下一首，且不会先执行单击暂停。

### P3

- 同时存在多个媒体会话时，滚轮向上/向下分别切换上一/下一会话并支持首尾循环。
- 切换后标题、艺术家、播放状态、进度、Tooltip 和控制目标同步切换。
- 单会话或无会话时不消费滚轮事件；手动选择的会话消失后自动回退到有效会话。

### P4

- 插件向 TrafficMonitor 暴露独立的“当前歌词”显示项，具有唯一 Item ID，可与标题项分别启用和排序。
- 歌词项独占另一列双行；标题和歌词同时启用时不会互相挤占第二行。
- 播放器暴露系统字幕/文本时显示当前歌词，长歌词最多两行并省略。
- 不暴露歌词时显示“无歌词”，不影响 P1–P3；用户可以在 TrafficMonitor 中关闭歌词项。

## 10. 已知限制

- 插件接口无法保证控制 TrafficMonitor 任务栏窗口整体在屏幕中的绝对位置。
- 独占双行依赖 TrafficMonitor 插件 API 8；当前仓库的接口副本仍是 API 7，实施前必须完整同步官方头文件并重新验证 ABI、PluginTester 和 TrafficMonitor 加载。
- 一个插件可以提供多个显示项，但 Tooltip 由 `ITMPlugin` 统一提供，不能按标题项和歌词项分别返回。
- GSMTC 要求 Windows 10 1809 或更新版本；更低版本只能显示不可用状态。
- GSMTC 中 `Subtitle` 不等价于通用同步歌词，歌词效果依赖播放器实现。
- 某些播放器可能不允许外部控制播放/暂停、上一首或下一首。

## 11. 延后需求：任务栏渲染位置（当前技术不支持）

原始需求：

- Windows 任务栏图标居中时，将插件整体放到任务栏左侧区域。
- Windows 任务栏图标靠左时，将插件整体放到任务栏中央区域。

最新调研结论：当前 TrafficMonitor 插件 API 只向插件提供主程序已经分配的显示项矩形，插件只能在该矩形内部自绘；API 不提供 Windows 任务栏对齐状态，也不允许插件重排 TrafficMonitor 显示项或移动 TrafficMonitor 任务栏窗口。因此当前技术条件下无法可靠实现上述整体居左/居中定位。

处理决定：

- 当前版本不实现该需求，也不使用 Win32 Hook、注入或修改 TrafficMonitor 进程等非官方方案。
- 现阶段只保证标题、状态按钮和进度条在插件自身区域内正确布局。
- 等待 TrafficMonitor 未来提供官方布局/定位 API 后再重新评估。
