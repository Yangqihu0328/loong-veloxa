# 实现计划：DevTool Phase C — Hot Reload 实施（Linux inotify + CSS-only 增量重载）

**任务 ID：** TASK-20260503-01
**复杂度：** Level 3（中等功能 — 跨 4 子系统 / 蓝图 plan §Phase C 12 子任务（精化后 11 — C.2.2 合并）/ 新增 hot_reload/ 子系统但非 dogfood UI 子系统级 / **不 escalate**，VAN push-back 6 项已沉淀）
**安全相关：** ✅ 是（**T2 路径穿越高威胁** — 8 步守卫 + **T8 mutation propagation 中威胁** — CSS-only 严格不踩 F-025）
**复用 spec：** `docs/specs/2026-04-30-devtool-design.md`（A10-A12 + A13-A14 + T2/T8 + I7/I8 + R4/R6）— 无新需求
**复用 creative：** `memory-bank/creative/creative-devtool-hot-reload.md` 5 决策（FileWatcher 抽象 / CSS-only 增量 / DOM 状态保留协议 / watcher root 边界 / CSS 解析失败错误恢复）— 无新设计
**蓝图参照：** `docs/plans/2026-04-30-devtool.md` §Phase C 段（12 子任务粗模板 — VAN 阶段精化 11 子任务）

---

## 0. Phase 0 — 全局约束与实证（writing-plans 必填 9 段全部就绪 + 4 段本任务专属扩展）

### 0.1 ctest baseline 二次验证（待 build 阶段首次 reconfigure 实测）

**预期基线（继承自 main `c0c4cbd` = TASK-20260502-02 Phase B 完成快照）：**
- DEVTOOL=ON: **1228/1228 PASS** ✅（继承 Phase B 终态）
- DEVTOOL=OFF: **1082/1082 PASS** ✅
- A14 链接闭包零自动化守门 `tests/smoke/devtool_a14_link_closure.cmake` 持续守护 ✅

**Build 阶段第一步必跑（因 `build/` 已清理）：**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DVX_BUILD_DEVTOOL=ON  # ON 基线
cmake --build build -j8
ctest --test-dir build --output-on-failure -j8
# 预期：1228 PASS

cmake -S . -B build-noffi -DCMAKE_BUILD_TYPE=Debug -DVX_BUILD_DEVTOOL=OFF  # OFF 基线
cmake --build build-noffi -j8
ctest --test-dir build-noffi --output-on-failure -j8
# 预期：1082 PASS
```

### 0.2 CMake 链接审计

**当前 `vx_devtool` 链接拓扑（grep 实证 `veloxa/devtool/inspector/CMakeLists.txt:6`）：**

```cmake
target_link_libraries(vx_devtool PUBLIC vx_core vx_devtool_resources)
```

**新增/修改链接拓扑：**

- `vx_devtool` 静态库 += `hot_reload/file_watcher.cc` + `hot_reload/file_watcher_inotify.cc` + `hot_reload/hot_reload_manager.cc`（C.0.1 / C.1.1 / C.2.1）— append 到既有 `veloxa/devtool/inspector/CMakeLists.txt`（沿用 Phase A.1.5 / Phase B.1.1 范式：`target_sources(vx_devtool PRIVATE ...)` append，避免新建子目录 lib，最小侵入）
- `vx_devtool` 链接 += **`Threads::Threads`**（PRIVATE，C.1.1 std::thread watch loop 必需）— **本任务唯一新增 link**
- `vx_api` 静态库 += `vx_view_attach_devtool` 加 hot_reload_dir 参数透传（C.4.1）— 既有 ABI 扩展，无新 C API 函数（HotReloadManager 内部直接调 `app->LoadCSS(...)`，零新公开 ABI）
- 测试新加 `tests/devtool/hot_reload/CMakeLists.txt` + 注册到 `tests/devtool/CMakeLists.txt`（与 inspector/ + overlay/ 兄弟子目录）— 由 `if(VX_BUILD_DEVTOOL)` guard

**结论：零新静态库循环依赖**（vx_devtool → vx_core 既有 B 链 A 模式持续）。Phase A/B 的 `if(VX_BUILD_DEVTOOL) target_link_libraries(vx_script PRIVATE vx_devtool)` 与 `if(VX_BUILD_DEVTOOL) target_link_libraries(vx_api PRIVATE vx_devtool)` 范式直接复用。

### 0.3 静态库循环依赖审计

| 新符号 | 归属 | 被谁调用 | 循环风险 |
|---|---|---|:-:|
| `vx::devtool::hot_reload::FileWatcher` 抽象基类 | `vx_devtool/hot_reload/file_watcher.h` | `vx_devtool::HotReloadManager` 内部持有 + `tests/devtool/hot_reload/file_watcher_test.cc` | ✅ 单向 |
| `vx::devtool::hot_reload::InotifyFileWatcher` Linux 实现 | `vx_devtool/hot_reload/file_watcher_inotify.{h,cc}` | `FileWatcher::CreatePlatform()` 工厂返回 + `tests/devtool/hot_reload/file_watcher_test.cc` | ✅ 单向 |
| `vx::devtool::hot_reload::HotReloadManager` | `vx_devtool/hot_reload/hot_reload_manager.{h,cc}` | `vx::Application::LoadDevtoolDocument` 内 attach 时构造 + `tests/devtool/hot_reload/*` | ✅ 单向，Application → HotReloadManager → app->LoadCSS 反向调用通过 callback 注入（不形成 link 闭环）|
| `vx_view_attach_devtool` 扩展（hot_reload_dir 参数）| `vx_api/veloxa_api.cc` | 用户代码 + example | ✅ 公开 ABI 表面 |

✅ 无静态库循环依赖。HotReloadManager 调 `app->LoadCSS` 是反向调用，但通过 callback function pointer 注入，**不**形成 vx_core ↔ vx_devtool link 闭环（vx_devtool 已 PUBLIC link vx_core，反向不需要）。

### 0.4 FetchContent 网络代理守卫

- 本任务零新 FetchContent 依赖（Linux inotify 是 libc 系统调用 + std::thread/std::atomic/std::condition_variable 是 C++ stdlib + POSIX `realpath()` 是 libc）
- **跳过** FetchContent 守卫流程

### 0.5 测试基础设施审计（friend pattern 关键）

**Phase 0 grep 实证（已执行）：** `friend class` 在 `veloxa/devtool/` 仅 1 处既有命中（`PerfOverlayTest` 在 `perf_overlay.h:101`，Phase B 引入）→ C.1.x + C.2.x 测试需访问内部状态时直接复用 Phase B 范式。

| 子任务 | 测试需访问 | 当前访问路径 | 引入方案 |
|---|---|---|---|
| C.0.1 FileWatcher 抽象 | 无（pure virtual + WatchOptions struct） | — | 无需 friend |
| C.1.1 InotifyFileWatcher | inotify_fd_ / wd_ / running_ / watch_thread_.joinable() | 当前 PRIVATE | `friend class FileWatcherTest;` 白名单 |
| C.1.2 T2 守卫 | rejected path 计数 / canonical_root_ 内部 | 新接口（`PUBLIC accept_count() / reject_count()` getter）+ 内部状态 friend | ✅ PUBLIC counter + friend 私有内部 |
| C.2.1 HotReloadManager | path_to_stylesheet_id_ 内部 map / OnFileChanged 调 callback 计数 | `PUBLIC tracked_count() const` getter（蓝图 plan §合规设定）| ✅ PUBLIC 自然语义 |

**禁止项：** ❌ 不用 `#define private public` / ❌ 不用 `reinterpret_cast` 越权 / ❌ 不在生产代码中暴露内部状态 PUBLIC（除非有 read-only 自然语义）

### 0.6 边界输入清单（writing-plans 必填，T2/T8 强相关）

| # | 类别 | 示例 | 预期行为 | 对应威胁 / 验收 |
|:-:|---|---|---|---|
| 1 | 默认 | watcher root = `/tmp/myapp_css/`，文件 `style.css` 修改 | callback 触发 + LoadCSS + restyle | A10 |
| 2 | 路径穿越（绝对）| filename = `/etc/passwd` | reject + warn 日志 | T2 / A11 |
| 3 | 路径穿越（相对）| filename = `../../etc/passwd` | reject + warn 日志 | T2 / A11 |
| 4 | symlink 跨 root | `/tmp/myapp_css/evil.css` → `/etc/shadow` symlink | realpath canonicalize → boundary check fail → reject | T2 / A11 |
| 5 | 文件超大 | 5 MiB CSS 文件 | reject + warn 日志（max_size 4 MiB 超出） | T2 / A12 |
| 6 | 非 .css 扩展 | `style.html` 修改 | filter pass-through（不调 LoadCSS）| CSS-only 契约 |
| 7 | 非绝对 root | watcher root = `relative/path` | Status::kInvalidArgument | T2 / A11 |
| 8 | watcher root 不存在 | watcher root = `/nonexistent/` | Status::kNotFound | T2 |
| 9 | watcher root = symlink | watcher root 本身是 symlink | realpath canonicalize 后再 add_watch | T2 |
| 10 | inotify_init1 失败 | fd 耗尽场景（mock） | Status::kInternal + errno 信息 | A11 |
| 11 | atomic write（vim/VSCode）| `style.css.swp` → mv → `style.css`（IN_MOVED_TO + IN_CLOSE_WRITE 重复事件）| 50 ms debounce → 单次触发 | A10 / R6 |
| 12 | restart watcher | Stop() + Start() 同 root | join thread + 重置 fd + 重新 add_watch | A10 |
| 13 | LoadHTML 反向探针 | HotReloadManager 接收 `.html` 修改 | filter 短路（不调 LoadCSS / 不调 LoadHTML）| F-025 不踩 / 反向探针 |
| 14 | dom_bindings_ 状态保留 | LoadCSS 后 `app->dom_bindings()` 仍 valid | LoadCSS 不动 dom_bindings_（grep 实证 application.cc:84-87）| T8 / F-025 不踩 |
| 15 | 回调线程安全 | inotify thread 触发 + main thread Update 并发 | thread-safe queue 跨线程消息（mutex + condition_variable）| R7（新增 — 见 §2 R 风险）|
| 16 | watch loop 关闭语义 | running_ = false → 50ms 后 join | std::atomic + sleep_for 兜底（read EAGAIN 路径） | R7 |

### 0.7 调用链端到端验证（已执行 grep 实证）

**inotify event → Application::LoadCSS → restyle 完整链路：**

| 层级 | 函数 | 文件 | 改动 | 已实证 |
|:-:|---|---|---|:-:|
| L0 | `InotifyFileWatcher::WatchLoop` | hot_reload/file_watcher_inotify.cc（新建）| 新建 std::thread loop + 8 步 T2 守卫 | 🔴 新建 |
| L1 | `InotifyFileWatcher::ProcessBuffer → opts_.callback(fce)` | 同上 | 跨线程 callback 投递（HotReloadManager::OnFileChanged）| 🔴 新建 |
| L2 | `HotReloadManager::OnFileChanged`（**inotify 线程上下文**）| hot_reload/hot_reload_manager.cc（新建）| filter `.css` 后缀 + push 到 thread-safe queue | 🔴 新建 |
| L3 | `HotReloadManager::DrainEvents`（**main 线程，每帧 Update 时调**）| 同上 | drain queue → 调 `app_->LoadCSS(ReadFile(path))` | 🔴 新建 |
| L4 | `Application::LoadCSS(StringView)` | application.cc:84 | `stylesheets_.push_back(...)` + `update_manager_.reset()` | ✅ 已就绪 |
| L5 | next `app->Update()` 触发 EnsureUpdateManager → 重建 update_manager + restyle | application.cc + update_manager.cc | 既有 lazy-attach 范式 | ✅ 已就绪 |

**关键安全契约：** L4 仅动 stylesheets_ + update_manager_，**不动 dom_bindings_ / target_document_** → CSS-only 严格契约自然成立 → F-025 不踩。**反向探针（L2）**：测试故意把 `.html` 文件路径塞进 OnFileChanged → 断言 `app->dom_bindings()` 仍 valid + 没有 LoadCSS 调用 + 没有 LoadHTML 调用。

### 0.8 既有测试隐式契约 fingerprint（grep `tests/core/application_test.cc` + `tests/api/api_test.cc`）

| 文件 | 关键测试 | 隐含契约 | 影响 |
|---|---|---|---|
| `tests/core/application_test.cc` | LoadCSS 后 `stylesheets_.size()` ++ + update_manager 重建 | LoadCSS 不动 dom_bindings_ / target_document_ | C.2.1 反向探针单测必须断言此契约 |
| `tests/api/api_test.cc` | `vx_view_load_css` 双调用模式 | C ABI 既有路径 | C.4.1 hot_reload_dir 参数透传不影响既有 vx_view_load_css 路径 |

**判读：** 既有 LoadCSS 路径测试覆盖充分（push_back + reset），HotReloadManager 仅作为 LoadCSS 触发器，复用既有契约。

### 0.9 CSS shorthand 能力 grep 表

⊘ **跳过** — 本任务不涉及新 CSS shorthand 解析（仅触发既有 CssParser::Parse 路径）。

### 0.10 工具链与子系统关闭守门

**VX_BUILD_DEVTOOL #ifdef + CMake conditional 范式（30+ 处既有命中，已成熟）：**

```cpp
// veloxa/api/veloxa_api.cc — vx_view_attach_devtool 现有签名
#ifdef VX_BUILD_DEVTOOL
VxResult vx_view_attach_devtool(VxView* view, ..., const char* hot_reload_dir) {
  // 实施
}
#else
VxResult vx_view_attach_devtool(VxView* view, ..., const char* hot_reload_dir) {
  return VX_ERROR_UNSUPPORTED;  // OFF stub
}
#endif
```

**A14 黑名单 +3 项**（C.5.2 finalize 必加）：

```cmake
# tests/smoke/devtool_a14_link_closure.cmake — Phase C 黑名单追加
set(DEVTOOL_INTERNAL_SYMBOLS
  ...  # Phase A/B 既有
  FileWatcher                # C.0.1
  InotifyFileWatcher         # C.1.1
  HotReloadManager           # C.2.1
)
# NOT in blacklist intentional 排除项清单：
# - vx_view_attach_devtool C ABI 公开（hot_reload_dir 参数扩展，OFF 路径返 VX_ERROR_UNSUPPORTED）
```

### 0.11 工具链 grep 命中

| 工具 | 验证 | 兜底 |
|---|---|---|
| `rg` / `ripgrep` | sandbox 环境不可用 | 使用 Grep tool（内置 ripgrep）+ 系统 `grep -rE` |
| `jq` | sandbox 环境缺失 | python3 + `import json` heredoc |
| `realpath` (POSIX) | libc 标准 — Linux 100% 可用 | — |
| `inotify_init1 / inotify_add_watch` | libc 标准 — Linux 100% 可用 | — |
| `pthread / -lpthread` | CMake `Threads::Threads` 自动 link | — |

### 0.12 std::thread 既有用例 grep（**本任务专属新段**）

**Phase 0 grep 实证（已执行）：** `std::thread / #include <thread> / std::condition_variable` 在 `veloxa/` 子目录**完全零命中**（仅 `veloxa/foundation/memory/memory_stats.h` 含 `std::atomic`）→ 本任务首次引入多线程 + 跨线程消息传递。

**设计决策（B4 / 生命周期与同步）：**

```cpp
// veloxa/devtool/hot_reload/hot_reload_manager.h
class HotReloadManager {
 public:
  // === inotify 线程上下文（callback 入口）===
  void OnFileChanged(const FileChangeEvent& evt);  // thread-safe push to queue

  // === main 线程上下文（每帧 Update 时调） ===
  void DrainEvents();  // pop queue + call app_->LoadCSS

 private:
  vx::Application* app_ = nullptr;  // 不持有所有权
  std::unique_ptr<FileWatcher> watcher_;

  // 跨线程消息传递
  std::mutex queue_mu_;
  std::queue<FileChangeEvent> queue_;
  std::condition_variable queue_cv_;  // C.1.1 watch loop 退出时 notify

  friend class HotReloadManagerTest;  // §0.5 white-listed
};
```

**关键不变量：**
- inotify 线程**仅** push queue（不调 app_ 任何方法）
- main 线程**仅** drain queue + 调 LoadCSS
- watcher_ 析构时 stop watch loop + join thread（确保 OnFileChanged 不再触发后才解锁 mutex）

### 0.13 realpath 选型决策实证（**本任务专属新段，B4 决策**）

**Phase 0 grep 实证（已执行）：** `realpath / std::filesystem::canonical / #include <filesystem>` 在 `veloxa/` 子目录**完全零命中** → 本任务首次引入路径 canonicalize。

**决策矩阵：**

| 候选 | lib 体积 | 异常处理 | 与代码库一致性 | CMake link 复杂度 | VAN 推荐 |
|---|:-:|:-:|:-:|:-:|:-:|
| **A POSIX `realpath(path, nullptr)` ⭐** | 小（libc 内置）| Status 返回（与代码库 vx::Status 模式一致）| ✅ 高 — 全代码库 POSIX 风格 string ops | 零额外 link | **A** |
| B `std::filesystem::canonical()` | 大（libstdc++fs ~200KB）| 抛 std::filesystem_error 异常 | ⚠️ 不一致 — 全代码库零 std::filesystem | GCC 8 前需独立 link `stdc++fs` | — |

**RAII wrapper（避免 malloc'd char* 泄漏）：**

```cpp
// veloxa/devtool/hot_reload/file_watcher_inotify.cc
namespace {
auto MakeRealpathBuf() {
  return std::unique_ptr<char, decltype(&free)>(nullptr, &free);
}

// 使用：
auto canonical = MakeRealpathBuf();
canonical.reset(realpath(input_path.Cstr(), nullptr));
if (!canonical) {
  return Status(StatusCode::kNotFound, "path not found");
}
String canonical_str(canonical.get());
}
```

---

## 1. B1-B8 build 级精化决策表（用户 1 次 AskQuestion 选 all_recommended → 8/8 按 VAN 推荐锁定）

| # | 决策 | 选定 | 理由 |
|:-:|---|---|---|
| **B1** | plan 文档形态 | **A 独立 plan** `docs/plans/2026-05-03-devtool-hot-reload.md` | 与 Phase A/B 一致；蓝图 plan 是参照基线 |
| **B2** | 子任务串行 | **A 严格串行** | inotify 实现是后续测试基础 + creative #2 决策 1 callback 接口要 C.0.1 先锁 |
| **B3** | 测试目录树 | **A 新建 `tests/devtool/hot_reload/`** | 与 Phase B `tests/devtool/overlay/` 同款；HotReload 是独立子系统 |
| **B4** | realpath 工具 | **A POSIX `realpath(path, nullptr)` + RAII wrapper** | lib 体积小 + 与 inotify 同源 POSIX + 全代码库零 std::filesystem 一致性 |
| **B5** | C.2.2 DOM 状态保留 | **A 合并到 C.2.1**（节省 ~27 min） | YAGNI — §0.7 grep 实证 hover/focus/scroll 当前不持久化；P3 候选未来 EventManager 集成时再加 |
| **B6** | commit 粒度 | **A 每子任务 1 commit**（11 commits + 1 finalize） | 与 Phase A/B 一致 |
| **B7** | 估时假设 | **A ~2-3 h plan ×0.6**（落「极窄档延续高效区 0.30-0.45×」） | Phase B 实证 0.40× + 5 大可复用范式 100% 命中 + lazy-attach C ABI 模式 + LoadCSS 已就绪 + C.2.2 合并节省 27 min |
| **B8** | spec / creative | **A 复用** | 蓝图主交付直接用 |

---

## 2. R 风险登记（继承 spec §9 + 新增本任务专属）

| # | 风险 | 来源 | 概率 | 影响 | mitigation |
|:-:|---|---|:-:|:-:|---|
| **R4** | 跨平台抽象一期仅 Linux 未来不兼容 | spec §9 | 🟡 中 | 🟢 低 | C.0.1 抽象基类参考 efsw ≥ 3 平台共有接口；Platform factory nullptr 退化（macOS/Windows return nullptr） |
| **R6** | DOM 状态保留与 hover/focus 不一致 | spec §9 | 🟢 低（§0.7 实证当前不持久化）| 🟢 低 | C.2.1 简单实施 LoadCSS 触发即可（不需要 snapshot/restore — 当前没有持久化状态需保留）；future P3 候选 |
| **R7** | inotify 线程 / main 线程 race condition（**本任务专属新增**）| §0.6 输入 #15-16 | 🟡 中 | 🔴 高 | 跨线程 thread-safe queue（mutex + condition_variable）+ inotify 线程仅 push queue + main 线程仅 drain queue + watcher_ 析构 join thread + std::atomic<bool> running_ + sleep_for 兜底 read EAGAIN 路径 |
| **R8** | T2 8 步守卫漏一步 CVE 级风险（**本任务专属新增**）| spec §7 T2 | 🟡 中（人为疏忽）| 🔴 高 | C.5.1 独立 security_test.cc 8 测全覆盖（§0.6 输入 #2-9 → 1:1 测）+ reverse probe（删 realpath → SymlinkEscape FAIL）+ creative #2 关键代码 8 步注释逐一对应 |
| **R9** | F-025 不踩契约 build 阶段被破坏（**本任务专属新增**）| F3 实证 | 🟢 低（§0.7 调用链清晰）| 🔴 高 | C.2.1 反向探针单测：HotReloadManager OnFileChanged 接收 `.html` 文件 → 断言不调 LoadHTML 也不调 LoadCSS（filter 短路）+ C.2.1 单测显式断言 `app->dom_bindings()` 在 LoadCSS 后仍 valid（指针不变）|
| **R10** | inotify mask 选择不当致 atomic write 重复触发（**本任务专属新增**）| §0.6 #11 | 🟡 中 | 🟢 低 | mask = `IN_MODIFY \| IN_CLOSE_WRITE \| IN_MOVED_TO`（**不**含 IN_CREATE 防 atomic 创建+write 双触发）+ 50 ms debounce per path（last_event_ms_ HashMap 去重）|
| **R11** | watch_thread_ join 阻塞 main thread（**本任务专属新增**）| 析构语义 | 🟢 低 | 🟡 中 | running_ = false → 50 ms 内 read EAGAIN 路径自然退出（sleep_for(50 ms) tick）+ join 等待最多 50 ms + 单测验证析构延迟 ≤ 100 ms |

---

## 3. 安全分析（来自 security.mdc 第一章 — 威胁建模）

### 3.1 信任边界

```
┌───────────────────────────────────────────────────────────┐
│ Untrusted: 文件系统外部进程（vim / VSCode / shell mv）         │
│   ↓ inotify event                                         │
├───────────────────────────────────────────────────────────┤
│ Trust boundary 1: inotify event → InotifyFileWatcher      │
│   8 步 T2 守卫（allowlist + canonicalize + boundary +       │
│   max_size + extension filter + symlink reject + log +    │
│   reverse probe）                                         │
│   ↓ thread-safe queue                                     │
├───────────────────────────────────────────────────────────┤
│ Trust boundary 2: inotify thread → main thread queue      │
│   mutex + condition_variable + atomic<bool> running_      │
│   ↓ HotReloadManager::DrainEvents (main thread)           │
├───────────────────────────────────────────────────────────┤
│ Trust boundary 3: HotReloadManager → Application::LoadCSS │
│   filter `.css` only（CSS-only 严格契约 → 不踩 F-025）       │
│   ↓ stylesheets_.push_back + update_manager_.reset        │
├───────────────────────────────────────────────────────────┤
│ Trusted: vx::Application + DOM tree                       │
└───────────────────────────────────────────────────────────┘
```

### 3.2 STRIDE 关键威胁

| STRIDE | 威胁 | 影响 | mitigation |
|:-:|---|:-:|---|
| **S** Spoofing | 攻击者伪造 inotify event（不直接可行 — kernel 守门）| ⊘ N/A | inotify event 来自 kernel 不可伪造 |
| **T** Tampering | T2 路径穿越 / symlink 跨 root | 🔴 高 | 8 步守卫（§3.3 详述）|
| **R** Repudiation | 无 reload 操作日志 | 🟢 低 | VX_LOG_INFO 每次 LoadCSS 触发 + 文件路径（stripped relative path）|
| **I** Information Disclosure | 错误日志泄露 watcher_root 绝对路径 | 🟢 低 | 仅记录 stripped relative path（去 root 段）|
| **D** Denial of Service | 大文件 + watch loop 死锁 / 线程泄漏 | 🟡 中 | max_file_size 4 MiB + std::atomic running_ + sleep_for 兜底 + 析构 join thread |
| **E** Elevation of Privilege | watcher 进程权限提升 | ⊘ N/A | watcher 进程权限沿用 host process（用户必须以足够权限启动 vx app）|

### 3.3 T2 路径穿越 8 步守卫详细映射

| 步骤 | 防御 | 实施位置 | 对应单测 | 反向探针 |
|:-:|---|---|---|---|
| 1 | watcher root 必须绝对路径 allowlist | `Start()` 入口 `if (!root.StartsWith("/"))` | C.5.1 #1 RelativeRootRejected | 删 `if` → 测 FAIL |
| 2 | inotify mask 限定 IN_MODIFY/IN_CLOSE_WRITE/IN_MOVED_TO | `inotify_add_watch(mask)` | C.5.1 #2 OnlyExpectedMaskRegistered | 改 mask 加 IN_CREATE → 测 FAIL |
| 3 | filename realpath canonicalize | `realpath(full_path, nullptr)` | C.5.1 #3 CanonicalizesSymlink | 删 realpath → 测 FAIL |
| 4 | canonicalized path boundary check 仍在 root 下 | `if (!canonical.StartsWith(root + "/"))` | C.5.1 #4 SymlinkEscapeRejected | 删 boundary check → 测 FAIL |
| 5 | max_file_size 4 MiB 上限 | `stat().st_size > 4 MiB` | C.5.1 #5 MaxFileSizeExceededRejected | 删 size check → 测 FAIL（5 MiB 文件本应 reject）|
| 6 | extension filter（`.css` only） | `MatchAnyExtension(filename, opts.extensions)` | C.5.1 #6 NonCssExtensionFiltered | 删 filter → 测 FAIL（`.html` 通过）|
| 7 | 安全日志（warn 级 — 拒绝时）| `VX_LOG_WARN("rejected: ...")` | C.5.1 #7 RejectionLoggedAtWarn | 删 log → 测 FAIL（捕获 log 流）|
| 8 | reverse probe（破坏 #1-7 → 各对应测试 FAIL）| 见 #1-7 | C.5.1 #8 AllGuardsHaveReverseProbe meta-test | 元测试 — 每步独立反向 |

### 3.4 OWASP A04 不安全设计 — 核心契约

- **CSS-only 严格契约**（不触发 LoadHTML 不踩 F-025）— C.2.1 反向探针单测验证
- **Linux only**（macOS/Windows 不开放接口 → P2 候选）— Platform factory nullptr 退化
- **HotReloadManager 不持有 Application 所有权** — `vx::Application*` raw pointer，析构由 caller 负责（与 InspectorOverlay 同款 ownership 模式）

---

## 4. 子任务清单（11 子任务 5-步 TDD 模板）

### C.0.1 FileWatcher 抽象基类 + Platform factory [TDD]

**文件：**
- 创建：`veloxa/devtool/hot_reload/file_watcher.h`（~80 行）
- 创建：`veloxa/devtool/hot_reload/file_watcher.cc`（~30 行 — Platform factory 实现）
- 测试：`tests/devtool/hot_reload/file_watcher_test.cc`（创建文件，仅含 C.0.1 抽象层测）
- 修改：`veloxa/devtool/inspector/CMakeLists.txt`（target_sources append）
- 修改：`tests/devtool/CMakeLists.txt`（add_subdirectory(hot_reload)）
- 创建：`tests/devtool/hot_reload/CMakeLists.txt`

**估时：** plan 30 min / plan ×0.6 = **18 min**

**步骤 1 — 编写失败测试：**

```cpp
// tests/devtool/hot_reload/file_watcher_test.cc
#include "veloxa/devtool/hot_reload/file_watcher.h"
#include <gtest/gtest.h>

namespace vx::devtool::hot_reload {
namespace {

TEST(FileWatcherTest, CreatePlatformReturnsLinuxImplOnLinux) {
  auto watcher = FileWatcher::CreatePlatform();
#if defined(__linux__)
  EXPECT_NE(watcher.get(), nullptr);
#else
  EXPECT_EQ(watcher.get(), nullptr);  // P2 候选，非 Linux 平台返 nullptr
#endif
}

TEST(FileWatcherTest, AbstractBaseExposesStartStopInterface) {
  auto watcher = FileWatcher::CreatePlatform();
  ASSERT_NE(watcher.get(), nullptr);
  EXPECT_FALSE(watcher->IsRunning());  // 初始未启动
}

}  // namespace
}  // namespace vx::devtool::hot_reload
```

**步骤 2 — 验证失败：** `ctest --test-dir build -R FileWatcherTest -V` → FAIL（FileWatcher class 不存在）

**步骤 3 — 编写最少实现：**

```cpp
// veloxa/devtool/hot_reload/file_watcher.h
#ifndef VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_H_
#define VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_H_

#include <functional>
#include <memory>
#include "veloxa/foundation/status/status.h"
#include "veloxa/foundation/strings/string.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/foundation/types.h"

namespace vx::devtool::hot_reload {

enum class FileChangeKind { kModified, kRenamed, kDeleted };

struct FileChangeEvent {
  String path;        // canonicalized absolute path
  FileChangeKind kind = FileChangeKind::kModified;
  u64 timestamp_ms = 0;
};

struct WatchOptions {
  String root_dir;                                       // must be absolute
  Vector<String> extensions = {String(".css")};          // empty = all
  usize max_file_size = 4 * 1024 * 1024;                 // 4 MiB
  std::function<void(const FileChangeEvent&)> callback;  // invoked on inotify thread
};

class FileWatcher {
 public:
  virtual ~FileWatcher() = default;
  virtual Status Start(WatchOptions opts) = 0;
  virtual void Stop() = 0;
  virtual bool IsRunning() const = 0;

  static std::unique_ptr<FileWatcher> CreatePlatform();
};

}  // namespace vx::devtool::hot_reload

#endif
```

```cpp
// veloxa/devtool/hot_reload/file_watcher.cc
#include "veloxa/devtool/hot_reload/file_watcher.h"

#if defined(__linux__)
#include "veloxa/devtool/hot_reload/file_watcher_inotify.h"  // 前向声明 — C.1.1 创建
#endif

namespace vx::devtool::hot_reload {

std::unique_ptr<FileWatcher> FileWatcher::CreatePlatform() {
#if defined(__linux__)
  return std::make_unique<InotifyFileWatcher>();
#else
  // macOS kqueue / Windows ReadDirectoryChangesW — P2 候选
  return nullptr;
#endif
}

}  // namespace vx::devtool::hot_reload
```

**注：** C.0.1 仅创建抽象基类，`InotifyFileWatcher` 在 C.1.1 创建。为了 C.0.1 可独立编译，需要在 file_watcher.cc 暂时返回 nullptr 占位，或拆分编译单元。**实际选择**：file_watcher.cc 创建空 stub `class InotifyFileWatcher : public FileWatcher { Status Start(WatchOptions) override { return Status::Ok(); } void Stop() override {} bool IsRunning() const override { return false; } };` 内联在 .cc — C.1.1 替换为 file_watcher_inotify.{h,cc} 完整实现。

```cmake
# veloxa/devtool/inspector/CMakeLists.txt — target_sources append
target_sources(vx_devtool PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../hot_reload/file_watcher.cc
)

# tests/devtool/hot_reload/CMakeLists.txt — 新建
vx_add_test(NAME file_watcher_test SOURCES file_watcher_test.cc)
```

**步骤 4 — 验证通过：** `ctest -R FileWatcherTest` → PASS（2 测）

**步骤 5 — 提交：** `git add veloxa/devtool/hot_reload/file_watcher.h veloxa/devtool/hot_reload/file_watcher.cc veloxa/devtool/inspector/CMakeLists.txt tests/devtool/hot_reload/file_watcher_test.cc tests/devtool/hot_reload/CMakeLists.txt tests/devtool/CMakeLists.txt && git commit -m "feat(devtool): FileWatcher 抽象基类 + Platform factory (C.0.1)"`

---

### C.1.1 InotifyFileWatcher 基础实现（init + add_watch + watch loop） [TDD]

**文件：**
- 创建：`veloxa/devtool/hot_reload/file_watcher_inotify.h`（~30 行）
- 创建：`veloxa/devtool/hot_reload/file_watcher_inotify.cc`（~120 行 — 不含 §C.1.2 的 8 步守卫）
- 修改：`veloxa/devtool/hot_reload/file_watcher.cc`（include + use InotifyFileWatcher）
- 测试：`tests/devtool/hot_reload/file_watcher_test.cc`（追加 4 测 — 基础 modify / Start/Stop / restart / IsRunning）
- 修改：`veloxa/devtool/inspector/CMakeLists.txt`（target_sources append + Threads::Threads PRIVATE）

**估时：** plan 90 min / plan ×0.6 = **54 min**

**步骤 1-4：** 完整实现（详见 creative #2 §决策 1 关键代码段）+ 4 RED 测：
- ModifyFileTriggersCallback（写入临时文件 → 50ms 后断言 callback 收到 kModified）
- StartStopRoundTrip（Start → IsRunning true → Stop → IsRunning false + thread joined）
- RestartSucceeds（Start → Stop → Start 再次成功 + fd 重新分配）
- StartReturnsErrorOnInvalidRoot（不存在路径 → kNotFound）

**步骤 5 — 提交：**

```bash
git commit -m "feat(devtool): InotifyFileWatcher 基础实现 + watch loop (C.1.1)"
```

---

### C.1.2 T2 路径穿越守卫 8 步 [TDD] [安全相关]

**文件：**
- 修改：`veloxa/devtool/hot_reload/file_watcher_inotify.cc`（实施 8 步守卫 + log 调用）
- 测试：`tests/devtool/hot_reload/file_watcher_test.cc`（追加 8 测 — §3.3 表 1:1）

**估时：** plan 90 min / plan ×0.6 = **54 min**

**步骤 1 — 编写失败测试（8 项 — §3.3 表）：** RelativeRootRejected / OnlyExpectedMaskRegistered / CanonicalizesSymlink / SymlinkEscapeRejected / MaxFileSizeExceededRejected / NonCssExtensionFiltered / RejectionLoggedAtWarn / DebounceCoalescesRapidEvents

**步骤 2 — 验证失败：** 8 测全 FAIL（守卫未实施）

**步骤 3 — 编写最少实现：** 详见 creative #2 §决策 1 关键代码段「ProcessBuffer」函数 — 8 步守卫逐一实施。注意 status 用 `Status::Ok()` 而非 `Status::OK()`（与 application.cc:84 一致）。

**步骤 4 — 验证通过：** `ctest -R FileWatcherTest` → 12+ PASS

**步骤 5 — 反向探针：** 临时删除每步守卫一个一个 → 对应测 FAIL（验证守卫精准捕获）→ 恢复 → 全 PASS

**步骤 6 — 提交：** `git commit -m "feat(devtool): T2 路径穿越 8 步守卫 + 反向探针 (C.1.2)"`

---

### C.1.3 InotifyFileWatcher 完整测试集 [TDD]

**文件：**
- 测试：`tests/devtool/hot_reload/file_watcher_test.cc`（追加 4 测 — atomic write / kInvalidArgument / multiple files / read EAGAIN）

**估时：** plan 75 min / plan ×0.6 = **45 min**

**4 项追加测试：**
- AtomicWriteCoalesced（vim/VSCode 模式：mv `.swp` → `.css` → IN_MOVED_TO + IN_CLOSE_WRITE 重复事件 → 50ms debounce → 单次 callback）
- KInvalidArgumentOnEmptyOptions（empty WatchOptions → Status::kInvalidArgument）
- MultipleFilesIndependentEvents（root 下 2 文件分别修改 → 2 callback events 独立）
- ReadEAGAINSleepFallback（mock inotify_fd_ EAGAIN → sleep_for(50ms) → continue loop，不破坏 thread）

**步骤 5 — 提交：** `git commit -m "test(devtool): InotifyFileWatcher 完整测试集 8 项 (C.1.3)"`

---

### C.2.1 HotReloadManager 基础（含 DOM 状态保留协议合并 — B5=A） [TDD]

**文件：**
- 创建：`veloxa/devtool/hot_reload/hot_reload_manager.h`（~60 行）
- 创建：`veloxa/devtool/hot_reload/hot_reload_manager.cc`（~80 行）
- 测试：`tests/devtool/hot_reload/hot_reload_manager_test.cc`（创建 — 6 测）
- 修改：`veloxa/devtool/inspector/CMakeLists.txt`（target_sources append）
- 修改：`tests/devtool/hot_reload/CMakeLists.txt`（vx_add_test）

**估时：** plan 60 min（合并 B5 节省 27 min）/ plan ×0.6 = **36 min**

**步骤 1 — 编写失败测试（6 项）：**

```cpp
// tests/devtool/hot_reload/hot_reload_manager_test.cc
#include "veloxa/devtool/hot_reload/hot_reload_manager.h"
#include "veloxa/core/application.h"
#include <gtest/gtest.h>

namespace vx::devtool::hot_reload {
namespace {

TEST(HotReloadManagerTest, AttachStartsWatcherAndTracksZeroFiles) { /* ... */ }
TEST(HotReloadManagerTest, OnFileChangedFiltersByExtension) { /* .html should be filtered */ }
TEST(HotReloadManagerTest, DrainEventsTriggersLoadCSS) { /* drain queue + verify stylesheets_++  */ }
TEST(HotReloadManagerTest, DomBindingsPreservedAfterLoadCSS) {
  // R9: F-025 不踩契约反向探针
  vx::Application app(...);
  app.LoadHTML("<html></html>");
  app.LoadScript("var x = 1;");
  ASSERT_NE(app.dom_bindings(), nullptr);
  auto* prev = app.dom_bindings();
  HotReloadManager mgr(&app);
  // simulate .css file change
  mgr.OnFileChanged(FileChangeEvent{.path = "/tmp/test.css", .kind = FileChangeKind::kModified});
  mgr.DrainEvents();
  EXPECT_EQ(app.dom_bindings(), prev);  // 指针不变 — LoadCSS 不动 dom_bindings_
}
TEST(HotReloadManagerTest, HtmlFileChangeDoesNotCallLoadCSSorLoadHTML) {
  // R9: F-025 不踩契约反向探针 — .html 修改 filter 短路
  HotReloadManager mgr(&app);
  mgr.OnFileChanged(FileChangeEvent{.path = "/tmp/test.html", .kind = FileChangeKind::kModified});
  mgr.DrainEvents();
  EXPECT_EQ(app.target_document()->stylesheets_count(), 0);  // LoadCSS 未被调
  // dom_bindings_ 指针不变（LoadHTML 也未被调）
}
TEST(HotReloadManagerTest, TrackedCountReflectsLoadCSSCalls) { /* ... */ }

}  // namespace
}  // namespace vx::devtool::hot_reload
```

**步骤 3 — 编写最少实现：**

```cpp
// veloxa/devtool/hot_reload/hot_reload_manager.h
#ifndef VELOXA_DEVTOOL_HOT_RELOAD_HOT_RELOAD_MANAGER_H_
#define VELOXA_DEVTOOL_HOT_RELOAD_HOT_RELOAD_MANAGER_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include "veloxa/devtool/hot_reload/file_watcher.h"
#include "veloxa/foundation/status/status.h"

namespace vx { class Application; }

namespace vx::devtool::hot_reload {

class HotReloadManager {
 public:
  explicit HotReloadManager(vx::Application* app);
  ~HotReloadManager();

  HotReloadManager(const HotReloadManager&) = delete;
  HotReloadManager& operator=(const HotReloadManager&) = delete;

  Status Attach(StringView root_dir);  // start watcher with root_dir
  void Detach();                       // stop watcher + clear queue

  // === inotify thread context ===
  void OnFileChanged(const FileChangeEvent& evt);  // thread-safe push

  // === main thread context ===
  void DrainEvents();  // pop queue + call app_->LoadCSS

  usize tracked_count() const { return tracked_count_; }
  bool IsAttached() const { return watcher_ && watcher_->IsRunning(); }

 private:
  vx::Application* app_;  // not owned
  std::unique_ptr<FileWatcher> watcher_;
  std::mutex queue_mu_;
  std::queue<FileChangeEvent> queue_;
  usize tracked_count_ = 0;

  friend class HotReloadManagerTest;
};

}  // namespace vx::devtool::hot_reload

#endif
```

```cpp
// veloxa/devtool/hot_reload/hot_reload_manager.cc
#include "veloxa/devtool/hot_reload/hot_reload_manager.h"
#include "veloxa/core/application.h"
#include <fstream>

namespace vx::devtool::hot_reload {

HotReloadManager::HotReloadManager(vx::Application* app) : app_(app) {}

HotReloadManager::~HotReloadManager() { Detach(); }

Status HotReloadManager::Attach(StringView root_dir) {
  Detach();
  watcher_ = FileWatcher::CreatePlatform();
  if (!watcher_) {
    return Status(StatusCode::kUnimplemented, "FileWatcher not supported on this platform");
  }
  WatchOptions opts;
  opts.root_dir = String(root_dir);
  opts.extensions = {String(".css")};
  opts.callback = [this](const FileChangeEvent& evt) { OnFileChanged(evt); };
  return watcher_->Start(std::move(opts));
}

void HotReloadManager::Detach() {
  if (watcher_) watcher_->Stop();  // joins thread
  watcher_.reset();
  std::lock_guard<std::mutex> lk(queue_mu_);
  while (!queue_.empty()) queue_.pop();
}

void HotReloadManager::OnFileChanged(const FileChangeEvent& evt) {
  // CSS-only 严格契约 — filter `.css` 后缀（R9 F-025 不踩）
  if (!evt.path.EndsWith(".css")) return;  // 反向探针单测验证
  std::lock_guard<std::mutex> lk(queue_mu_);
  queue_.push(evt);
}

void HotReloadManager::DrainEvents() {
  std::queue<FileChangeEvent> local;
  {
    std::lock_guard<std::mutex> lk(queue_mu_);
    std::swap(local, queue_);
  }
  while (!local.empty()) {
    const auto& evt = local.front();
    // read file content
    std::ifstream f(evt.path.Cstr());
    if (!f) { local.pop(); continue; }
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    app_->LoadCSS(StringView(content.data(), content.size()));  // R9 F-025 不踩 — 仅调 LoadCSS
    ++tracked_count_;
    local.pop();
  }
}

}  // namespace vx::devtool::hot_reload
```

**步骤 4 — 验证通过：** `ctest -R HotReloadManagerTest` → 6 PASS

**步骤 5 — 反向探针：** 删 OnFileChanged 内 `.css` filter → HtmlFileChangeDoesNotCallLoadCSSorLoadHTML 测 FAIL（验证 F-025 契约捕获）

**步骤 6 — 提交：** `git commit -m "feat(devtool): HotReloadManager + R9 F-025 不踩契约 + DOM 状态保留 YAGNI 合并 (C.2.1)"`

---

### C.2.3 CSS 解析失败错误恢复 [TDD]

**文件：**
- 修改：`veloxa/devtool/hot_reload/hot_reload_manager.cc`（DrainEvents 加 try/catch CssParser::Parse + 错误日志 + 旧 stylesheet 保留）
- 测试：`tests/devtool/hot_reload/hot_reload_manager_test.cc`（追加 2 测）

**估时：** plan 30 min / plan ×0.6 = **18 min**

**注：** creative #2 决策 5 选定方案 C — 保留旧 stylesheet + 错误日志 + 状态指示器（C.3.1 实施）。本子任务仅做错误日志 + 旧 stylesheet 保留（指示器留 C.3.1）。

**核心实施：** Application::LoadCSS 当前直接 `stylesheets_.push_back(css::CssParser::Parse(css))` — 即使 CssParser::Parse 返回空 ParsedStylesheet 也 push。HotReloadManager 在 DrainEvents 内 pre-validate `CssParser::Parse(content).rules.IsEmpty() && content 含非空白字符` → 视为解析失败 → 不调 LoadCSS + VX_LOG_WARN。

**步骤 5 — 提交：** `git commit -m "feat(devtool): CSS 解析失败保留旧 stylesheet + 错误日志 (C.2.3)"`

---

### C.3.1 DevTool UI 状态指示器（"watching: N files" + 错误指示器） [TDD]

**文件：**
- 修改：`veloxa/devtool/inspector_panel.html`（加 `#hot-reload-status` div）
- 修改：`veloxa/devtool/inspector_panel.css`（加 `.status-watching` / `.status-error` 样式）
- 修改：`veloxa/devtool/inspector_panel.js`（加 `updateHotReloadStatus()`）
- 修改：`veloxa/script/dom_bindings.cc`（加 `vx_devtool_get_hot_reload_status` JS native binding 返回 JSON `{watching: N, last_error: ""}`）
- 测试：`tests/devtool/inspector_panel_smoke_test.cc`（追加 2 测）+ `tests/script/devtool_bindings_test.cc`（追加 2 测）

**估时：** plan 30 min / plan ×0.6 = **18 min**

**步骤 5 — 提交：** `git commit -m "feat(devtool): DevTool UI hot reload 状态指示器 (C.3.1)"`

---

### C.4.1 vx_view_attach_devtool 加 hot_reload_dir 参数 [TDD] [安全相关]

**文件：**
- 修改：`veloxa/api/veloxa_api.h`（vx_view_attach_devtool 签名扩展 hot_reload_dir 参数）
- 修改：`veloxa/api/veloxa_api.cc`（实施 hot_reload_dir 透传到 Application + 构造 HotReloadManager）
- 修改：`veloxa/core/application.h`（加 `HotReloadManager& hot_reload_manager()` getter + private 字段）
- 修改：`veloxa/core/application.cc`（构造 HotReloadManager + Attach + Update 时调 DrainEvents）
- 测试：`tests/api/devtool_attach_api_test.cc`（追加 4 测）

**估时：** plan 30 min / plan ×0.6 = **18 min**

**关键：lazy-attach C ABI 容错模式（Phase B B.0.1 范式复用）：**

```cpp
// veloxa/api/veloxa_api.cc
VxResult vx_view_attach_devtool(VxView* view, ..., const char* hot_reload_dir) {
#ifdef VX_BUILD_DEVTOOL
  auto* app = reinterpret_cast<vx::Application*>(view);
  // ... 既有 inspector / overlay attach ...
  if (hot_reload_dir && hot_reload_dir[0] != '\0') {
    auto& mgr = app->hot_reload_manager();
    auto st = mgr.Attach(StringView(hot_reload_dir));
    if (!st.ok()) {
      // T2 守卫拒绝 — 不阻塞 inspector/overlay attach，但返回 warning 码
      return VX_WARNING_HOT_RELOAD_FAILED;  // 新错误码 — 提示但不致命
    }
  }
  return VX_OK;
#else
  return VX_ERROR_UNSUPPORTED;
#endif
}
```

**步骤 5 — 提交：** `git commit -m "feat(api): vx_view_attach_devtool 加 hot_reload_dir + lazy-attach (C.4.1)"`

---

### C.4.2 examples/hello_devtool.cc — Hot Reload smoke [TDD]

**文件：**
- 修改：`examples/hello_devtool.cc`（加 `--hot-reload-dir` cmdline + 临时 CSS 文件 + 修改触发观察）
- 修改：`tests/CMakeLists.txt`（新增 `hello_devtool_hot_reload_smoke` ctest with 500ms autoquit + PASS_REGULAR_EXPRESSION `HOT RELOAD: triggered`）

**估时：** plan 45 min / plan ×0.6 = **27 min**

**实施：** example 第三次扩展 — dogfood 路径成熟。example 内：
1. mkdir `/tmp/vx_hot_reload_smoke_XXXXXX/`（mkstemp）
2. 写入初始 `style.css` 含 `body { background: red; }`
3. 调 `vx_view_attach_devtool(..., hot_reload_dir = tmpdir)`
4. 启动 EventLoop（100 ms 后 modify CSS → `body { background: blue; }`，再 200 ms autoquit）
5. 退出前 print `HOT RELOAD: triggered count=N`（HotReloadManager::tracked_count）

**步骤 5 — 提交：** `git commit -m "feat(devtool): hello_devtool hot reload smoke + dogfood (C.4.2)"`

---

### C.5.1 T2 完整安全单测（独立 security_test.cc） [TDD] [安全相关]

**文件：**
- 测试：`tests/devtool/hot_reload/security_test.cc`（创建 — 8 测对应 §3.3 表 + 8 反向探针 = 16 项）
- 修改：`tests/devtool/hot_reload/CMakeLists.txt`（注册 security_test）

**估时：** plan 45 min / plan ×0.6 = **27 min**

**步骤 1-4：** §3.3 表 1:1 — 8 步守卫每步独立测试 + 反向探针 meta-test

**步骤 5 — 提交：** `git commit -m "test(devtool): T2 完整安全单测 8 步守卫 + 反向探针 (C.5.1)"`

---

### C.5.2 Phase C reflect prep + finalize commit + A14 黑名单更新 [TDD]

**文件：**
- 修改：`tests/smoke/devtool_a14_link_closure.cmake`（黑名单 +3 项 — FileWatcher / InotifyFileWatcher / HotReloadManager + Phase A/B/C 区分注释 + NOT in blacklist intentional 注释段说明 vx_view_attach_devtool 公开 ABI 不入名单）

**估时：** plan 30 min / plan ×0.6 = **18 min**

**步骤：**
1. 全 ctest 双绿 verify（DEVTOOL=ON 1228 + 11 子任务追加 ~30+ 测 → 期望 ~1260+ / DEVTOOL=OFF 1082 + ~少量 stub 测追加 → 期望 ~1085+）
2. A14 黑名单更新 + ctest -R devtool_a14 PASS
3. git log 检视 11 commits 顺序与子任务对齐
4. 提交 finalize commit

**步骤 5 — 提交：** `git commit -m "chore(devtool): finalize Phase C 11/11 + A14 黑名单更新"`

---

## 5. Checkpoint（3 个 — 与 Phase B 同款）

| Checkpoint | 触发条件 | 决策点 |
|:-:|---|---|
| **CP1** | C.1.3 完成（FileWatcher 完整测试集 8+4 测全过）| 是否进入 C.2.x HotReloadManager？暂停审视 watch loop race condition / debounce 是否够 / max_size 实测大文件场景是否真守住 |
| **CP2** | C.4.1 完成（C API attach 路径打通）| 是否进入 C.4.2 example？暂停审视 lazy-attach C ABI 模式实证 + DEVTOOL=OFF 路径返 VX_ERROR_UNSUPPORTED 是否符合预期 |
| **CP3** | C.5.1 完成（T2 8 项安全测全过）| 是否进入 C.5.2 finalize？暂停审视 8 步守卫每项是否真有反向探针 + 是否漏覆盖 §3.3 表任一行 |

---

## 6. systemPatterns 协同度自我对照（≥ 9 条）

| # | systemPatterns 段 | 本任务对照 | 是否合规 |
|:-:|---|---|:-:|
| 1 | 双 UpdateManager 范式（Phase A 沉淀）| HotReloadManager 调 `app_->LoadCSS` → 触发 `update_manager_.reset()` → next Update 重建 — 与 B.0.1 lazy-attach 同款 | ✅ |
| 2 | 双层 API 范式（Phase A 沉淀）| 内部 C++（`HotReloadManager::Attach`）+ 公开 C ABI 薄封装（`vx_view_attach_devtool` 加 hot_reload_dir 参数）| ✅ |
| 3 | #ifdef + CMake conditional 范式（Phase A 沉淀）| `if(VX_BUILD_DEVTOOL) target_sources(vx_devtool PRIVATE hot_reload/...)` + `#ifdef VX_BUILD_DEVTOOL` 包裹 vx_view_attach_devtool 实现 | ✅ |
| 4 | 资源嵌入范式（Phase A 沉淀）| ⊘ 本任务无资源嵌入需求（CSS 文件由 watcher root 用户提供）| ✅ N/A |
| 5 | canvas Translate 范式（Phase A 沉淀）| ⊘ 本任务无 DevTool UI 渲染需求（C.3.1 状态指示器复用 inspector_panel 既有 UI 区域）| ✅ N/A |
| 6 | lazy-attach C ABI 容错模式（Phase B 沉淀）| C.4.1 vx_view_attach_devtool 在 hot_reload_dir = nullptr 时跳过 attach；attach 失败返 VX_WARNING_HOT_RELOAD_FAILED 不阻塞主路径 | ✅ |
| 7 | Phase 0 投入越深 / build phase 越快定律（Phase A/B 双实证）| 13 子段 Phase 0 实测填写（含 §0.7 LoadCSS 路径 100% 安全发现 + §0.12 std::thread 零既有用例发现 + §0.13 realpath 选型决策实证）| ✅ |
| 8 | A14 链接闭包零自动化守门 + 维护演进透明度（Phase A/B 沉淀）| C.5.2 黑名单 +3 项 + Phase A/B/C 区分注释 + NOT in blacklist intentional 排除项清单 | ✅ |
| 9 | 反向探针 D3 类回归测试（systemPatterns 5 次实证）| C.1.2 8 步守卫 + 反向探针 + C.2.1 R9 F-025 不踩契约反向探针（HtmlFileChangeDoesNotCallLoadCSSorLoadHTML） | ✅ |
| 10 | T6 边界场景显式语义状态优于数值阈值（Phase B 沉淀）| HotReloadManager::OnFileChanged filter 用 `evt.path.EndsWith(".css")` 显式语义而非 `evt.kind == kCssOnly` 隐式 enum | ✅ |
| 11 | Level 3 vs L4 子代理决策法则（Phase B 沉淀）| 本任务 Level 3 锁定不 escalate — 11 子任务跨 4 子系统 + 蓝图 plan §Phase C 已就绪 + 5 大可复用范式 100% 命中 + 无 dogfood UI 子系统级 + 无 JS native binding 设计（C.3.1 状态指示器是既有 inspector_panel 扩展非新建）| ✅ |
| 12 | 子系统关闭守门 ctest smoke 范式（Phase A 沉淀）| C.5.2 复用既有 `tests/smoke/devtool_a14_link_closure.cmake` ctest smoke 自动化守门 | ✅ |

---

## 7. plan ×0.6 第 48+ 数据点假设入库

**估时矩阵（11 子任务）：**

| 子任务 | plan | plan ×0.6 | VAN 假设比值 |
|:-:|---:|---:|:-:|
| C.0.1 FileWatcher 抽象 | 30 min | 18 min | 0.30-0.50× |
| C.1.1 InotifyFileWatcher 基础 | 90 min | 54 min | 0.40-0.60× |
| C.1.2 T2 8 步守卫 | 90 min | 54 min | 0.40-0.60× |
| C.1.3 完整测试集 4 项 | 75 min | 45 min | 0.30-0.50× |
| C.2.1 HotReloadManager 基础（合并 B5）| 60 min | 36 min | 0.30-0.50× |
| C.2.3 CSS 解析失败错误恢复 | 30 min | 18 min | 0.30-0.50× |
| C.3.1 DevTool UI 状态指示器 | 30 min | 18 min | 0.40-0.60×（跨 vx_devtool ↔ vx_script binding）|
| C.4.1 vx_view_attach_devtool 扩展 | 30 min | 18 min | 0.30-0.50× |
| C.4.2 example smoke | 45 min | 27 min | 0.30-0.50× |
| C.5.1 T2 完整安全单测 | 45 min | 27 min | 0.30-0.50× |
| C.5.2 finalize + A14 黑名单 | 30 min | 18 min | 0.20-0.40× |
| **合计** | **555 min** | **333 min（5.55 h）** | **~0.30-0.45×（落「极窄档延续高效区」候选新子档 — Phase B 同档实证 0.40×）** |

**最终 VAN 假设：** **~100-150 min（1.7-2.5 h）实测耗时** — 比蓝图 ~6 h 低 ~70%，比 Phase B archive ~30% 下调进一步实证。

---

## 8. 后续步骤 / 立项交接

**Plan 完成后：**
1. 用户审查本 plan 文档
2. 进入 `/build` 阶段 — 启动 C.0.1 FileWatcher 抽象（最小子任务 plan 30 min ×0.6 = 18 min）
3. 严格按 11 子任务串行执行（B2=A 锁定）+ 每子任务 1 commit（B6=A 锁定）
4. CP1/CP2/CP3 三 Checkpoint 自动暂停 — 由用户决策继续/调整

**预期产出：**
- 新子系统：`vx::devtool::hot_reload::{FileWatcher, InotifyFileWatcher, HotReloadManager}`
- 1 公开 C API 扩展：`vx_view_attach_devtool` 加 hot_reload_dir 参数
- 1 example 扩展：hello_devtool.cc 加 `--hot-reload-dir` 参数 + smoke
- 11 commits + 1 finalize + reflect/archive
- ctest DEVTOOL=ON ~1260+ / DEVTOOL=OFF ~1085+
- A14 黑名单 +3 项

**DevTool 三件套主线收官** — Phase A → B → C 完整闭环；后续候选见 activeContext.md 待处理事项。
