# 插件选项与自由输入映射实施计划

> **执行要求：** 实施时必须使用 `superpowers:executing-plans` 按任务顺序执行；每个任务完成后先自动验证，整个阶段完成后由用户使用 PluginTester / TrafficMonitor 人工验收。实施期间不进入 P3 多媒体会话切换。

**目标：** 修复 TrafficMonitorMedia 的插件选项窗口和中文乱码，为已经实现的显示能力提供持久化配置，把所有 TrafficMonitor 鼠标触发器改为可独立、可重复组合的动作映射，并在任务栏标题左侧显示高质量媒体状态图标。

**架构：** 使用独立的 `MediaSettings.h` 定义设置、动作枚举、默认值、合法化和 INI 序列化名称；MFC 选项对话框只编辑设置副本，用户确认且内容确实变化后才应用并保存。插件主体通过互斥锁发布和读取完整设置快照，避免绘制、鼠标回调与选项更新并发访问同一对象。任务栏项目负责绘制离线生成的 MDI 状态 ICO，并把 TrafficMonitor 鼠标事件转换为配置动作；`CMediaSessionService` 继续负责点击仲裁和在 MTA 工作线程执行 GSMTC 查询与控制。

**技术栈：** C++17、MFC、Win32 INI API、C++/WinRT GSMTC、TrafficMonitor `ITMPlugin` / `IPluginItem` API、MSBuild v145。

---

## 一、已确认的规范和设计边界

### 1. TrafficMonitor 选项接口

- 保留并实现 `ITMPlugin::ShowOptionsDialog(void* hParent)`。
- 使用 TrafficMonitor 传入的 `hParent` 创建模态 MFC 对话框。
- 用户取消，或按“确定”但设置未发生变化时返回 `OR_OPTION_UNCHANGED`。
- 只有设置实际变化并成功应用时返回 `OR_OPTION_CHANGED`。
- 不返回 `OR_OPTION_NOT_PROVIDED`，因为本插件明确提供选项窗口。
- 配置目录继续通过 `OnInitialize(ITrafficMonitor*)` 中的 `GetPluginConfigDir()` 获取，不重复引入旧式 `EI_CONFIG_DIR` 初始化路径。

### 2. 首版显示设置

仅配置已经实现的显示能力：

| 设置 | 类型 | 默认值 | 合法范围/行为 |
|---|---|---:|---|
| 显示播放进度条 | 布尔值 | 开启 | 关闭后不绘制进度背景、进度前景和顶部间距，标题使用完整高度 |
| 标题最大宽度 | 整数 | 400 | 96 DPI 逻辑像素，读取时限制到 `100..1000`，绘制时按当前 DPI 缩放 |

固定保留：

- 最小宽度为 100 个 96 DPI 逻辑像素；暂不暴露为选项。
- 标题文本始终显示；不增加“隐藏标题”这种会使插件失去主要用途的设置。
- 不增加尚不存在的颜色、字体、标题格式和歌词设置。
### 3. 任务栏媒体状态图标

状态图标只显示在任务栏插件区域的标题左侧；hover 提示继续保持“标题 + 可选艺术家/错误详情”的纯文本格式，不增加图标或“当前媒体：”前缀。

图标来自 Iconify 的 Material Design Icons（MDI），作者 Pictogrammers，许可证 Apache-2.0：

| 状态 | MDI 图标 |
|---|---|
| 正在播放（按钮动作：暂停） | `mdi:pause` |
| 已暂停或停止（按钮动作：播放） | `mdi:play` |
| 没有媒体、正在读取或读取失败 | `mdi:music-off` |

资源策略：

- 在 `assets/mdi/` 保存原始 SVG，作为图标来源和可追溯素材。
- 使用 `tools/Convert-MdiIcons.ps1` 调用 Windows 自带 WPF `Geometry.Parse` 和 `RenderTargetBitmap`，离线生成多尺寸透明 ICO。
- 生成深色任务栏使用的浅色图标和浅色任务栏使用的深色图标；运行时不解析 SVG、不依赖 WebView、Direct2D SVG 或第三方图像库。
- 构建工程直接嵌入已生成 ICO；普通构建不自动执行转换脚本，避免增加本地构建依赖和修改生成物。
- 运行时按 `dark_mode` 选择浅色或深色资源，通过 `DrawIconEx` 绘制。
- 图标尺寸以 16 个 96 DPI 逻辑像素为基准，按 DPI 缩放；图标与标题之间保留 4 个逻辑像素间距。
- `GetItemWidthEx()` 将图标宽度和间距计入所需宽度，标题最大宽度只限制文本部分。
- 图标资源由插件缓存，避免每次重绘重新加载。

### 4. 自由输入映射

TrafficMonitor 当前 `MouseEventType` 暴露的五种触发器全部进入配置：

1. 左键单击 `MT_LCLICKED`
2. 右键单击 `MT_RCLICKED`
3. 左键双击 `MT_DBCLICKED`
4. 滚轮向上 `MT_WHEEL_UP`
5. 滚轮向下 `MT_WHEEL_DOWN`

每个触发器都有独立下拉框，首版可映射到：

- 无操作
- 播放/暂停
- 上一首
- 下一首

组合规则：

- 不做互斥限制；多个触发器可以映射到同一个动作。
- 任意触发器都可以设为“无操作”。
- 不固定“单击只能播放/暂停”或“双击只能下一首”。
- 不提前增加“切换媒体会话”“歌词”等尚未实现的动作。
- 以后新增动作时只扩展统一动作描述表和执行分发，不重做对话框结构。

默认映射保持 P2 的现有行为：

| 触发器 | 默认动作 |
|---|---|
| 左键单击 | 播放/暂停 |
| 左键双击 | 下一首 |
| 右键单击 | 无操作 |
| 滚轮向上 | 无操作 |
| 滚轮向下 | 无操作 |

事件处理约束：

- 配置只作用于任务栏窗口；没有 `MF_TASKBAR_WND` 时返回 0。
- 右键映射为“无操作”时返回 0，保留 TrafficMonitor 默认右键菜单。
- 右键映射了动作时返回非 0，动作由插件处理，不再弹出主程序右键菜单。
- 滚轮每收到一次事件执行一次对应动作。
- 左键单击继续延迟到系统双击判定时间结束后再执行。
- 双击优先于单击：双击发生后取消待处理单击，并抑制双击后的尾随单击。
- 即使双击映射为“无操作”，双击也应取消待处理的单击；这样“单击=播放/暂停、双击=无操作”时，真正的双击不会误触发单击动作。

### 5. 配置文件格式

继续使用：

```text
<插件配置目录>\TrafficMonitorMedia.ini
```

固定键名：

```ini
[display]
show_progress=1
max_title_width=400

[input]
left_click=toggle_play_pause
left_double_click=skip_next
right_click=none
wheel_up=none
wheel_down=none
```

持久化规则：

- 动作用稳定字符串保存，不直接保存枚举整数。
- 可识别值为 `none`、`toggle_play_pause`、`skip_next`。
- 配置文件不存在时使用默认值。
- 单个键缺失或值非法时，只对该键使用默认值，不影响其他合法设置。
- 最大宽度读取后统一限制到 `100..1000`。
- 用户取消选项窗口时不修改内存设置，也不写配置文件。

---

## 二、文件职责与变更范围

### 新建

- `TrafficMonitorMedia/MediaSettings.h`
  - 定义 `MediaControlAction`、输入映射结构和完整 `SettingData`。
  - 定义默认值、宽度限制、设置比较、动作合法化、动作配置字符串转换。

- `TrafficMonitorMedia/MediaSettingsTests.cpp`
  - 放置不依赖 MFC/WinRT 的设置模型编译期断言。

- `assets/mdi/*.svg`
  - 保存用户提供的 `play`、`pause`、`music-off` 三个 Iconify MDI 原始 SVG。

- `tools/Convert-MdiIcons.ps1`
  - 使用 Windows WPF 将 MDI path 离线渲染为深浅两套多尺寸透明 ICO。

- `TrafficMonitorMedia/res/status-*.ico`
  - 插件运行时直接加载的生成图标资源。

### 修改

- `TrafficMonitorMedia/MediaText.h`
  - 移除其中重复的 `MediaControlAction` 定义，改为包含 `MediaSettings.h`。
  - 定义媒体播放状态到任务栏图标状态的纯逻辑映射。
  - 把点击仲裁纯函数改为接受任意单击动作和双击动作，而不是写死播放/暂停与下一首。

- `TrafficMonitorMedia/TrafficMonitorMedia.h`
  - 使用新的 `SettingData`，并将设置成员改为私有。
  - 增加设置互斥锁和按值返回的 `GetSettingsSnapshot()`。
  - 提供统一动作分发接口，将固定的 `RequestTogglePlayPause()` / `RequestSkipNext()` 门面改为通用输入请求门面。

- `TrafficMonitorMedia/TrafficMonitorMedia.cpp`
  - 实现 INI 读取与保存。
  - 正确比较对话框前后的设置。
  - 只在设置变化时保存并返回 `OR_OPTION_CHANGED`。
  - 提供显示设置和触发器映射给 `TrafficMonitorMediaItem`。

- `TrafficMonitorMedia/OptionsDlg.h`
  - 增加进度条复选框、最大宽度编辑框/微调框、五个动作下拉框成员。
  - 增加动作选项填充和选中值读写辅助方法。

- `TrafficMonitorMedia/OptionsDlg.cpp`
  - 在 `OnInitDialog()` 中加载设置副本。
  - 用下拉框 ItemData 保存 `MediaControlAction`，不依赖显示文本反向解析。
  - 在 `OnOK()` 中验证宽度并写回 `m_data`。

- `TrafficMonitorMedia/TrafficMonitorMediaItem.cpp`
  - 在标题左侧按媒体状态和深浅模式绘制图标。
  - 用设置中的最大宽度替换固定 400。
  - 根据设置决定是否绘制进度条。
  - 将五种鼠标事件分别查询映射并提交对应动作。
  - 实现右键菜单保留规则以及单击/双击消费规则。

- `TrafficMonitorMedia/MediaSessionService.h`
  - 在媒体快照中增加播放状态，供任务栏选择状态图标。
  - 增加通用 `RequestImmediateAction(action)`、`RequestSingleClick(action)`、`RequestDoubleClick(action)`。
  - 待处理单击同时保存截止时间和动作，不再默认它一定是播放/暂停。

- `TrafficMonitorMedia/MediaSessionService.cpp`
  - 通用化工作队列和单/双击仲裁。
  - 双击动作可以是“无操作”，但仍取消待处理单击并启用尾随单击抑制。
  - 所有非空动作仍只在 MTA 工作线程执行。

- `TrafficMonitorMedia/TrafficMonitorMedia.rc`
  - 重建已经损坏的简体中文选项资源文本。
  - 增加“显示”和“鼠标操作”分组及全部控件。
  - 同步维护简体中文和英文资源。
  - 保持简体中文资源段 `#pragma code_page(65001)`。
  - 删除无定义且无用途的 `IDD_SELECT_CITY_DIALOG AFX_DIALOG_LAYOUT` 模板残留。

- `TrafficMonitorMedia/resource.h`
  - 增加控件 ID、动作字符串资源 ID和状态图标资源 ID。
  - 修正 `_APS_NEXT_*` 值，避免以后资源编辑器生成冲突 ID。

- `TrafficMonitorMedia/TrafficMonitorMedia.vcxproj`
- `TrafficMonitorMedia/TrafficMonitorMedia.vcxproj.filters`
  - 把 `MediaSettings.h` 加入 Visual Studio 工程和筛选器。

- `PLAN.md`
  - 把本阶段登记为 Phase 5.5，并在人工验收后更新完成状态。

### 明确不修改

- `include/PluginInterface.h`
- `.git*`
- `justfile`
- P3/P4 的实现代码
- TrafficMonitorPlugins 参考仓库

---

## 三、实施任务

### Task 1：建立设置和动作模型

**文件：**

- 新建：`TrafficMonitorMedia/MediaSettings.h`
- 新建：`TrafficMonitorMedia/MediaSettingsTests.cpp`
- 修改：`TrafficMonitorMedia/MediaText.h`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.h`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.vcxproj`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.vcxproj.filters`

- [ ] 定义稳定的动作枚举：`None`、`TogglePlayPause`、`SkipNext`。
- [ ] 定义 `InputBindings`，包含 `left_click`、`left_double_click`、`right_click`、`wheel_up`、`wheel_down`。
- [ ] 定义 `SettingData`，包含 `show_progress`、`max_title_width` 和 `InputBindings`。
- [ ] 提供默认设置，确保默认行为与当前 P2 完全一致。
- [ ] 提供 `NormalizeSettings()`，限制宽度并修复非法动作值。
- [ ] 提供 `operator==` / `operator!=`，用于准确判断选项是否变化。
- [ ] 提供 `constexpr std::wstring_view ToConfigValue(MediaControlAction)` 和 `constexpr MediaControlAction ParseConfigValue(std::wstring_view, MediaControlAction fallback)`。
- [ ] 在 `MediaSettingsTests.cpp` 增加编译期断言，至少覆盖：默认映射、重复动作允许、非法宽度上下界、动作字符串往返和非法动作回退。
- [ ] 把动作枚举从 `MediaText.h` 移到设置模型，避免配置层和执行层重复定义。
- [ ] 在工程和筛选器中登记 `MediaSettings.h` 与 `MediaSettingsTests.cpp`。

验证：

```powershell
just build Debug x64
```

预期：Debug x64 编译成功，不改变当前运行时行为。

提交检查点：

```text
feat: add media plugin settings model
```

### Task 2：导入并生成 MDI 状态图标

**文件：**

- 新建：`assets/mdi/play.svg`
- 新建：`assets/mdi/pause.svg`
- 新建：`assets/mdi/music-off.svg`
- 新建：`tools/Convert-MdiIcons.ps1`
- 新建：`TrafficMonitorMedia/res/status-*.ico`
- 修改：`TrafficMonitorMedia/resource.h`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.rc`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.vcxproj`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.vcxproj.filters`

- [ ] 从用户提供的 `D:\projects\rust\qt-demo\.ignored\tmp-icons` 复制 `mdi--play.svg`、`mdi--pause.svg`、`mdi--music-off.svg`；不复制 `skip-next` 和 `skip-previous`。
- [ ] 转换脚本只读取受控 SVG path，使用 WPF 在 `16/20/24/32/48` 像素画布居中渲染。
- [ ] 分别用 `RGB(245,245,245)` 和 `RGB(32,32,32)` 生成深色/浅色任务栏资源。
- [ ] 每个 ICO 包含五个 PNG 帧并保留透明背景。
- [ ] 在 `.rc` 和工程文件登记所有生成 ICO；普通构建不调用转换脚本。
- [ ] 增加 Apache-2.0 来源说明，记录 MDI 作者、图标名和 Iconify 来源。

验证：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\Convert-MdiIcons.ps1
just build Debug x64
```

预期：六个 ICO（3 种状态 × 深浅两套）可重复生成，Debug x64 资源编译成功。

提交检查点：

```text
feat: add mdi media status icons
```

### Task 3：实现可靠的 INI 持久化

**文件：**

- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.cpp`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.h`

- [ ] 将持久化接口确定为 `void LoadConfig(const std::wstring& config_dir)` 和 `void SaveConfig(const SettingData& settings) const`；保存函数接收不可变快照，不在写文件期间持有设置锁。
- [ ] `LoadConfig()` 先保存完整配置路径，再从 `[display]` 和 `[input]` 读取所有固定键。
- [ ] 文件不存在、键缺失、动作字符串未知时按字段回退到默认值。
- [ ] 对读取结果调用 `NormalizeSettings()`。
- [ ] `SaveConfig()` 使用 `WritePrivateProfileStringW()` 写入稳定字符串动作名。
- [ ] 布尔值只写 `0/1`，宽度写十进制字符串。
- [ ] 配置路径为空时不写文件，避免插件初始化前或异常卸载时写入错误位置。
- [ ] 保留析构阶段保存行为，但改为 `SaveConfig(GetSettingsSnapshot())`；先停止媒体工作线程，再获取设置快照并保存，确保接口签名一致且不直接访问共享设置成员。

验证：

```powershell
just build Debug x64
```

预期：编译成功；尚未操作选项时插件行为维持默认值。

提交检查点：

```text
feat: persist media plugin settings
```

### Task 4：修复并重建本地化选项窗口

**文件：**

- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.rc`
- 修改：`TrafficMonitorMedia/resource.h`
- 修改：`TrafficMonitorMedia/OptionsDlg.h`
- 修改：`TrafficMonitorMedia/OptionsDlg.cpp`

- [ ] 将损坏的中文标题、字体和按钮文本重新写为有效 UTF-8，例如“媒体插件设置”“微软雅黑”“确定”“取消”。
- [ ] 清理资源文件中已有的替换字符和无效城市对话框布局残留。
- [ ] 建立“显示”分组：进度条复选框、最大标题宽度编辑框和微调框。
- [ ] 建立“鼠标操作”分组：五个触发器标签和五个动作下拉框。
- [ ] 增加提示文字：右键为“无操作”时保留 TrafficMonitor 默认菜单。
- [ ] 为中文、英文资源提供完全对应的布局、控件和动作字符串。
- [ ] 使用固定控件 ID：`IDC_SHOW_PROGRESS_CHECK`、`IDC_MAX_TITLE_WIDTH_EDIT`、`IDC_MAX_TITLE_WIDTH_SPIN`、`IDC_LEFT_CLICK_ACTION_COMBO`、`IDC_LEFT_DOUBLE_CLICK_ACTION_COMBO`、`IDC_RIGHT_CLICK_ACTION_COMBO`、`IDC_WHEEL_UP_ACTION_COMBO`、`IDC_WHEEL_DOWN_ACTION_COMBO`。
- [ ] 使用字符串资源 ID：`IDS_ACTION_NONE`、`IDS_ACTION_TOGGLE_PLAY_PAUSE`、`IDS_ACTION_SKIP_NEXT`，确保动作列表文本跟随 TrafficMonitor 当前 UI 语言。
- [ ] 微调框范围设为 `100..1000`，编辑框输入非法时阻止关闭并由 MFC 显示验证提示。
- [ ] 下拉框使用 ItemData 绑定动作枚举；允许五个下拉框选择相同动作。
- [ ] `OnInitDialog()` 只读取 `m_data`；`OnOK()` 验证后才写回 `m_data`。

自动验证：

```powershell
rg -n "�|����|ȷ|ȡ|IDD_SELECT_CITY_DIALOG" TrafficMonitorMedia\TrafficMonitorMedia.rc
just build Debug x64
```

预期：第一次命令无损坏资源匹配；Debug x64 编译成功。

人工检查点：

- 中文 PluginTester 打开选项窗口时，标题、字体、按钮、分组和下拉框均无乱码。
- 英文模式下显示对应英文文本。
- 五个下拉框可以选择相同动作。

此任务不单独提交；与 Task 4 的选项应用语义一起形成一个可用提交。

### Task 5：正确应用选项并报告变化

**文件：**

- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.cpp`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.h`

- [ ] 增加 `SettingData GetSettingsSnapshot() const`，在短临界区内复制完整设置后返回。
- [ ] 增加 `void PublishSettings(const SettingData& settings)`，在短临界区内一次性替换完整设置。
- [ ] 打开对话框前通过快照接口复制当前设置到局部旧值和对话框数据。
- [ ] 用户取消时不修改当前设置、不保存文件，返回 `OR_OPTION_UNCHANGED`。
- [ ] 用户确认后先合法化对话框数据，再与旧值比较。
- [ ] 数据相同则不写文件，返回 `OR_OPTION_UNCHANGED`。
- [ ] 数据不同则先发布完整新快照，再调用 `SaveConfig(new_settings)`，并返回 `OR_OPTION_CHANGED`。
- [ ] 设置对象按值提供给绘制和事件处理代码，避免对话框内部对象生命周期泄漏到插件主体。
- [ ] 绘制和事件回调每次只获取一次设置快照，保证同一次回调看到的字段彼此一致。

验证：

```powershell
just build Debug x64
```

人工检查点：

- 不修改任何内容直接“确定”，主程序收到未变化结果。
- 修改后“取消”，显示和鼠标行为不变。
- 修改后“确定”，立即生效；重启 PluginTester 后仍保留。

提交检查点：

```text
fix: rebuild localized plugin options dialog
```

### Task 6：把状态图标和显示设置接入自绘

**文件：**

- 修改：`TrafficMonitorMedia/TrafficMonitorMediaItem.cpp`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.h`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.cpp`

- [ ] GSMTC 刷新时读取 `PlaybackInfo().PlaybackStatus()`：Playing 使用暂停按钮图标，Paused/Stopped/Closed 使用播放按钮图标；没有会话、读取中、未知和错误统一使用无媒体图标。
- [ ] `GetItemWidthEx()` 使用设置中的 `max_title_width`，按 DPI 缩放后参与 `std::clamp()`，并额外计入图标宽度和图标间距。
- [ ] 固定最小宽度继续为 100 个 96 DPI 逻辑像素。
- [ ] `DrawItem()` 根据媒体标题状态和播放状态选择深色/浅色 ICO，在标题左侧按 DPI 垂直居中绘制。
- [ ] `DrawItem()` 只有在“显示播放进度条”开启且时间线有效时才预留进度高度并绘制。
- [ ] 关闭进度条后标题矩形使用整个项目高度，不保留空白间距。
- [ ] 图标与文本布局不得覆盖进度条；长标题继续使用省略号。
- [ ] hover 提示实现保持不变，不添加状态图标、Emoji 或“当前媒体：”前缀。
- [ ] 设置改变后无需重启媒体服务；下次宽度查询和绘制立即读取新值。

自动验证：

```powershell
just build Debug x64
```

人工检查点：

- 关闭/开启进度条后显示立即变化。
- 改小最大宽度后长标题更早省略；改大后可显示更多内容。
- 不同 DPI 下宽度仍按比例缩放。

提交检查点：

```text
feat: make media display configurable
```

### Task 7：通用化媒体动作请求和单/双击仲裁

**文件：**

- 修改：`TrafficMonitorMedia/MediaText.h`
- 修改：`TrafficMonitorMedia/MediaSessionService.h`
- 修改：`TrafficMonitorMedia/MediaSessionService.cpp`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.h`
- 修改：`TrafficMonitorMedia/TrafficMonitorMedia.cpp`

- [ ] 将服务接口改为 `void RequestImmediateAction(media::MediaControlAction action)`、`void RequestSingleClick(media::MediaControlAction action)`、`void RequestDoubleClick(media::MediaControlAction action)`。
- [ ] 定义 `PendingSingleClick { MediaControlAction action; steady_clock::time_point deadline; }`，并以 `std::optional<PendingSingleClick>` 保存待处理单击，不再写死为播放/暂停。
- [ ] `RequestImmediateAction(None)` 不入队；其他动作立即唤醒工作线程。
- [ ] `RequestSingleClick(None)` 不建立待处理动作。
- [ ] `RequestDoubleClick(action)` 总是取消待处理单击并开启尾随单击抑制；只有非空动作才进入执行队列。
- [ ] 工作循环优先处理已经排队的即时/双击动作，然后处理到期单击。
- [ ] `ExecuteControlAction()` 继续只在 MTA 线程调用 GSMTC；播放/暂停、上一首、下一首均通过统一动作分发执行。
- [ ] 将 `ResolveClickAction()` 的输入改为“是否有双击 + 双击动作 + 是否有到期单击 + 单击动作”，更新 `MediaTextTests.cpp` 的编译期断言，覆盖：单击映射下一首、双击映射播放/暂停、双击无操作优先于并取消单击、尾随单击抑制。

验证：

```powershell
just build Debug x64
```

预期：编译通过，无 WinRT 调用移动到 TrafficMonitor UI 线程。

### Task 8：接入五种自由鼠标映射

**文件：**

- 修改：`TrafficMonitorMedia/TrafficMonitorMediaItem.cpp`

- [ ] 在 `CTrafficMonitorMedia` 增加三个门面：`RequestImmediateAction(action)`、`RequestSingleClick(action)`、`RequestDoubleClick(action)`，仅向服务转发请求。
- [ ] 对五种 `MouseEventType` 分别读取同一份设置快照中的对应配置动作。
- [ ] 左键单击调用通用延迟单击请求。
- [ ] 左键双击调用通用双击请求，即使其动作是“无操作”也执行取消单击仲裁。
- [ ] 右键和滚轮使用即时动作请求。
- [ ] 非任务栏窗口始终返回 0。
- [ ] 右键无操作返回 0；右键有动作返回 1。
- [ ] 其他事件有映射动作时返回 1；无动作时返回 0。
- [ ] 双击动作为空但单击动作非空时返回 1，因为插件必须消费双击并阻止单击动作落地。

自动验证：

```powershell
just rebuild Debug x64
just check Release x64
git diff --check
```

预期：

- Debug 全量重建成功。
- Release DLL 构建成功并导出 `TMPluginGetInstance`。
- 无空白错误。

人工组合测试至少覆盖：

1. 默认映射：单击播放/暂停，双击下一首。
2. 交换映射：单击下一首，双击播放/暂停。
3. 重复映射：单击和滚轮向上都设为播放/暂停。
4. 双击无操作：单击播放/暂停、双击无操作；双击时不得触发播放/暂停。
5. 右键无操作：正常打开 TrafficMonitor 菜单。
6. 右键上一首：执行上一首且不打开 TrafficMonitor 菜单。
7. 右键下一首：执行下一首且不打开 TrafficMonitor 菜单。
8. 滚轮向上/向下分别映射上一首和下一首。
9. 所有触发器无操作：插件不提交任何媒体控制请求。
10. 重启 PluginTester：全部映射从 INI 正确恢复。

提交检查点：

```text
feat: add configurable media input bindings
```

### Task 9：阶段收尾

**文件：**

- 修改：`PLAN.md`

- [ ] 记录自动验证结果和用户人工验收结果。
- [ ] 确认工作区只包含本阶段计划内文件。
- [ ] 确认没有修改 `include/PluginInterface.h`、`.git*`、`justfile` 或参考仓库。
- [ ] 用户人工验收通过后提交阶段文档更新。
- [ ] 本阶段提交完成后才开始重新讨论并规划 P3。

最终检查：

```powershell
git status --short
git diff --check
git log -6 --oneline
```

---

## 四、验收定义

本阶段只有同时满足以下条件才算完成：

- 中文选项窗口完全无乱码，英文资源也能正常显示。
- 任务栏标题左侧使用用户提供的三个 MDI 图标表达可执行按钮动作：播放中显示暂停按钮、暂停/停止时显示播放按钮，无可用媒体显示禁用媒体图标；读取中、未知和错误归入无可用媒体图标，深浅任务栏均清晰。
- hover 提示仍为纯文本，不出现状态图标或额外前缀。
- 显示进度条和标题最大宽度可以配置、立即生效并持久化。
- 五种鼠标触发器均可独立选择“无操作 / 播放或暂停 / 上一首 / 下一首”。
- 映射允许重复，没有互斥或固定动作限制。
- 默认配置保持已通过人工验证的 P2 行为。
- 双击优先级和尾随单击抑制在任意动作映射下仍正确。
- 右键无操作时不破坏 TrafficMonitor 默认菜单。
- Debug/Release x64 自动构建和 DLL 导出检查通过。
- PluginTester / TrafficMonitor 交互由用户人工验收通过。
