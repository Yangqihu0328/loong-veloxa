# Loong Veloxa

轻量级 HTML/CSS 渲染引擎，面向车载 / 嵌入式场景。

Veloxa 提供子集 HTML5 + CSS 引擎（~45 属性）+ Block/Inline/Flex 布局
+ 软件渲染器 + W3C DOM 事件 + CSS Transitions + QuickJS 脚本引擎，
通过稳定的 C ABI 集成到嵌入式应用，目标在 100MB 内存以内的硬件上
渲染 60fps 流畅 HMI 界面。

## 核心能力

| 子系统 | 状态 | 备注 |
|---|:-:|---|
| HTML 解析器（子集 HTML5）| ✅ | 隐式关闭规则 |
| CSS 引擎（~45 属性）| ✅ | 选择器匹配、层叠、继承 |
| Block / Inline / Flex 布局 | ✅ | CSS 2.1 + Flexbox Level 1 §9 |
| 软件渲染器 | ✅ | 覆盖率光栅化 + Display List |
| 事件系统 | ✅ | W3C DOM Events 三阶段 + `:hover/:active/:focus` |
| 脏区更新 | ✅ | DisplayList diff |
| CSS Transitions | ✅ | 单属性过渡 |
| C API | ✅ | 不透明指针 ABI |
| QuickJS 脚本引擎 | ✅ | EvalGlobal + 32 MiB 内存限制 |
| FreeType + HarfBuzz 字体 | ✅ | 真实字形光栅化 + GlyphCache |
| PNG / JPEG 解码 | ✅ | 解码 + 缓存 + DrawImage |
| JS-DOM 绑定 | ✅ | getElementById、属性、style proxy、事件 |
| SDL2 实时窗口后端 | ✅ | WSLg / 桌面真窗口 + 输入事件桥接 |
| **DevTool 三件套** | ✅ | 见下方专段 |

## DevTool 三件套（开发者主线）

Veloxa 内建一套自托管（dogfood）DevTool，以 Veloxa 引擎自身渲染调试 UI，
无需第三方浏览器即可对目标 Document 做 inspect / 性能监控 / 热重载。

| Phase | 子系统 | 主交付 | 关键 C API |
|---|---|---|---|
| **A** | Inspector | 双 Document（target + DevTool）+ 跨文档 DOM JSON + T3 redaction | `vx_view_attach_devtool` / `vx_view_serialize_dom_json` / `vx_inspector_set_redaction_policy` |
| **B** | Performance Overlay | 5 PipelineHooks + 60 帧 ring buffer + F11 HUD toggle + dirty rect 边框 | `vx_view_set_pipeline_hooks` / `vx_view_get_perf_stats` / `vx_view_is_hud_visible` |
| **C** | Hot Reload | Linux inotify + CSS-only 增量重载 + T2 路径穿越 8 步守卫 + warning 语义层 | `VxDevtoolOptions.hot_reload_dir` / `vx_view_hot_reload_tracked_count` |

- **F12** 切换 DevTool 可见 / **F11** 切换 Performance HUD
- 编译开关：`-DVX_BUILD_DEVTOOL=ON`（关闭时 A14 链接闭包零字节增长，由 ctest smoke 守门）
- dogfood 范例：`examples/hello_devtool.cc`
- 设计 spec：`docs/specs/2026-04-30-devtool-design.md`
- 蓝图 plan：`docs/plans/2026-04-30-devtool.md`
- 归档文档：`memory-bank/archive/archive-TASK-20260502-01.md`（Phase A）/ `archive-TASK-20260502-02.md`（Phase B）/ `archive-TASK-20260503-01.md`（Phase C）

## 构建与运行

基础构建（headless / 测试 / CI）：

```bash
cmake -B build
cmake --build build -j
cd build && ctest -j
```

SDL2 真窗口 + DevTool 三件套：

```bash
cmake -B build -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON
cmake --build build --target hello_devtool
./build/examples/hello_devtool
```

详细环境配置（FetchContent 代理、SDL2 系统包安装、benchmark 启用等）见
`memory-bank/techContext.md`。

## 项目状态

- 当前 ctest baseline：DEVTOOL=ON 1247 PASS / DEVTOOL=OFF 1082 PASS / SDL2=ON 1265 PASS
- 主线收官里程碑：DevTool 三件套 ✅（2026-05-03）
- 后续路线：见 `memory-bank/activeContext.md`「待处理事项」段（含 P3 候选清单）
