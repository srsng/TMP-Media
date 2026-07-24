# TrafficMonitor 插件 A 方案：C++ 壳 + Rust staticlib 核心

日期：2026-07-23  
目标：在 TrafficMonitor 插件体系下，用最少 C++ 代码接入官方插件接口，把主要业务逻辑放到 Rust 中实现。  
选型：C++ shim + Rust `staticlib`，最终输出一个 TrafficMonitor 可加载的插件 DLL。

---

## 1. 方案结论

采用 A 方案：

```text
TrafficMonitor
    |
    | 加载插件 DLL
    v
TrafficMonitorRustPlugin.dll
    |
    | C++ 层：实现 TrafficMonitor 官方插件接口
    | - ITMPlugin
    | - IPluginItem
    | - TMPluginGetInstance()
    |
    | C ABI 调用
    v
Rust Core staticlib
    |
    | Rust 层：业务逻辑
    | - 数据获取
    | - 状态缓存
    | - 文本格式化
    | - 配置读取
    | - 后台刷新
```

最终分发时优先只提供一个 DLL：

```text
TrafficMonitorRustPlugin.dll
```

Rust 代码会被编译成 `.lib`，再静态链接进 C++ 插件 DLL。

---

## 2. 为什么不用纯 Rust 直接实现插件

TrafficMonitor 插件接口是 C++ 虚类接口，不是稳定的 C ABI。插件需要实现 `ITMPlugin`、`IPluginItem`，并导出 `TMPluginGetInstance()` 返回 C++ 接口对象指针。

如果纯 Rust 直接实现，需要模拟：

- C++ 对象布局；
- MSVC ABI；
- vtable 布局；
- C++ 虚函数调用规则；
- `wchar_t*` 字符串生命周期。

这些都不适合作为长期维护方案。

因此：

- C++ 负责和 TrafficMonitor 对接；
- Rust 负责插件真正的功能。

---

## 3. 项目目录建议

建议后续在 `TMP-media` 下使用如下结构：

```text
TMP-media/
├─ RUST_PLUGIN_A方案.md
├─ rust-core/
│  ├─ Cargo.toml
│  └─ src/
│     ├─ lib.rs
│     ├─ ffi.rs
│     ├─ state.rs
│     └─ config.rs
├─ cpp-shim/
│  ├─ CMakeLists.txt
│  ├─ PluginInterface.h
│  ├─ RustApi.h
│  └─ TrafficMonitorRustPlugin.cpp
├─ scripts/
│  ├─ build.ps1
│  └─ package.ps1
└─ dist/
   └─ TrafficMonitorRustPlugin.dll
```

如果要和当前 `TMP-media` 里的 TrafficMonitor 媒体插件工程融合，也可以把 Rust 核心放在：

```text
TMP-media/src/rust-core/
```

C++ 插件工程继续放在：

```text
TMP-media/src/TrafficMonitorMediaPlugin/
```

但第一版模板建议先独立使用 `cpp-shim/` + `rust-core/`，边界更清楚。

---

## 4. C++ 壳职责

C++ 层只做适配，不做复杂业务逻辑。

### 4.1 必须实现的内容

C++ 插件 DLL 负责：

1. 引入 TrafficMonitor 官方 `PluginInterface.h`。
2. 实现 `ITMPlugin`。
3. 实现一个或多个 `IPluginItem`。
4. 导出 `TMPluginGetInstance()`。
5. 在插件初始化时调用 Rust 初始化函数。
6. 在 `DataRequired()` 中调用 Rust 刷新函数。
7. 从 Rust 获取 item 文本并缓存到 `std::wstring`。
8. 在 `GetItemValueText()`、`GetItemLableText()` 等函数中返回缓存的 `const wchar_t*`。

### 4.2 C++ 文件建议

```text
cpp-shim/
├─ PluginInterface.h
├─ RustApi.h
└─ TrafficMonitorRustPlugin.cpp
```

`RustApi.h` 用来声明 Rust 暴露的 C ABI：

```cpp
#pragma once
#include <stddef.h>

extern "C" {
    int tm_rust_init(const wchar_t* config_dir);
    void tm_rust_shutdown();
    int tm_rust_item_count();
    int tm_rust_refresh();

    int tm_rust_get_plugin_info(int info_index, wchar_t* out, size_t out_len);
    int tm_rust_get_item_id(int index, wchar_t* out, size_t out_len);
    int tm_rust_get_item_name(int index, wchar_t* out, size_t out_len);
    int tm_rust_get_item_label(int index, wchar_t* out, size_t out_len);
    int tm_rust_get_item_value(int index, wchar_t* out, size_t out_len);
    int tm_rust_get_item_sample(int index, wchar_t* out, size_t out_len);
}
```

---

## 5. Rust 核心职责

Rust 层负责所有业务逻辑。

### 5.1 Rust 负责

- 初始化插件状态；
- 读取配置；
- 获取实际业务数据；
- 维护 item 列表；
- 刷新缓存；
- 格式化显示文本；
- 向 C++ 输出 UTF-16 字符串。

### 5.2 Rust 不负责

- 不直接实现 `ITMPlugin`；
- 不直接实现 `IPluginItem`；
- 不把 Rust 分配的字符串指针交给 TrafficMonitor 长期保存；
- 不让 panic 穿过 FFI 边界。

### 5.3 Cargo 配置

`rust-core/Cargo.toml`：

```toml
[package]
name = "tm_rust_core"
version = "0.1.0"
edition = "2021"

[lib]
crate-type = ["staticlib"]
```

---

## 6. C++ 与 Rust 的边界

使用 C ABI 作为唯一跨语言边界。

### 6.1 初始化

```text
C++ OnInitialize 或首次加载
    -> tm_rust_init(config_dir)
```

### 6.2 刷新

```text
TrafficMonitor 调用 DataRequired()
    -> C++ 调用 tm_rust_refresh()
    -> C++ 逐个调用 tm_rust_get_item_value()
    -> C++ 更新 std::wstring 缓存
```

### 6.3 绘制和取值

```text
TrafficMonitor 调用 GetItemValueText()
    -> C++ 返回 value_cache_.c_str()
```

`GetItemValueText()` 不直接调用耗时 Rust 逻辑。

### 6.4 卸载

```text
TrafficMonitor 卸载插件
    -> C++ 调用 tm_rust_shutdown()
```

---

## 7. 字符串传递规则

TrafficMonitor 使用 `const wchar_t*`。在 Windows MSVC 环境下，`wchar_t` 通常是 UTF-16 code unit。

推荐规则：

1. C++ 分配 `wchar_t` buffer。
2. Rust 接收 `*mut u16` 和长度。
3. Rust 把内部 `String` 通过 `encode_utf16()` 转为 UTF-16。
4. Rust 最多写入 `out_len - 1` 个 UTF-16 单元。
5. Rust 写入结尾 `0`。
6. Rust 返回状态码。
7. C++ 把 buffer 拷贝到 `std::wstring` 缓存。
8. TrafficMonitor 只拿到 C++ 缓存的 `c_str()`。

禁止：

- Rust 返回临时字符串指针；
- C++ 释放 Rust 分配的字符串；
- Rust 释放 C++ 分配的字符串；
- `GetItemValueText()` 返回局部变量地址。

---

## 8. Rust FFI 返回码

建议统一错误码：

```text
0  = 成功
-1 = 未初始化
-2 = 参数为空
-3 = index 越界
-4 = buffer 太小
-5 = 内部错误
```

C++ 收到错误时可以显示：

```text
--
```

或显示一个简短错误文本。

---

## 9. Rust 状态设计

建议 Rust 内部维护一个全局状态：

```rust
use std::sync::{Mutex, OnceLock};

static STATE: OnceLock<Mutex<PluginState>> = OnceLock::new();
```

状态结构：

```text
PluginState
  - initialized: bool
  - config_dir: Option<PathBuf>
  - items: Vec<PluginItemState>
  - last_refresh: Option<SystemTime>
  - last_error: Option<String>
```

item 结构：

```text
PluginItemState
  - id: String
  - name: String
  - label: String
  - value: String
  - sample: String
```

第一版只需要一个 item：

```text
id: rust_sample
name: Rust Sample
label: RS
value: 123
sample: 000
```

---

## 10. 构建方案

### 10.1 Rust 构建

第一版只支持 x64：

```powershell
cargo build --release --target x86_64-pc-windows-msvc
```

输出：

```text
rust-core/target/x86_64-pc-windows-msvc/release/tm_rust_core.lib
```

### 10.2 C++ 构建

使用 CMake + MSVC 构建 DLL。

CMake 负责：

1. 创建 `SHARED` 动态库；
2. 编译 `TrafficMonitorRustPlugin.cpp`；
3. include TrafficMonitor `PluginInterface.h`；
4. 链接 Rust 生成的 `tm_rust_core.lib`；
5. 输出 `TrafficMonitorRustPlugin.dll` 到 `dist/`。

### 10.3 一键脚本

`scripts/build.ps1` 执行顺序：

```text
1. 进入 rust-core
2. cargo build --release --target x86_64-pc-windows-msvc
3. 返回项目根目录
4. cmake -S cpp-shim -B build/cpp-shim -A x64
5. cmake --build build/cpp-shim --config Release
6. 复制 DLL 到 dist/
```

---

## 11. 第一版范围

第一版目标是跑通 C++ 与 Rust 的集成，不做真实业务功能。

必须包含：

1. 可被 TrafficMonitor 加载的 DLL。
2. 一个由 Rust 提供文本的 item。
3. C++ 缓存 Rust 返回的字符串。
4. x64 MSVC 构建。
5. 一键构建脚本。
6. README 安装说明。

第一版不包含：

1. 配置界面；
2. 多 item 动态增删；
3. 网络请求；
4. 后台线程；
5. 歌词、媒体控制等真实功能；
6. 32 位构建；
7. 自动安装到 TrafficMonitor 插件目录。

第一版验收显示示例：

```text
RS: 123
```

---

## 12. 与当前媒体插件方向的关系

如果最终目标是开发“当前媒体信息插件”，建议分两步走。

### 第一步：先做 Rust 集成模板

只验证：

- TrafficMonitor 能加载 DLL；
- C++ 插件壳能返回 item；
- C++ 能调用 Rust；
- Rust 能返回 UTF-16 文本；
- 多次刷新不崩溃。

### 第二步：把媒体逻辑迁移到 Rust

Rust 后续可以负责：

- 当前播放标题；
- 艺术家；
- 播放状态；
- 播放进度；
- 歌词解析；
- 配置读取。

C++ 继续只负责：

- TrafficMonitor 插件接口；
- 自绘入口；
- 鼠标事件入口；
- 调用 Rust 查询当前快照。

如果需要使用 WinRT/GSMTC，有两个可选方向：

1. 继续用 C++/WinRT 获取媒体信息，Rust 只做格式化和配置；
2. 使用 Rust 的 Windows bindings 获取媒体信息，C++ 只做 TrafficMonitor 接口。

首选方向是第 2 个，但应在最小模板成功后再做。

---

## 13. 风险与规避

| 风险 | 影响 | 规避 |
|---|---|---|
| Rust 直接实现 C++ 虚接口 | ABI 不稳定，容易崩溃 | C++ 实现 TrafficMonitor 接口 |
| 字符串生命周期错误 | 乱码或崩溃 | C++ 持有 `std::wstring` 缓存 |
| 跨语言释放内存 | 堆损坏 | C++ 分配 buffer，Rust 只写入 |
| Rust panic 穿过 FFI | 未定义行为 | FFI 函数内部 `catch_unwind` |
| `GetItemValueText()` 做耗时操作 | UI 卡顿 | 只返回缓存 |
| x86/x64 不匹配 | 插件无法加载 | 第一版固定 x64 |
| Rust staticlib 链接失败 | 构建失败 | 固定 `x86_64-pc-windows-msvc` 和 MSVC 工具链 |

---

## 14. 成功标准

完成第一版后应满足：

1. Rust `staticlib` 构建成功。
2. C++ 插件 DLL 链接成功。
3. `dist/TrafficMonitorRustPlugin.dll` 生成成功。
4. DLL 放入 TrafficMonitor 插件目录后可被识别。
5. TrafficMonitor 能显示 Rust 返回的示例文本。
6. 多次刷新不崩溃。
7. 文本无乱码。

---

## 15. 下一步实现计划

后续实现顺序建议：

```text
1. 创建 rust-core staticlib 项目
2. 实现 Rust FFI：init / shutdown / item_count / refresh / get_item_text
3. 创建 cpp-shim CMake 项目
4. 放入 PluginInterface.h
5. 实现 C++ ITMPlugin / IPluginItem
6. 链接 Rust staticlib
7. 写 build.ps1
8. 生成 DLL
9. 放到 TrafficMonitor plugins 目录手动验证
```

完成最小模板后，再决定是否把当前媒体信息、播放进度、控制按钮和歌词逻辑逐步迁移到 Rust。
