# TMP-media 重启开发计划

## 当前状态

- 初始化基线已完成：官方 `PluginTemplate` 已机械重命名为 `TrafficMonitorMedia`，官方 `PluginInterface.h` 已放入 `include/`。
- `TrafficMonitorMedia.sln` 由 `dotnet sln` 创建，工程可在 Visual Studio 中打开；工程本体来自官方模板，不是手写项目文件。
- 当前处于 Phase 3（P1 当前媒体标题）开始前；尚未接入媒体逻辑。

## 硬性规则

1. 不再手写 `.sln` / `.vcxproj` / `PluginInterface.h`。
2. 文档页面可通过 MCP 获取；允许在 `D:\projects\cplusplus` 下将官方或参考 Git 仓库克隆为 `TMP-media` 的同级目录。
3. 不通过 PowerShell、curl、wget 抓取单个远程文件；源码优先从同级 Git 克隆仓库复制。PowerShell 只用于本地文件处理、构建与检查。
4. 清理无用文件时只删除明确列出的生成物，不碰 `.git*`、文档、repo 配置。
5. 工具链、模板、构建步骤有问题时先停下说明，不继续乱造文件。
6. 每个阶段完成后先汇报目录状态和验证结果，再进入下一阶段。

## 实施状态（2026-07-24）

### 已确认环境（直接复用，不再重复检查）

- Visual Studio Community：`D:\dev_tools\Microsoft Visual Studio\18\Community`
- MSBuild：`D:\dev_tools\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`
- 当前构建验证改用已安装的 v145 工具集；这是用户明确批准的兼容性调整。
- 后续 GSMTC / C++/WinRT 使用 C++17 或更高，并链接 `windowsapp.lib`。

### 测试职责边界

- 自动执行：工程编译、DLL 产物检查、`TMPluginGetInstance` 导出检查、可拆分纯逻辑测试。
- 人工执行：TrafficMonitor 插件加载、任务栏显示、点击/双击、媒体切换、歌词和实际任务栏布局测试。
- 自动化阶段不会启动或修改用户的 TrafficMonitor 安装。

### 当前执行记录

- [x] 清理错误工程，仅保留文档与仓库配置。
- [x] 明确目标目录结构、测试职责和已确认工具链。
- [x] 已在工作区同级克隆 `TrafficMonitorPlugins` 官方仓库（提交 `eb7fc9e56f93fb69c99b185c6b8c395153d78bc6`）。
- [x] 已复制官方 `PluginTemplate` 与 `PluginInterface.h`，并机械重命名为 `TrafficMonitorMedia`。
- [x] 已由 dotnet 工具创建 `TrafficMonitorMedia.sln`，并在设置 `VCTargetsPath` 后加入 C++ 工程。
- [x] 已使用 v145 成功构建 `Release|x64`，并确认 DLL 与 `TMPluginGetInstance` 导出。
- [x] 已添加 `.gitignore` 与本地 `justfile`，固化构建、导出验证和受限清理命令；`justfile` 含本机 VS 路径并已被 Git 忽略。
- [x] Phase 3：P1 当前媒体标题已完成自动验证和 PluginTester 人工加载/显示验证。

### 构建验证记录（2026-07-24）

- 工程使用当前安装的 v145 工具集；仅将官方模板迁移工程中的 6 个 `<PlatformToolset>` 值由 `v143` 改为 `v145`，未修改模板功能代码。
- 初次 v145 构建的 MSB6001 并非工程或 MFC 问题：当前 Codex 进程环境同时注入了大小写不同的 PATH 与 Path，MSBuild 启动 CL.exe 时在 Windows 不区分大小写的环境变量表中冲突。
- 本地忽略的 `justfile` 以多行 PowerShell 脚本直接调用 `MSBuild.exe`；在脚本进程内归一化 `PATH`/`Path`，构建日志能正常显示中文。
- `just build` 已成功，退出码为 `0`；`just verify` 已确认 DLL 导出 `TMPluginGetInstance`。
- `just rebuild` 已在仅清理 `TrafficMonitorMedia/bin`、`TrafficMonitorMedia/TrafficM.1C2173CA` 与 `.vs` 后成功，日志显示 `All 434 functions were compiled`。因此此前 `0 of 434 functions were compiled` 为增量链接复用缓存的正常提示，不代表 DLL 未编译。
- 产物：`TrafficMonitorMedia\\bin\\x64\\Release\\TrafficMonitorMedia.dll`。
- 编译有 C4819 编码警告（模板的 UTF-8 中文注释被当前代码页 936 编译）；不影响本次 DLL 构建与导出验证。尚未改动编码设置，后续清理警告前先单独确认。

### Phase 3 自动验证记录（2026-07-25）

- 新增 `CMediaSessionService`：在独立 MTA 工作线程中请求 GSMTC manager，每秒轮询或响应 `DataRequired()` 的刷新请求；UI / 绘制线程只读取互斥保护的媒体快照。
- 当前会话有标题时显示标题；标题为空时回退到来源应用 ID；无会话时显示“未检测到媒体”；媒体 API 失败时显示“媒体不可用”，错误详情进入 Tooltip。
- `CTrafficMonitorMediaItem` 采用单行自绘，垂直居中、超长标题省略；宽度按标题文本计算并限制在 100–400 个 96 DPI 像素之间。
- `MediaTextTests.cpp` 的 `static_assert` 已作为同一插件工程的编译单元参与构建，覆盖加载中、标题优先、来源应用回退、无会话、错误等纯逻辑。
- `just rebuild Debug x64` 与 `just check Release x64` 已成功；两个配置的 DLL 均已验证导出 `TMPluginGetInstance`。
- 已人工验证：PluginTester 可加载 `bin\x64\Release\TrafficMonitorMedia.dll`；播放媒体时标题正常显示；显示项名称“媒体标题”仅作为 PluginTester 的调试辅助信息，不属于任务栏自绘内容。
- 修复资源乱码：`TrafficMonitorMedia.rc` 为 UTF-8，中文资源段必须使用 `#pragma code_page(65001)`，不能使用 GBK 的 `936`；已从构建出的 DLL 读取并确认插件名、描述、显示项名称均为正常中文。
- 实际任务栏整体位置由 TrafficMonitor 主程序管理，插件接口无法移动其整体区域。

### 当前执行顺序

1. 在 `D:\projects\cplusplus` 下克隆 `TrafficMonitorPlugins` 官方仓库，并从同级仓库取得 `PluginTemplate` 和 `PluginInterface.h` 原文件。
2. 将官方模板完整落盘，再机械重命名为 `TrafficMonitorMedia`；不凭空编写工程文件。
3. 先编译原模板语义的最小插件并检查 DLL 导出。
4. 再按 P1 → P2 → P3 → P4 逐阶段实现；每阶段先完成本地验证，再交给人工测试。

## 目标

开发 TrafficMonitor 插件 `TrafficMonitorMedia`：

### P1

- 在任务栏显示当前播放媒体的标题。
- 支持根据任务栏对齐方式决定显示位置：
  - 任务栏图标居中时，插件区域靠任务栏左侧渲染。
  - 任务栏图标靠左时，插件区域居中渲染。

### P2

- 在标题底部显示当前播放进度。
- 单击标题：播放 / 暂停。
- 双击标题：下一首。

### P3

- 多个媒体会话同时存在时，提供按钮切换媒体轨道 / 会话。

### P4

- 支持显示歌词。

## 正确项目结构目标

参考 `TMP-WeatherPro` 的独立插件仓库风格：仓库根就是插件根。

计划结构：

```text
TMP-media/
├─ .git/
├─ .gitattributes
├─ DESIGN.md
├─ PLAN.md
├─ RUST_PLUGIN_A方案.md
├─ README.md                         # 后续创建，说明插件用途和测试方式
├─ TrafficMonitorMedia.sln            # 必须由 VS/官方模板流程生成或从官方模板可靠改名
├─ TrafficMonitorMedia/                # 单插件工程目录
│  ├─ TrafficMonitorMedia.vcxproj      # 不手写，必须来自 VS/官方模板流程
│  ├─ TrafficMonitorMedia.vcxproj.filters
│  ├─ pch.h / pch.cpp
│  ├─ framework.h
│  ├─ resource.h
│  ├─ TrafficMonitorMedia.rc
│  ├─ TrafficMonitorMedia.cpp/.h
│  ├─ TrafficMonitorMediaItem.cpp/.h
│  └─ res/
└─ include/
   └─ PluginInterface.h                # 必须来自官方 TrafficMonitorPlugins
```

第一阶段只做单插件工程。后续如果代码复杂，再拆 core/test 工程。

## 阶段计划

### Phase 0：资料获取与确认

只用 MCP 做以下事情：

1. 获取 TrafficMonitor 插件开发指南。
2. 获取官方 `TrafficMonitorPlugins` 的模板文件清单。
3. 获取官方 `include/PluginInterface.h`。
4. 获取 `TMP-WeatherPro` 顶层结构和工程结构作为参考。
5. 整理出：
   - 插件必须导出的函数。
   - `ITMPlugin` / `IPluginItem` 最小实现要求。
   - 官方推荐的本地测试方式。
   - 模板工程需要保留和重命名的文件。

交付物：只更新文档，不创建工程。

### Phase 1：初始化空工程

前置条件：Phase 0 完成，并确认工具链可用。

初始化方式优先级：

1. 优先：使用 Visual Studio / 官方模板生成 MFC DLL 插件工程。
2. 次选：从官方 `PluginTemplate` 完整复制模板文件后做机械重命名。
3. 禁止：凭空手写 `.sln` / `.vcxproj` / 接口头。

验收：

- 目录结构看起来像独立 VS C++ 插件仓库。
- `.sln` 能被 VS 打开。
- 工程文件来源明确。
- 未实现媒体逻辑前，空插件能编译出 DLL。

### Phase 2：最小插件可加载

实现最小 TrafficMonitor 插件：

- 导出 `TMPluginGetInstance`。
- 实现 `ITMPlugin`。
- 实现一个 `IPluginItem`。
- 先显示固定文本，例如 `未检测到媒体`。
- 自绘区域只做标题文本，不做复杂布局。

验收：

- 编译通过。
- DLL 能放入 TrafficMonitor 插件目录。
- TrafficMonitor 能识别插件和显示项目。

### Phase 3：P1 当前媒体标题

接入 Windows Global System Media Transport Controls Session Manager，读取当前媒体会话：

- 标题。
- 艺术家 / 应用名可作为调试信息。
- 播放状态。

实现策略：

- 后台线程或异步刷新媒体快照。
- `DataRequired()` 只读取缓存，不直接阻塞查询系统媒体 API。
- 绘制层只使用线程安全快照。

验收：

- 播放媒体时标题能更新。
- 无媒体时显示占位文本。
- 不导致 TrafficMonitor 卡顿。

### Phase 4：P1 布局策略（已核查）

结论：当前 `PluginInterface.h` **不提供**任务栏窗口整体坐标、Windows 任务栏对齐方式或移动窗口的接口。

- `IPluginItem` 仅提供自身最小宽度（`GetItemWidth` / `GetItemWidthEx`）、自绘区域（`DrawItem`）和鼠标事件（`OnMouseEvent`）。
- `DrawItem` 收到的 `x/y/w/h` 是主程序已分配的插件区域；插件可以在其中绘制，但不能重排 TrafficMonitor 的显示项目或移动任务栏窗口。
- `MF_TASKBAR_WND` / `KF_TASKBAR_WND` 只表明事件来自任务栏窗口，不携带 Windows 任务栏的靠左/居中对齐信息。
- 已检查官方参考插件；未发现位置或对齐控制扩展接口。

处理决定：不使用 Win32 hook 或修改 TrafficMonitor 进程。P1 的布局能力限定为：在主程序分配给本插件的区域内按内容自适应宽度、单行垂直居中并省略超长标题。用户提出的整体左侧/居中定位需要 TrafficMonitor 主程序未来提供官方 API 后才可实现。

### Phase 5：P2 进度与点击控制（已完成）

已实现：

- 后台线程读取 GSMTC `TimelineProperties`，按开始时间、当前位置和结束时间计算 `[0, 1]` 播放进度。
- 标题底部绘制 2 个 96 DPI 像素高的进度条；无有效时间线时不绘制。
- 任务栏插件区域单击：延迟到系统双击时间结束后，在工作线程调用 `TryTogglePlayPauseAsync()`。
- 任务栏插件区域双击：取消待处理的单击，在工作线程调用 `TrySkipNextAsync()`；同时抑制双击后的第二次抬键事件，避免额外触发播放/暂停。
- TrafficMonitor 绘制/鼠标线程只提交请求；所有 GSMTC 查询和控制操作仍在 MTA 工作线程执行。
- 纯逻辑编译期断言覆盖进度边界、无效时间线、进度裁剪、单击到期和双击优先级。

自动验证（2026-07-25）：

- `just rebuild Debug x64` 成功。
- `just check Release x64` 成功；Release DLL 完整编译 1215 个函数并导出 `TMPluginGetInstance`。
- `git diff --check` 通过。

人工验收（2026-07-25）：

- 进度条显示与推进正常。
- 单击播放/暂停正常。
- 双击下一首正常，未额外触发播放/暂停。

### Phase 5.5：插件选项与自由输入映射（已规划）

在进入 P3 前先完成插件选项基础设施：

- 修复简体中文选项资源乱码并同步英文资源。
- 提供“显示播放进度条”和“标题最大宽度”设置。
- 在任务栏标题左侧显示用户提供的 MDI 播放、暂停、无媒体图标；SVG 离线转换为 ICO，hover 提示保持纯文本。
- 将左键单击、左键双击、右键单击、滚轮向上、滚轮向下分别映射到“无操作 / 播放或暂停 / 下一首”。
- 所有触发器独立配置、允许重复动作；默认映射保持现有 P2 行为。
- 通过 `TrafficMonitorMedia.ini` 持久化设置。
- 正确返回 `OR_OPTION_CHANGED` / `OR_OPTION_UNCHANGED`。

详细设计、实施顺序和验收项见 `OPTIONS_PLAN.md`。

### Phase 6：P3 多媒体会话切换

实现：

- 枚举所有 GSMTC 媒体会话。
- 在插件自绘区域增加切换按钮。
- 点击切换当前显示 / 控制的媒体会话。

验收：

- 多播放器同时存在时可切换。
- 当前会话关闭后自动降级到有效会话。

### Phase 7：P4 歌词

先确认可行来源：

1. 系统媒体元数据是否包含歌词。
2. 常见播放器是否通过标题/元数据暴露歌词。
3. 是否需要外部歌词源或本地歌词文件。

默认不引入联网歌词 API，除非单独确认。

验收：

- 能显示当前歌词行或明确显示“无歌词”。
- 长歌词不破坏任务栏布局。

## 测试计划

### 编译测试

- `Release|x64`
- `Debug|x64`
- 如 TrafficMonitor 插件要求 32 位，再增加 `Win32`。

### 本地加载测试

根据官方插件开发指南确认具体方式后执行，候选方式：

1. 将 DLL 放入 TrafficMonitor 插件目录。
2. 启动或重启 TrafficMonitor。
3. 在插件管理 / 显示项目中启用插件。
4. 观察任务栏窗口显示和鼠标事件。

### 功能测试

- 无播放器。
- 单播放器播放。
- 单播放器暂停。
- 多播放器同时存在。
- 标题很长。
- 深色 / 浅色任务栏。
- 不同 DPI。

## 下一步

1. 用户审阅并确认 `OPTIONS_PLAN.md` 中的显示设置、五种触发器和三种已有动作范围。
2. 确认后按 Phase 5.5 的 Task 1 到 Task 8 顺序实施，每个提交检查点执行自动构建验证。
3. 完成后由用户使用 PluginTester / TrafficMonitor 人工验收，再提交 Phase 5.5 状态并进入 P3。
