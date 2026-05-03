# Veloxa

> **轻量级 HTML/CSS 渲染引擎，面向车载 / 嵌入式场景**

[![ctest DEVTOOL=ON](https://img.shields.io/badge/ctest_DEVTOOL%3DON-1252_PASS-brightgreen)](#测试与基准)
[![ctest DEVTOOL=OFF](https://img.shields.io/badge/ctest_DEVTOOL%3DOFF-1087_PASS-brightgreen)](#测试与基准)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-064F8C)](https://cmake.org/)
[![Version](https://img.shields.io/badge/version-0.1.0-orange)](veloxa/api/veloxa_api.h)

Veloxa 是一个为嵌入式 / 车载 HMI / IoT 显示设备打造的浏览器级渲染引擎。它实现了**子集 HTML5** + **CSS 引擎（~45 属性）** + **Block / Inline / Flex 布局** + **软件渲染器** + **W3C DOM 事件** + **CSS Transitions** + **QuickJS 脚本** + **JS-DOM 绑定** + **完整自托管 DevTool**，通过稳定的 C ABI 集成到嵌入式应用，目标在 **100 MB 内存** 以内的硬件上流畅渲染 **60 fps** 的复杂界面。

---

## 目录

- [为什么选择 Veloxa](#为什么选择-veloxa)
- [Quick Start](#quick-start)
- [核心能力](#核心能力)
- [DevTool 三件套](#devtool-三件套)
- [架构概览](#架构概览)
- [构建选项](#构建选项)
- [API 概览](#api-概览)
- [示例](#示例)
- [测试与基准](#测试与基准)
- [项目状态](#项目状态)
- [项目结构](#项目结构)
- [文档](#文档)
- [License](#license)

---

## 为什么选择 Veloxa

| 维度 | Veloxa | 浏览器引擎（Chromium / WebKit） | 原生 GUI（Qt / GTK） |
|---|---|---|---|
| 内存占用 | **~10-100 MB** | 200 MB+ | 50-200 MB |
| 二进制体积 | **~5-20 MB**（DEVTOOL=OFF）| 100 MB+ | 30-80 MB |
| 启动时间 | **< 100 ms** | 1-3 s | 200-500 ms |
| 集成方式 | **C ABI**（一个 header） | 复杂嵌入层 | 框架级耦合 |
| HTML/CSS 子集 | ✅ 实用子集 | ✅ 完整 | ❌ |
| JS 脚本 | ✅ QuickJS | ✅ V8 / JSC | 第三方桥接 |
| 渲染后端 | 软件 + SDL2 | GPU 加速 | OpenGL / Vulkan |

### 适用场景

- **车载 HMI** — 仪表盘 / 中控大屏 / HUD
- **工业控制面板** — 触摸屏操作界面
- **IoT 显示设备** — 智能家居 / 充电桩 / 自助终端
- **嵌入式调试 UI** — 现场维护工具内嵌渲染

### 不适用场景

- 完整 Web 浏览器（不实现完整 HTML5 规范 / 无 WebRTC / WebGL / Service Worker）
- 通用桌面应用框架（焦点在嵌入式，不替代 Qt / GTK）
- 高动态 GPU 渲染（当前为软件光栅化）

---

## Quick Start

### 30 秒最小示例

将 HTML + CSS 渲染到一张 PPM 图片：

```cpp
#include "veloxa/api/veloxa_api.h"
#include <cstring>

int main() {
  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(400, 300);

  VxViewConfig config = {};
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;
  config.background_color = 0xFFFFFFFF;
  VxView* view = vx_view_create(&config);

  const char* html = "<div id='hi'>Hello Veloxa</div>";
  const char* css  = "#hi { color: #E74C3C; font-size: 32px; padding: 20px; }";
  vx_view_load_html(view, html, std::strlen(html));
  vx_view_load_css (view, css,  std::strlen(css));
  vx_view_update(view);

  vx_view_destroy(view);
  vx_surface_save_ppm(surface, "out.ppm");
  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);
}
```

### 构建并运行

```bash
# 1. 配置（headless / CI 默认配置）
cmake -B build

# 2. 构建（含所有 1087 单测 + examples/hello）
cmake --build build -j

# 3. 跑测试
ctest --test-dir build -j

# 4. 跑示例
./build/examples/hello_veloxa
# 输出文件：hello_veloxa_output.ppm
```

依赖：CMake 3.20+ / GCC 11+ 或 Clang 14+ / C++17 / Linux（Windows / macOS 实验性支持）。
GoogleTest 通过 `find_package(GTest REQUIRED)` 引入；其他依赖（QuickJS-ng / FreeType / HarfBuzz / libpng / libjpeg）通过 `FetchContent` 自动获取。

详细环境配置（FetchContent 代理 / SDL2 系统包 / benchmark 启用）见 [`memory-bank/techContext.md`](memory-bank/techContext.md)。

---

## 核心能力

| 子系统 | 状态 | 关键能力 |
|---|:-:|---|
| **HTML 解析器** | ✅ | 子集 HTML5、隐式关闭规则、inline style |
| **CSS 引擎**（~45 属性）| ✅ | 选择器匹配、层叠、继承、shorthand（`border` / `border-radius` / `font` / `margin` / `padding`）|
| **Block 布局** | ✅ | margin collapsing（CSS 2.1 §8.3.1）|
| **Inline 布局** | ✅ | line box、`vertical-align`、`line-height` 半-leading（§10.8）|
| **Flexbox 布局** | ✅ | CSS Flexbox Level 1 §9 完整实现 |
| **软件渲染器** | ✅ | 覆盖率光栅化 + Display List + dirty rect 增量重绘 |
| **DOM 事件** | ✅ | W3C 三阶段（capture / target / bubble）+ `:hover` / `:active` / `:focus` |
| **CSS Transitions** | ✅ | 单属性过渡 + 动画曲线 |
| **C API** | ✅ | 不透明指针 ABI（`VxView` / `VxEventLoop` / `VxSurface`）|
| **QuickJS 脚本引擎** | ✅ | EvalGlobal + 32 MiB 内存限制 + **Interrupt Handler + 执行预算 API**（v0.1.0）|
| **JS-DOM 绑定** | ✅ | `getElementById` / 属性 / `style` proxy / 事件监听器 |
| **FreeType + HarfBuzz** | ✅ | 真实字形光栅化 + GlyphCache + `hb_shape` 结果缓存 |
| **PNG / JPEG 解码** | ✅ | libpng + libjpeg-turbo + ImageCache + `<img>` |
| **SDL2 实时窗口后端** | ✅ | WSLg / 桌面真窗口 + 输入事件桥接（鼠标 / 键盘） |
| **DevTool 三件套** | ✅ | 完整自托管 — 见下节 |

### QuickJS 脚本安全（v0.1.0）

宿主可为不可信脚本设置执行预算，防止死循环 / CPU DoS：

```cpp
// 启用预算（默认 budget=0 = 不启用 / 行为不变）
engine.SetEvalInterruptBudget(QuickjsEngine::kDefaultInterruptBudgetCheckpoints);

auto result = engine.EvalGlobal("while (true) {}", "user.js");
if (engine.WasInterrupted()) {
  // result.status() == StatusCode::kAborted
  // result.status().message() == "script aborted (interrupt budget exhausted)"
}
```

T1 mitigation 5 维度完整性：
- **默认安全** — `budget=0` 默认值不影响既有调用方
- **opt-in 启用** — 调用方显式 `SetEvalInterruptBudget(N>0)` 才生效
- **不可被脚本绕过** — `JS_SetUncatchableError` 设计，JS `try/catch` 无法捕获
- **单线程 atomic 防御** — `std::atomic<int64_t>` + relaxed memory order
- **状态可查** — `WasInterrupted()` getter 区分 timeout vs 异常

---

## DevTool 三件套

Veloxa 内建一套**自托管（dogfood）** DevTool — 用 Veloxa 自身渲染调试 UI，无需第三方浏览器即可对目标 Document 做 Inspect / 性能监控 / 热重载。

| Phase | 子系统 | 主交付 | 关键 C API |
|:-:|---|---|---|
| **A** | Inspector | 双 Document（target + DevTool）+ 跨文档 DOM JSON + T3 redaction policy | `vx_view_attach_devtool` / `vx_view_serialize_dom_json` / `vx_inspector_set_redaction_policy` |
| **B** | Performance Overlay | 5 PipelineHooks + 60 帧 ring buffer + F11 HUD toggle + dirty rect 边框可视化 | `vx_view_set_pipeline_hooks` / `vx_view_get_perf_stats` / `vx_view_is_hud_visible` |
| **C** | Hot Reload | Linux inotify + CSS-only 增量重载 + T2 路径穿越 8 步守卫 + warning 语义层 | `VxDevtoolOptions.hot_reload_dir` / `vx_view_hot_reload_tracked_count` |

### 启用 DevTool

```bash
cmake -B build -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON
cmake --build build --target hello_devtool
./build/examples/hello_devtool
# F12 切换 DevTool 可见 / F11 切换 Performance HUD
```

### 安全设计

- **A14 链接闭包零字节增长** — `VX_BUILD_DEVTOOL=OFF` 时二进制体积零增长（pre-merge ctest smoke 强制守门）
- **T2 路径穿越 8 步守卫** — Hot Reload absolute root + canonical path + symlink reject + max-file-size 4 MiB + .css extension filter + ...
- **T3 redaction policy** — `input[type=password]` 等敏感字段默认 `[REDACTED]`
- **T5 / T6 / T7** — DisplayList overlay 隔离 + callback budget guard + 序列化 OOM 守卫

dogfood 范例：[`examples/hello_devtool.cc`](examples/hello_devtool.cc)
设计 spec：[`docs/specs/2026-04-30-devtool-design.md`](docs/specs/2026-04-30-devtool-design.md)
归档文档：[`memory-bank/archive/`](memory-bank/archive/)（Phase A/B/C 各一份）

---

## 架构概览

```
                    ┌──────────────────────────────────┐
                    │     veloxa/api (C ABI 公共头)    │
                    │   veloxa/api/veloxa_api.h        │
                    └──────────────────┬───────────────┘
                                       │
        ┌──────────────────────────────┼──────────────────────────────┐
        │                              │                              │
        ▼                              ▼                              ▼
┌───────────────┐            ┌───────────────┐            ┌───────────────┐
│ veloxa/core   │            │ veloxa/script │            │veloxa/devtool │
│ (Application, │            │  (QuickJS +   │            │ (Inspector +  │
│  EventLoop,   │◀──────────▶│  JS-DOM)      │            │  PerfOverlay +│
│  Update)      │            │               │            │  HotReload)   │
└───────┬───────┘            └───────┬───────┘            └───────┬───────┘
        │                            │                            │
        └──────────────┬─────────────┴───────────────┬────────────┘
                       │                             │
                       ▼                             ▼
              ┌────────────────┐            ┌─────────────────┐
              │ veloxa/graphics│            │  veloxa/text    │
              │ (Layout, Style,│            │ (FreeType +     │
              │  Render)       │            │  HarfBuzz +     │
              └───────┬────────┘            │  GlyphCache)    │
                      │                     └─────────────────┘
                      ▼
              ┌────────────────┐
              │veloxa/platform │
              │ (Surface, IO)  │
              │  └─ sdl2/      │
              └───────┬────────┘
                      │
                      ▼
              ┌────────────────┐
              │veloxa/foundation│
              │ (Status, Vector,│
              │  HashMap, Log)  │
              └────────────────┘
```

**8 大子系统：**

| 模块 | 路径 | 职责 |
|---|---|---|
| `foundation` | `veloxa/foundation/` | 基础设施（`Status` / `StatusOr` / 自定义 `Vector` / `HashMap` / 日志）|
| `platform` | `veloxa/platform/` | 平台抽象 + 内存 surface + SDL2 后端（可选）|
| `graphics` | `veloxa/graphics/` | CSS Engine / Layout Engine / Render Pipeline / DisplayList |
| `core` | `veloxa/core/` | Application 主循环 + UpdateManager + DOM + Event System + TransitionManager |
| `text` | `veloxa/text/` | FreeType + HarfBuzz 字形测量 + GlyphCache |
| `script` | `veloxa/script/` | QuickJS 引擎 + JS-DOM 绑定 + Interrupt Handler |
| `api` | `veloxa/api/` | C ABI 公共头 + 实现 |
| `devtool` | `veloxa/devtool/` | Inspector + PerfOverlay + HotReload（VX_BUILD_DEVTOOL=ON 才编译） |

---

## 构建选项

| Option | 默认 | 说明 |
|---|:-:|---|
| `VX_BUILD_TESTS` | `ON` | 编译单元测试（GoogleTest）+ 启用 ctest |
| `VX_BUILD_BENCHMARKS` | `OFF` | 编译性能基准（FetchContent 拉取 google/benchmark） |
| `VX_PLATFORM_SDL2` | `OFF` | 编译 SDL2 真窗口后端（`vx_event_loop_create_sdl2` + `vx_surface_create_window`） |
| `VX_BUILD_DEVTOOL` | `OFF` | 编译 DevTool 三件套（启用时 ctest +165 测；A14 守门保证 OFF 时零字节增长） |
| `VX_LOG_LEVEL` | `0` | 最低日志等级（0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=FATAL） |

### 常见配置组合

```bash
# 1. 最小 headless + 测试（CI 默认）
cmake -B build

# 2. SDL2 + DevTool 三件套（开发者全功能）
cmake -B build -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON

# 3. 性能基准
cmake -B build -DVX_BUILD_BENCHMARKS=ON
cmake --build build --target bench_drawtext
./build/benchmarks/bench_drawtext

# 4. Production 极小体积（A14 零字节增长验证）
cmake -B build -DVX_BUILD_TESTS=OFF -DVX_BUILD_DEVTOOL=OFF -DVX_PLATFORM_SDL2=OFF \
              -DCMAKE_BUILD_TYPE=Release
```

### 复用次配置 baseline 验证（开发者技巧）

切换 DEVTOOL=ON ↔ OFF 等次配置 baseline 验证时，复用主 `build/_deps` 避免 FetchContent 重新拉取（节省 ~75s）：

```bash
cmake -S . -B build-off -G Ninja \
    -DVX_BUILD_DEVTOOL=OFF \
    -DFETCHCONTENT_BASE_DIR="$(pwd)/build/_deps"
# Configuring done in 1.8s（vs 75s+ 冷启动）
```

---

## API 概览

唯一公共头：[`veloxa/api/veloxa_api.h`](veloxa/api/veloxa_api.h)（~318 行 C99 兼容）。

### 三 Opaque Handle

```c
typedef struct VxView      VxView;       // 渲染视图（Document + Layout + Render）
typedef struct VxEventLoop VxEventLoop;  // 事件循环（headless / SDL2）
typedef struct VxSurface   VxSurface;    // 渲染表面（memory PPM / SDL2 window）
```

### 错误码

```c
typedef enum {
  VX_OK = 0,
  VX_ERROR_NULL_PARAM       = -1,
  VX_ERROR_INVALID_STATE    = -2,
  VX_ERROR_OUT_OF_MEMORY    = -3,
  VX_ERROR_NOT_FOUND        = -4,
  VX_WARNING_HOT_RELOAD_FAILED = 1,  // 警告（非阻塞，主功能继续）
} VxResult;
```

### 主要 API 速查

| 类别 | 函数 |
|---|---|
| **Lifecycle** | `vx_event_loop_create_headless` / `vx_event_loop_create_sdl2` / `vx_view_create` / `vx_view_destroy` |
| **Content** | `vx_view_load_html` / `vx_view_load_css` / `vx_view_load_font` / `vx_view_load_script` |
| **Rendering** | `vx_view_update` / `vx_view_run` / `vx_view_quit` |
| **Input** | `vx_view_inject_input` / `vx_event_loop_pump_input` |
| **Surface** | `vx_surface_create_memory` / `vx_surface_create_window` / `vx_surface_save_ppm` |
| **DevTool — Attach** | `vx_view_attach_devtool` / `vx_view_detach_devtool` / `vx_view_devtool_loaded` |
| **DevTool — Inspector** | `vx_view_serialize_dom_json` / `vx_inspector_set_redaction_policy` |
| **DevTool — Performance** | `vx_view_set_pipeline_hooks` / `vx_view_is_hud_visible` |
| **DevTool — Hot Reload** | `VxDevtoolOptions.hot_reload_dir` / `vx_view_hot_reload_tracked_count` |

详细使用规约（双调用协议 / max_size 守卫 / lazy-attach C ABI 容错 / F11/F12 hotkey 拦截语义）见头文件 docstring。

---

## 示例

`examples/` 目录下三个递进式 example：

| Example | 文件 | 演示内容 | 构建命令 |
|---|---|---|---|
| **hello** | [`examples/hello.cc`](examples/hello.cc) | 最简 headless 渲染 → PPM 输出 | `cmake --build build --target hello_veloxa` |
| **hello_sdl2** | [`examples/hello_sdl2.cc`](examples/hello_sdl2.cc) | SDL2 真窗口 + 鼠标/键盘输入 + `vx_view_run` 主循环 | 需 `-DVX_PLATFORM_SDL2=ON` |
| **hello_devtool** | [`examples/hello_devtool.cc`](examples/hello_devtool.cc) | DevTool 三件套完整 dogfood + F12/F11 hotkey + Hot Reload | 需 `-DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON` |

---

## 测试与基准

### ctest baseline（截至 2026-05-04）

| 配置 | 测试数 | 耗时 |
|---|:-:|:-:|
| `DEVTOOL=ON` | **1252 PASS** | ~6.4s |
| `DEVTOOL=OFF` | **1087 PASS** | ~6.4s |
| `SDL2=ON` | **~1270 PASS** | ~6.5s |

**所有配置零退化** — A14 链接闭包 + ctest smoke 强制守门。

### 性能基准（`benchmarks/`）

13 个 Google Benchmark 套件覆盖 CSS Tokenizer / CSS Parser / CSS Property Lookup / Layout Build Tree / Layout Flex / Render Record / Render Replay / DrawText / HashMap / String / ImageCache / Allocators / Containers。基线对照见 [`benchmarks/baseline/README.md`](benchmarks/baseline/README.md)。

---

## 项目状态

### 已完成里程碑

| 日期 | 里程碑 | 备注 |
|---|---|---|
| 2026-04-30 | Layout 正确性消化 | margin collapsing + line box + vertical-align + JS path 一致性 |
| 2026-04-30 | DevTool 三件套蓝图设计 | spec + plan + creative ×7 |
| 2026-05-02 | DevTool **Phase A · Inspector** | 双 Document + 跨文档 DOM JSON + T3 redaction |
| 2026-05-03 | DevTool **Phase B · Performance Overlay** | 5 PipelineHooks + 60 帧 ring buffer + F11 HUD |
| 2026-05-03 | DevTool **Phase C · Hot Reload** | Linux inotify + CSS-only 增量重载 + T2 8 步守卫 |
| 2026-05-03 | **DevTool 三件套主线收官** ✅ | Inspector + Performance Overlay + Hot Reload 完整闭环 |
| 2026-05-03 | **QuickJS Interrupt Handler + 执行预算 API** | T1 mitigation 5 维度完整性 / 解锁 Console JS REPL |

### 路线图

- **TASK-20260503-04 DevTool Phase D · Console JS REPL**（搁置 → 硬前置依赖 #44 已闭环 / 待恢复）
- **DevTool Phase E** · JS Debugger backend（QuickJS Debug API）
- **DevTool Phase F** · CDP 远程调试 port（HMAC token + nonce + loopback only）
- **DevTool Phase G** · 完整 UI 编辑器（dogfood 完整闭环）
- **DomBindings R2 三连补全**（`Element.children` / `addEventListener` / `innerHTML setter`）
- **#35 阶段 2** · 拆 LayoutEngine 内 style/layout

详细 P3 候选清单见 [`memory-bank/activeContext.md`](memory-bank/activeContext.md) 「待处理事项」段。

---

## 项目结构

```
loong-veloxa/
├── veloxa/              # 8 大子系统源码
│   ├── api/             # C ABI 公共头 + 实现
│   ├── core/            # Application 主循环 + UpdateManager + DOM + Event
│   ├── devtool/         # DevTool 三件套（VX_BUILD_DEVTOOL=ON）
│   ├── foundation/      # Status / Vector / HashMap / Log
│   ├── graphics/        # CSS Engine / Layout / Render Pipeline
│   ├── platform/        # 平台抽象 + memory surface + sdl2/
│   ├── script/          # QuickJS + JS-DOM + Interrupt Handler
│   └── text/            # FreeType + HarfBuzz + GlyphCache
├── examples/            # 3 个递进式 example
├── tests/               # 1252 单元测试 + 集成测试
├── benchmarks/          # 12 个 Google Benchmark 套件
├── docs/
│   ├── specs/           # 设计 spec
│   ├── plans/           # 实施 plan
│   └── reports/         # 性能 / 安全报告
├── memory-bank/         # 项目上下文（任务 / 进度 / 系统模式 / 归档）
└── CMakeLists.txt
```

---

## 文档

| 类别 | 路径 |
|---|---|
| C API 参考 | [`veloxa/api/veloxa_api.h`](veloxa/api/veloxa_api.h)（每个函数含详细 docstring） |
| DevTool 设计 spec | [`docs/specs/2026-04-30-devtool-design.md`](docs/specs/) |
| 项目技术上下文 | [`memory-bank/techContext.md`](memory-bank/techContext.md)（构建依赖 / 工具链版本 / 历史技术债） |
| 系统模式 / 架构决策 | [`memory-bank/systemPatterns.md`](memory-bank/systemPatterns.md) |
| 任务归档 | [`memory-bank/archive/`](memory-bank/archive/)（每个里程碑一份详细归档） |
| benchmark baseline | [`benchmarks/baseline/README.md`](benchmarks/baseline/README.md) |
| WPT 测试 fixture | [`tests/fixtures/wpt/README.md`](tests/fixtures/wpt/README.md) |

---

## License

License 待定。当前为内部开发阶段（v0.1.0 pre-release）。
