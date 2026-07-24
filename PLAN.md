# TMP-media 重启开发计划

## 当前状态

- 已清理上一轮错误生成的工程文件和目录。
- 当前仓库只保留：
  - `.git/`
  - `.gitattributes`
  - `DESIGN.md`
  - `PLAN.md`
  - `RUST_PLUGIN_A方案.md`
- 当前还没有有效的 Visual Studio 解决方案、工程文件或插件源码。

## 硬性规则

1. 不再手写 `.sln` / `.vcxproj` / `PluginInterface.h`。
2. 远程资料只通过 MCP 获取：
   - TrafficMonitor 插件开发 Wiki
   - TrafficMonitorPlugins 官方模板
   - `include/PluginInterface.h`
   - `TMP-WeatherPro` 参考结构
3. PowerShell 只用于本地文件检查、清理、复制、构建，不用于联网抓资料。
4. 清理无用文件时只删除明确列出的生成物，不碰 `.git*`、文档、repo 配置。
5. 工具链、模板、构建步骤有问题时先停下说明，不继续乱造文件。
6. 每个阶段完成后先汇报目录状态和验证结果，再进入下一阶段。

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

### Phase 4：P1 布局策略

研究 TrafficMonitor 插件接口能否控制插件整体位置。

风险点：

- 插件接口可能只允许控制显示项目宽度和自绘内容，未必能决定任务栏窗口整体位置。
- 如果 TrafficMonitor 主程序没有暴露任务栏对齐信息或位置控制 API，插件只能在自身区域内绘制，不能移动到任务栏左侧 / 居中。

处理方式：

1. 先确认官方接口能力。
2. 如果接口不支持，记录限制，不瞎写 Win32 hook。
3. 只有在明确可行时再设计窗口定位方案。

### Phase 5：P2 进度与点击控制

实现：

- 底部播放进度条。
- 单击：调用当前会话播放 / 暂停。
- 双击：下一首。

验收：

- 点击只响应任务栏插件区域。
- 单击和双击不互相误触。
- 媒体会话失效时不崩溃。

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

下一步只做 Phase 0：用 MCP 补齐官方文档、模板和参考仓库信息，然后汇报，不创建工程文件。
