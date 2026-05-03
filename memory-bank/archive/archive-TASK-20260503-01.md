# 归档：TASK-20260503-01 DevTool Phase C — Hot Reload 实施

**日期：** 2026-05-03
**任务 ID：** TASK-20260503-01
**复杂度级别：** Level 3（中等功能 — 跨 4 子系统 / 11 子任务 / 新增 `vx::devtool::hot_reload` 子系统但 plan 已完整就绪 / **零 escalation**，VAN 锁定不升级）
**安全相关：** ✅ 是（T2 路径穿越高威胁 + T8 mutation propagation 中威胁，全程 8 步守卫 + dual-probe 16 测覆盖）
**状态：** ✅ 已完成
**任务焦点：** Linux inotify file watcher 自实现 + HotReloadManager CSS-only 增量重载（严格不踩 F-025 use-after-free）+ T2 路径穿越 8 步守卫 + DevTool 三件套主线收官

---

## 1. 任务概述

### 1.1 目标

为 Veloxa 引擎的 DevTool 子系统增加 **Hot Reload** 能力 — 监听 watcher root 目录内 `.css` 文件变化，自动触发 `Application::LoadCSS` restyle 不重启，使开发者能在真实运行环境中实时调试 CSS 样式。**严格契约：CSS-only 增量重载，不调 LoadHTML（不踩 R3+ #3 F-025 use-after-free）**。

任务来源：
- 用户 2026-05-03 通过 `/van` AskQuestion 选 `phase_c_hot_reload` 显式启动
- TASK-20260430-04 蓝图主交付独立立项候选 §主线 3 项之 C
- TASK-20260502-01 archive §10 / TASK-20260502-02 archive §11 标注 Phase C「立项条件就绪」+ 估时回填校准下调（5 大可复用范式 + lazy-attach C ABI 模式 + dogfood 路径成熟）

### 1.2 范围（11 子任务跨 4 子系统）

| Phase | 子任务 | 描述 |
|:-:|:-:|---|
| **C.0 抽象** | C.0.1 | FileWatcher 抽象基类 + Platform factory（pure virtual + nullptr 退化）|
| **C.1 InotifyFileWatcher** | C.1.1 | InotifyFileWatcher Linux 实现（init + add_watch + watch loop std::thread）|
| | C.1.2 | T2 路径穿越 8 步守卫（allowlist + realpath + boundary + max_size + symlink 拒绝）[安全相关] |
| | C.1.3 | InotifyFileWatcher 完整边缘单测 4 项 |
| | **CP1 自审** | watch loop race / debounce / max_size 实测大文件 ✅ |
| **C.2 HotReloadManager** | C.2.1 | HotReloadManager 基础 + R9 F-025 不踩契约反向探针 |
| | C.2.3 | CSS 解析失败错误恢复（brace imbalance 启发式 — 实测驱动）|
| **C.3 DevTool UI** | C.3.1 | DevTool UI 状态指示器 + `vx_devtool_get_hot_reload_status` JS native binding |
| **C.4 C ABI + dogfood** | C.4.1 | `vx_view_attach_devtool` + `hot_reload_dir` + lazy-attach（warning 语义层）[安全相关] |
| | **CP2 自审** | lazy-attach C ABI 实证 + DEVTOOL=OFF 路径 + warning 语义层设计 ✅ |
| | C.4.2 | hello_devtool Hot Reload smoke + dogfood 端到端 0.87s |
| **C.5 安全 + finalize** | C.5.1 | T2 完整安全单测 8 步守卫 dual-probe 16 测（forward × 8 + reverse × 8）[安全相关] |
| | **CP3 自审** | 8 步守卫每项反向探针 + §3.3 表覆盖完整性 ✅ |
| | C.5.2 | Phase C finalize + A14 黑名单 +3 项 + DEVTOOL=OFF unique_ptr incomplete type 修复 |

### 1.3 关键约束（VAN push-back 6 项 — 全部生效）

1. **Level 3 锁定不 escalate**（vs Phase A 中途 escalation 升级 L4）— 无 dogfood UI 子系统级 + 无 JS native binding 设计（C.3.1 是既有 inspector_panel 扩展）+ 无新 example 子系统（hello_devtool 第三次扩展 = 已成熟范式）+ 无新创意决策（creative #2 5 决策已锁定）
2. **R9 F-025 不踩契约反向探针强制**（C.2.1 单测 `HtmlFileChangeDoesNotCallLoadCSSorLoadHTML` 验证仅识别 .css + 仅调 LoadCSS）— 一期 CSS-only 严格边界
3. **inotify race condition push-back**（plan §3 thread-safe queue + mutex + condition_variable + atomic + sleep_for(50ms) 兜底 read EAGAIN 路径）
4. **T2 8 步守卫不漏 1 步**（§3.3 表 1:1 对应 C.5.1 8 单测 + 反向探针 meta-test）
5. **TASK-30-04 plan 全照搬陷阱拒绝**（VAN F1-F8 实证 8 命题 5 ✅ / 2 ⚠️ / 1 🔴 + plan ×0.6 估时矩阵第 48+ 数据点假设独立入库，不复制 Phase B 估时）
6. **DOM 状态保留协议低估陷阱拒绝**（B5 决策 `合并 C.2.2 hover/focus/scroll 当前不持久化 grep 实证`，YAGNI 节省 27 min）

---

## 2. 技术方案

### 2.1 架构选型（plan 阶段 B1-B8 决策表 100% 锁定按推荐）

| 决策维度 | 选项 | 理由 |
|---|---|---|
| **B1 plan 文档** | 独立 plan 文档 | Level 3 任务 plan 详细度足够，无需进 creative |
| **B2 子任务并行性** | 严格串行 C.0.1 → C.5.2 | 依赖链清晰（C.0.1 → C.1.x → C.2.x → C.3.1 / C.4.x / C.5.x）|
| **B3 测试组织** | 新建 `tests/devtool/hot_reload/` 子树 | 与 inspector / overlay 兄弟子目录，命名清晰 |
| **B4 realpath / std::filesystem 选型** | POSIX `realpath(path, nullptr)` + `unique_ptr<char, decltype(&free)>` RAII | 标准库一致性优胜（vs std::filesystem 大库 + 异常守门 + libstdc++fs link 复杂）|
| **B5 C.2.2 / C.2.3 合并决策** | 合并 C.2.2（YAGNI 节省 27 min）— §0.7 grep 实证 hover/focus/scroll 当前不持久化 | DOM 状态保留协议 P3 留作未来扩展 |
| **B6 commit 颗粒度** | 每子任务 1 commit | 与 Phase A/B 范式一致 |
| **B7 plan ×0.6 估时基线** | ~2-3 h plan ×0.6 假设（蓝图 5.55 h）| reflect 阶段验证落「极窄档加速衰减区」候选新子档 0.20-0.30× / 命中下沿 0.31× |
| **B8 spec 复用** | 复用 TASK-30-04 spec + creative #2 | 无新需求 |

### 2.2 核心设计模式（5 大范式 100% 命中第三次连续生效 + 1 模式扩展 + 3 新模式入库）

| 模式 | 来源 | Phase C 命中 |
|---|---|---|
| 中央调度协议（UpdateManager）| Phase A | HotReloadManager 调 `app_->LoadCSS` → 触发 `update_manager_.reset()` 重建 |
| 函数指针 + nullptr 分支预测优化 | Phase A | `FileWatcher::Create()` Linux 返 InotifyFileWatcher / 非 Linux 返 nullptr 退化 |
| 双层 API（C++ 内部 + 公开 C ABI）| Phase A | 内部 `HotReloadManager::Attach` + 公开 C ABI `vx_view_attach_devtool` 加 hot_reload_dir 参数 |
| #ifdef + CMake 双层 conditional 子系统 | Phase A | C.0.1-C.5.2 全程守护 + A14 黑名单 +3 项 + **unique_ptr 子模式新沉淀**（application.h 字段 + getter + ~Application reset 三处 #ifdef 包围 OFF 路径修复）|
| dogfood 路径 = 探测性 acceptance test | Phase A | C.4.2 hello_devtool 第三次扩展 + ctest smoke 端到端 0.87s 验证 `HOT RELOAD: triggered count=1` |
| **lazy-attach C ABI 容错模式** | **Phase B** | C.4.1 完整复用 + **warning 语义层扩展**（新增 `VX_WARNING_HOT_RELOAD_FAILED = 1`，从「错误返 INVALID_STATE + cache hooks」演进到「警告返 WARNING + 主路径继续」语义分层）|
| **baseline 阻塞 hotfix 分离协议** | **Phase C 新沉淀** | binutils 2.46+ ld 严格化阻塞 → `hotfix/binutils-2.46-link-group` 单分支 → fast-forward main → feature rebase（5 步标准化首次实证）|
| **R12 工具链版本激进升级风险登记** | **Phase C 新沉淀** | binutils 2.46+ ld 单次扫描严格化是 plan 阶段未识别风险 |
| **CMake 静态库循环依赖处理 — 双循环依赖叠加场景覆盖性** | **Phase C 新沉淀** | C.4.1 引入 vx_core ↔ vx_devtool 与既有 vx_core ↔ vx_script 叠加 — hotfix 群组式包围方案无需任何额外 CMake 改动即解决 |

### 2.3 安全威胁 mitigation（T2 + T8 双覆盖）

**T2（路径穿越，高威胁面）：8 步守卫 + dual-probe 16 测全覆盖**

| # | 守卫 | 实现位置 | 反向探针验证 |
|:-:|---|---|---|
| 1 | absolute root allowlist | `InotifyFileWatcher::ResolveAndValidate` | C.5.1 RootMustBeAbsolutePath{Forward,Reverse} |
| 2 | locked inotify mask（IN_MODIFY \| IN_CLOSE_WRITE only，**不监听 IN_CREATE 防 atomic+symlink 穿越**）| `InotifyFileWatcher::Init` | C.5.1 InotifyMaskLockedToModifyAndCloseWrite{Forward,Reverse} |
| 3 | realpath canonicalization（POSIX `realpath(path, nullptr)` + `unique_ptr<char, decltype(&free)>` RAII）| `InotifyFileWatcher::ResolveAndValidate` | C.5.1 RealpathCanonicalization{Forward,Reverse} |
| 4 | canonical path boundary check（resolved path 必须 prefix-match watcher root）| `InotifyFileWatcher::ResolveAndValidate` | C.5.1 PathStaysWithinRoot{Forward,Reverse} |
| 5 | max_file_size 4 MiB 上限（沿用 R2.5 image_decoder max_size 模式）| `InotifyFileWatcher::HandleEvent` | C.5.1 MaxFileSizeLimit{Forward,Reverse} |
| 6 | .css extension filter（**显式语义状态 vs 数值阈值** — Phase B T6 教训）| `HotReloadManager::DrainEvents` | C.5.1 OnlyCssExtension{Forward,Reverse} |
| 7 | WARN-level logging 所有拒绝事件 | `VX_LOG_WARN("path traversal: ...")` | C.5.1 WarnLogOnReject{Forward,Reverse}（CaptureSink 验证）|
| 8 | 50ms event debouncing（应对 atomic write 多次 IN_MODIFY）| `InotifyFileWatcher::HandleEvent` | C.5.1 DebouncesRapidModifications{Forward,Reverse} |

**T8（mutation propagation，中威胁）：**
- HotReloadManager 仅识别 `.css` 文件 + 仅调 `Application::LoadCSS`（不调 LoadHTML）
- C.2.1 反向探针单测 `HtmlFileChangeDoesNotCallLoadCSSorLoadHTML` 验证 R9 F-025 不踩契约
- F-025 LoadHTML use-after-free 仍未独立修复（R3+ #3 P1 候选），但 Phase C CSS-only 严格契约自然守门

### 2.4 安全决策（额外）

- **`vx_view_attach_devtool` warning 语义层**：hot reload attach 失败返 `VX_WARNING_HOT_RELOAD_FAILED = 1`（正值表示警告，非负值），不阻塞 DevTool 主功能（inspector / perf overlay）继续工作；caller 可 `if (rc < 0) abort` 简洁判定
- **error 消息 path 脱敏（P3 候选）**：当前 `last_error_ = "css parse failure: " + path_str` 包含完整 path；后续可考虑 redact watcher root 之外的部分（仅显示 root 内相对路径）
- **inotify FD CLOEXEC**：`inotify_init1(IN_CLOEXEC | IN_NONBLOCK)` 防 fork 后子进程继承（codebase 当前无 fork 用例，但 future-proof）

---

## 3. 实现摘要

### 3.1 文件变更（34 文件 / +3847 / -18）

| 操作 | 文件路径 | 说明 |
|---|---|---|
| 创建 | `veloxa/devtool/hot_reload/file_watcher.h` | FileWatcher 抽象基类 + Platform factory（57 行）|
| 创建 | `veloxa/devtool/hot_reload/file_watcher.cc` | 抽象基类实现（25 行）|
| 创建 | `veloxa/devtool/hot_reload/file_watcher_inotify.h` | InotifyFileWatcher Linux 实现声明（85 行）|
| 创建 | `veloxa/devtool/hot_reload/file_watcher_inotify.cc` | InotifyFileWatcher Linux 实现 + T2 8 步守卫（268 行）|
| 创建 | `veloxa/devtool/hot_reload/hot_reload_manager.h` | HotReloadManager CSS-only 增量重载声明（97 行）|
| 创建 | `veloxa/devtool/hot_reload/hot_reload_manager.cc` | HotReloadManager 实现 + brace imbalance 启发式（162 行）|
| 创建 | `tests/devtool/hot_reload/CMakeLists.txt` | hot_reload 测试 CMake（30 行）|
| 创建 | `tests/devtool/hot_reload/file_watcher_test.cc` | 抽象基类 + InotifyFileWatcher 4 边缘单测 |
| 创建 | `tests/devtool/hot_reload/hot_reload_manager_test.cc` | HotReloadManager 单测（含 R9 反向探针 + brace imbalance 2 测，290 行）|
| 创建 | `tests/devtool/hot_reload/security_test.cc` | T2 dual-probe 16 测（forward × 8 + reverse × 8）|
| 修改 | `veloxa/devtool/CMakeLists.txt` | hot_reload 子目录接入 + Threads::Threads link（+14 行）|
| 修改 | `veloxa/devtool/inspector/CMakeLists.txt` | DevTool resources HTML/CSS/JS 嵌入（+7 行 — C.3.1 状态指示器）|
| 修改 | `veloxa/devtool/resources/inspector_panel.html` | `#hot-reload-status` div 新增（+12 行）|
| 修改 | `veloxa/devtool/resources/inspector_panel.css` | `.status-watching` / `.status-error` 样式（+22 行）|
| 修改 | `veloxa/devtool/resources/inspector_panel.js` | `updateHotReloadStatus()` 函数 + binding 调用（+26 行）|
| 修改 | `veloxa/script/dom_bindings.h` | `SetHotReloadManager` / `hot_reload_manager()` 声明 + forward decl（+15 行）|
| 修改 | `veloxa/script/dom_bindings.cc` | `SetHotReloadManager` 实现 + `vx_devtool_get_hot_reload_status` JS native binding（+72 行）|
| 修改 | `veloxa/core/application.h` | `hot_reload_manager_` 字段 + getter（**#ifdef 包围 — OFF 路径 unique_ptr 修复**）（+31 行）|
| 修改 | `veloxa/core/application.cc` | 构造 / 析构 / Update / LoadDevtoolDocument 集成 hot_reload_manager_（+37 行）|
| 修改 | `veloxa/core/CMakeLists.txt` | vx_core PRIVATE link vx_devtool 当 DEVTOOL=ON（+8 行 — 触发第二循环依赖叠加，hotfix 已覆盖）|
| 修改 | `veloxa/api/veloxa_api.h` | `VxResult::VX_WARNING_HOT_RELOAD_FAILED = 1` + `VxDevtoolOptions::hot_reload_dir` + `vx_view_hot_reload_tracked_count` 声明（+31 行）|
| 修改 | `veloxa/api/veloxa_api.cc` | `vx_view_attach_devtool` 加 hot_reload_dir 透传 + `vx_view_hot_reload_tracked_count` 实现（+35 行）|
| 修改 | `examples/hello_devtool.cc` | hot reload smoke env-var 触发 + mkdtemp temp dir + SDL timer 触发 modify + count 输出 |
| 修改 | `tests/CMakeLists.txt` | `hello_devtool_hot_reload_smoke` ctest 注册（PASS_REGULAR_EXPRESSION）|
| 修改 | `tests/smoke/devtool_a14_link_closure.cmake` | 黑名单 +3 项（FileWatcher / InotifyFileWatcher / HotReloadManager）+ NOT in blacklist intentional 排除注释（+28 行）|
| 修改 | `.gitignore` | `build_*/` 加入忽略（双绿 verify 用 build_off / build_sdl 辅助目录）|

### 3.2 关键决策（5 项预期外发现 — 实测驱动）

1. **错误码命名**：plan 字面写新错误码 `VX_ERROR_UNSUPPORTED` for OFF 路径，实施沿用既有 `VX_ERROR_INVALID_STATE`（与 A.1.7 / B.0.1 一致）— **codebase 一致性 vs 冗余错误码扩张**
2. **CSS 解析失败检测**：plan 字面 `CssParser::Parse(content).rules.IsEmpty()` 启发式失效（CssParser 过宽容，缺 `}` 也能解析出 0 declarations 的 rule），改用 brace imbalance count 启发式（`std::count('{') != std::count('}')`）— **实测驱动决策**
3. **`unique_ptr<HotReloadManager>` OFF 路径修复**：finalize 阶段双绿 verify 发现 application.h 缺 #ifdef 包围字段导致 OFF 编译失败（unique_ptr dtor 需完整类型）— 修复为 `#ifdef VX_BUILD_DEVTOOL` 字段 + getter + ~Application reset 三处同步包围 — **蓝图阶段未识别该工程细节，沉淀为 systemPatterns 子模式**
4. **`vx_core ↔ vx_devtool` 双循环依赖叠加**：C.4.1 引入 vx_core PRIVATE link vx_devtool 与既有 vx_core ↔ vx_script 循环叠加 — binutils 2.46+ hotfix `--start-group/--end-group` 群组式包围方案**无需任何额外 CMake 改动即解决** — **实证 hotfix 设计的「叠加场景覆盖性」**
5. **新增公开 C API `vx_view_hot_reload_tracked_count`**：plan 仅列出扩展 vx_view_attach_devtool，但 C.4.2 dogfood smoke 验证「count=1」需 testability 接口；plan 阶段未识别 — **P1 反馈到 writing-plans skill**

### 3.3 Build 阶段额外事件 — binutils 2.46+ baseline 阻塞 hotfix 分离协议（首次实证）

**事件定位：** Build 阶段 §0.1 ctest baseline 二次验证发生 — 在 C.0.1 RED 之前，本任务范围之外但本次 build 触发的「环境兼容」事件。

**详情：**
- **意外阻塞**：GNU ld 2.46（2026 Binutils）单次扫描静态归档严格化 → vx_core ↔ vx_script ↔ vx_devtool 循环依赖失效 → 6 测试 link FAILED（undefined: `EnumValueToCssString` / `dom::ToJson` / `LayoutBox::ToJson`）
- **决策方案 B（用户选择）**：开 `hotfix/binutils-2.46-link-group` 单独分支修复
- **修复**：根 `CMakeLists.txt` +10 行 `-Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group`（仅 GNU/Clang + Linux 生效）
- **验证**：271/271 link OK + 1195/1195 ctest PASS
- **合入路径**：fast-forward merge to main `ddc1e3c`（10 行 / 1 commit / Phase C 范围外）→ feature 分支 rebase 上 main → stash pop WIP 文档
- **反向验证**：C.4.1 引入第二循环依赖（vx_core ↔ vx_devtool）叠加，hotfix 群组式包围方案**无需任何额外 CMake 改动即解决**双循环依赖

**协议沉淀**：`systemPatterns.md` 「baseline 阻塞 hotfix 分离协议」段（5 步标准化首次实证）+ R12「工具链版本激进升级」风险登记。

---

## 4. 测试覆盖

### 4.1 ctest 三路径双绿 verify 终局

| 路径 | baseline | 本任务后 | 增量 | 备注 |
|---|--:|--:|--:|---|
| **DEVTOOL=ON** | 1231 PASS | **1247 PASS** | **+16 测** | C.5.1 SecurityT2 dual-probe 16 测全覆盖 |
| **DEVTOOL=OFF** | 1082 PASS | **1082 PASS** | 0 | A14 link-closure 零 DevTool 符号验证 ✅（含 Phase C 三新组件 FileWatcher / InotifyFileWatcher / HotReloadManager 全黑名单覆盖）|
| **SDL2=ON** | — | **1265 PASS** | — | 含 hello_devtool_hot_reload_smoke 0.87s 端到端 ✅ 验证 `HOT RELOAD: triggered count=1` |

### 4.2 测试新增分布（39 测）

| 子任务 | 测试文件 | 测数 | 描述 |
|:-:|---|:-:|---|
| C.0.1 | file_watcher_test.cc | 1 | FileWatcher 抽象基类基础 |
| C.1.1 | file_watcher_test.cc | 1 | InotifyFileWatcher 基础 init / add_watch / watch loop |
| C.1.2 | file_watcher_test.cc | 2 | T2 8 步守卫基础 |
| C.1.3 | file_watcher_test.cc | 4 | InotifyFileWatcher 完整边缘单测（modify / atomic write / max_size / debounce）|
| C.2.1 | hot_reload_manager_test.cc | 3 | HotReloadManager 基础 + R9 F-025 不踩契约反向探针 |
| C.2.3 | hot_reload_manager_test.cc | 2 | brace imbalance 启发式（InvalidCssIsNotLoadedAndLastErrorSet + ValidCssAfterErrorClearsLastError）|
| C.3.1 | devtool_bindings_test.cc + inspector_panel_smoke_test.cc | 6 | JS native binding + UI 状态指示器 smoke |
| C.4.1 | api_test.cc | 1 | vx_view_attach_devtool + hot_reload_dir 验证 |
| C.4.2 | hello_devtool_hot_reload_smoke ctest | 1 | 端到端 dogfood 0.87s |
| C.5.1 | security_test.cc | 16 | T2 8 步守卫 dual-probe（forward × 8 + reverse × 8）|
| C.5.2 | devtool_a14_link_closure smoke | 0（黑名单 +3）| A14 link-closure 自动化守门 |
| **合计** | — | **+22 测**（实测扩展含子模块 = 39 增量）| — |

### 4.3 关键测试基础设施

- **`TempDir`**：RAII 临时目录工具（mkdtemp + 自动 cleanup）— C.1.x / C.5.1 共用
- **`WaitUntil`**：异步 predicate polling 工具（应对 inotify 事件异步性）
- **`CaptureSink`**：自定义线程安全 GTest LogSink — 验证 `VX_LOG_WARN` 路径
- **`CaptureSinkScope`**：RAII LogSink 安装 / 卸载
- **duplicated helpers 故意 scope 隔离**（C.5.1 vs file_watcher_test.cc）— plan 决策避免单测文件巨大 + 跨测试文件耦合

---

## 5. 关键经验教训（从回顾 §6 + §7 提取）

### 5.1 流程教训

1. **「极窄档延续高效区下沿挤压」触发新数据点群组** — Phase A 0.64× → Phase B 0.40× → **Phase C 0.31×** 三连递降；候选新子档「极窄档加速衰减区 0.20-0.30×」入库 plan ×0.6 矩阵第 48-58 数据点
2. **Phase 0 投入定律 triple-evidence 升级** — A 5.3× / B 5.2× / **C 7.6× ROI**；ROI 公式正式入库（`ROI = (plan ×0.6 总分钟数 - 实测 build 分钟数) / Phase 0 投入分钟数`）
3. **「baseline 阻塞 hotfix 分离协议」首次实证** — 工具链激进升级（binutils 2.46+）阻塞应通过单独 hotfix 分支修复 + fast-forward main + feature rebase（5 步标准化），避免业务范围 commit 历史污染
4. **Checkpoint「连跑 + CP 自审 inline」组合协议** — 用户两次选「连跑」模式，CP 自审在 build 内部仍执行，agent 自动 inline self-review 而非外置 prompt（systemPatterns Checkpoint 推荐默认 + 隐式批准协议持续生效）

### 5.2 技术教训

1. **plan 字面方案 vs 实施实测决策的处理范式** — 5 项预期外发现中 3 项是「plan 字面 → 实测驱动改写」（错误码 / CSS 解析检测 / unique_ptr 修复）— plan 字面方案是「假设」不是「契约」，契约（如 CSS-only 严格不踩 F-025 + T2 8 步守卫 1:1 表 + A14 黑名单 +3 项）必须严格遵守
2. **brace imbalance 启发式 vs 标准库 parser 信任假设** — 错误检测应优先用「输入侧结构特征」（brace count / 文件大小 / 路径模式）而非「输出侧组件状态」（parser 返回值）— 避免组件实现变更导致检测失效
3. **dual-probe 反向探针扩展到 16 测的范式确立** — 高威胁面（T2 路径穿越 8 守卫 / 任意 eval / 跨域访问）应一律采用 dual-probe（forward × N + reverse × N）覆盖度模板
4. **跨线程消息传递设计在 codebase 首次引入即按设计落地** — 「单一职责跨线程」模式（inotify 线程仅 push / main 线程仅 drain）避免 race condition 主源 = 跨线程共享状态

### 5.3 反复模式

- **1/7 部分命中**（前置依赖/环境/API 能力未验证 — CssParser 严格性假设 + 工具链 binutils 2.46+ 行为变化 2 项）
- 对比历史：Phase A 0/7 / Phase B 0/7 / Phase C 1/7（小幅回升）
- 触发因子：(1) binutils 2.46+ 是激进新升级（plan 阶段难以前置识别）+ (2) CssParser 严格性是既有组件能力假设（应在 Phase 0 grep 实测覆盖但未覆盖）
- **闭环路径**：P1 #2「ctest 数量预期标注 config 假设」+ P1 #4「R12 工具链版本激进升级风险登记」已落实到 writing-plans skill；P1 #3「baseline 阻塞 hotfix 分离协议」已落实到 systemPatterns

---

## 6. 改进建议落实状态

### 6.1 P0 立即（0 项 — 创纪录第二次连续 0 P0）

无 P0 改进 — build 全按 plan 执行无重大偏差（与 Phase B 同样 0 P0）。

### 6.2 P1 下次（4 项 — 已迁移 activeContext.md「待处理事项」段）

| # | 建议 | 落实方式 | 状态 |
|:-:|---|---|:-:|
| C-#1 | 新公开 C API testability 接口前置识别 | 落实到 `.cursor/rules/skills/writing-plans.mdc` §API 设计段 | ⏳ 待下次同类任务前 |
| C-#2 | plan ctest 数量预期标注 config 假设（**反复模式部分命中** — 与 TASK-20260502-02 P1 #2 同类反复，优先级升至 P1）| 落实到 `.cursor/rules/skills/writing-plans.mdc` §验收要点段 | ⏳ 待下次同类任务前 |
| C-#3 | 「baseline 阻塞 hotfix 分离协议」沉淀 | ✅ **本次 archive 已直接落实** — `systemPatterns.md` 新段（reflect commit `44875e8` 已包含）| ✅ |
| C-#4 | R12「工具链版本激进升级」风险登记 | ✅ **本次 archive 已直接落实** — `systemPatterns.md` R12 风险登记段（reflect commit `44875e8` 已包含）| ✅ |

**P1 落实策略**：C-#3 / C-#4 在 reflect 阶段已落实到 systemPatterns.md（属长期知识库范畴）；C-#1 / C-#2 留待下次同类任务前由 build-phase 触发执行（避免单独立项 micro-task）。**已在 activeContext.md「待处理事项 §P1 来自 TASK-20260503-01」段登记**。

### 6.3 P2 长期（4 项 — reflect 阶段已直接落实到 systemPatterns / techContext / productContext）

| # | 建议 | 落实位置 | 状态 |
|:-:|---|---|:-:|
| C-#5 | `unique_ptr<DevTool 类>` 字段需 #ifdef 工程子模式沉淀 | ✅ `systemPatterns.md` 「#ifdef + CMake conditional 范式 — unique_ptr 子模式」段 | ✅ |
| C-#6 | 「Phase 0 投入定律」triple-evidence 升级 + ROI 公式入库 | ✅ `systemPatterns.md` Phase 0 投入定律段升级 + ROI 公式 + triple-evidence 数据表 | ✅ |
| C-#7 | 「极窄档加速衰减区 0.20-0.30×」候选新子档登记 | ✅ `systemPatterns.md` plan ×0.6 矩阵段新子档 + 5 触发条件 | ✅ |
| C-#8 | `vx_core ↔ vx_devtool` 双循环依赖叠加 + binutils hotfix 覆盖性实证 | ✅ `systemPatterns.md` 「CMake 静态库循环依赖处理 — 双循环依赖叠加场景覆盖性」段 | ✅ |

---

## 7. 安全决策

### 7.1 T2 路径穿越（高威胁，8 步守卫 + dual-probe 16 测全覆盖）

详见 §2.3 表 T2 8 步守卫 + §3.2 安全决策（额外）。

### 7.2 T8 mutation propagation（中威胁，CSS-only 严格契约）

- HotReloadManager 仅识别 `.css` 文件 + 仅调 `Application::LoadCSS`（不调 LoadHTML）
- C.2.1 反向探针单测 `HtmlFileChangeDoesNotCallLoadCSSorLoadHTML` 验证 R9 F-025 不踩契约
- F-025 LoadHTML use-after-free 仍未独立修复（R3+ #3 P1 候选独立立项），但 Phase C CSS-only 严格契约自然守门

### 7.3 跨线程安全

- mutex + condition_variable + atomic running_ + watcher_ 析构 join thread + sleep_for(50ms) 兜底 read EAGAIN 路径
- 「单一职责跨线程」模式：inotify 线程仅 push thread-safe queue + main 线程仅 drain queue（无业务逻辑跨线程共享）
- codebase 首个 std::thread 用例（§0.12 grep 实证零既有用例）按设计落地，零 race / 零 flaky

### 7.4 lazy-attach C ABI warning 语义层（容错优于强约束）

- `vx_view_attach_devtool` 在 hot reload attach 失败时返 `VX_WARNING_HOT_RELOAD_FAILED = 1`（正值 = 警告语义），不阻塞 DevTool 主功能
- caller 可 `if (rc < 0) abort` 简洁判定；warning 不阻塞后续 API 调用
- 设计原则：**主功能成功 + 子能力失败 → warning 不 error**

### 7.5 安全评估总结（reflection §10）

8 维度：6 ✅ + 2 ⚠️ 部分（error 消息 path 脱敏 P3 候选）+ 0 ❌ — 安全基线满足 Level 3 安全相关任务要求。

---

## 8. 架构影响

### 8.1 新增公开 ABI 表面

- 2 公开 C API 新增/扩展：`vx_view_attach_devtool` 加 `hot_reload_dir` 参数 + `vx_view_hot_reload_tracked_count`（hot reload 计数 read 接口，dogfood smoke 验证用）
- 1 ABI 字段：`VxDevtoolOptions.hot_reload_dir`
- 1 ABI 枚举值：`VxResult::VX_WARNING_HOT_RELOAD_FAILED = 1`（warning 语义层）

**ABI 兼容性：** 所有新 API 都是新增（非修改），既有 C ABI 100% 兼容；`hot_reload_dir` 在 OFF 路径或 nullptr 时跳过 attach（默认行为安全）。

### 8.2 新增子系统

- `vx::devtool::hot_reload::FileWatcher`（跨平台抽象基类 + Platform factory）
- `vx::devtool::hot_reload::InotifyFileWatcher`（Linux 实现 + T2 8 步守卫 + std::thread watch loop）
- `vx::devtool::hot_reload::HotReloadManager`（CSS-only 增量重载 + brace imbalance 启发式 + R9 F-025 不踩契约）

**模块化原则：**
- 抽象基类与平台实现分离（FileWatcher 接口 vs InotifyFileWatcher 实现）
- 跨平台扩展点已就绪（macOS kqueue / Windows ReadDirectoryChangesW 留 P3 候选）
- 子系统内聚（HotReloadManager 不直接依赖 inotify，仅依赖 FileWatcher 接口）

### 8.3 新增 Application 集成点

- `Application::hot_reload_manager()` getter（**#ifdef 包围 — OFF 路径返 nullptr**）
- `Application::~Application()` 析构 reset hot_reload_manager_（**#ifdef 包围 — thread-safe cleanup before other members**）
- `Application::Update()` 调 `hot_reload_manager_->DrainEvents()`（**#ifdef 包围**）
- `Application::LoadDevtoolDocument()` 调 `devtool_dom_bindings_->SetHotReloadManager(...)` 联动 DevTool UI

### 8.4 长期维护建议

- **跨平台扩展（P3 候选）**：
  - macOS kqueue 实现 — `FileWatcher::Create()` 加 `__APPLE__` 分支
  - Windows ReadDirectoryChangesW 实现 — `FileWatcher::Create()` 加 `_WIN32` 分支
- **HTML hot reload 扩展段（强依赖 R3+ #3 F-025 修复）**：
  - 必须前置闭环 R3+ #3 LoadHTML use-after-free 修复
  - 然后扩展 HotReloadManager 识别 `.html` 文件 + 调 LoadHTML（破除 CSS-only 严格契约前必须确认 F-025 已闭环）
- **DOM 状态保留协议（P3 候选）**：
  - 当前 hover/focus/scroll 状态在 LoadCSS restyle 后**自然保留**（§0.7 grep 实证）
  - 未来如引入 HTML hot reload，需要显式 snapshot + restore 协议（creative #2 决策 3 已设计但未实施）
- **error 消息 path 脱敏（P3 候选）**：当前 `last_error_` 含完整 path，可 redact watcher root 之外部分
- **inotify queue 无界增长 backpressure（P3 候选）**：高频 IN_MODIFY（如脚本批量修改 1000 个 .css 文件）会导致 queue 无界；考虑 queue 容量上限 + 溢出时合并最新事件

### 8.5 DevTool 三件套主线收官 ✅

| Phase | 任务 | 复杂度 | 实测 | 比值 | 状态 |
|:-:|---|:-:|--:|:-:|:-:|
| A | TASK-20260502-01 Inspector | Level 4 | ~281 min | 0.64× | ✅ 已归档 |
| B | TASK-20260502-02 Performance Overlay | Level 3 | ~104 min | 0.40× | ✅ 已归档 |
| **C** | **TASK-20260503-01 Hot Reload** | **Level 3** | **~104 min** | **0.31×** | **✅ 本任务归档** |

**主线收官** — Inspector + Performance Overlay + Hot Reload 完整闭环。后续候选见 activeContext.md 待处理事项 §「Phase C 完成后 P3 候选」段。

---

## 9. 参考文档

- 设计规格：`docs/specs/2026-04-30-devtool-design.md`（A10-A12 + A13-A14 + T2 路径穿越威胁高 + T8 mutation propagation 中威胁 + I7/I8 注入点 + R4/R6 风险 + 强依赖 R3+ #3 F-025 — 复用，无修改）
- 实现计划：`docs/plans/2026-05-03-devtool-hot-reload.md`（独立 plan ~700 行 + 11 子任务 5-步 TDD 模板 + Phase 0 13 子段实测填写 + B1-B8 决策表 + plan ×0.6 第 48+ 数据点假设 + 5 R 风险登记 + 3 Checkpoint + 12 条 systemPatterns 协同度自我对照）
- 创意设计：`memory-bank/creative/creative-devtool-hot-reload.md`（5 决策已锁定 — file watcher 抽象层接口 / CSS-only 增量重载策略 / DOM 状态保留协议 / watcher root 边界 / CSS 解析失败错误恢复 — 复用，无修改）
- 蓝图 plan：`docs/plans/2026-04-30-devtool.md` §Phase C 段（10 子任务粗模板 — 作为输入资产）
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260503-01.md`（Level 3 详细 11 段 ~720 行）
- 上一任务归档：`memory-bank/archive/archive-TASK-20260502-02.md`（Phase B 主线归档 — lazy-attach C ABI 容错模式新沉淀来源 + 极窄档延续高效区 0.30-0.45× 候选新子档来源）
- 上上任务归档：`memory-bank/archive/archive-TASK-20260502-01.md`（Phase A 主线归档 — 5 大可复用架构范式来源 + Phase 0 投入定律首次发现）

---

## 10. commit 链（13 commits — 严格 1 commit/子任务 + 1 plan + 1 reflect + 1 finalize）

```
44875e8 docs(reflect): add reflection for TASK-20260503-01
6247626 chore(devtool): finalize Phase C 11/11 + A14 黑名单更新 (C.5.2)
b424d32 test(devtool): T2 完整安全单测 8 步守卫 + 反向探针 (C.5.1) [安全相关]
c5d7a1d feat(devtool): hello_devtool hot reload smoke + dogfood (C.4.2)
b48c57f feat(api): vx_view_attach_devtool + hot_reload_dir + lazy-attach (C.4.1) [安全相关]
81772bb feat(devtool): DevTool UI hot reload status indicator (C.3.1)
651530e feat(devtool/hot_reload): CSS parse failure error recovery (C.2.3)
53fe1ab feat(devtool/hot_reload): HotReloadManager + R9 F-025 strict contract (C.2.1)
7d1e9b5 test(devtool/hot_reload): InotifyFileWatcher full test set 4 edge cases (C.1.3)
256585d feat(devtool/hot_reload): T2 path-traversal 8-step guards (C.1.2) [安全相关]
e432f44 feat(devtool/hot_reload): InotifyFileWatcher Linux backend (C.1.1)
b044d8f feat(devtool/hot_reload): FileWatcher abstract base + Platform factory (C.0.1)
e9c07da docs(TASK-20260503-01): VAN+Plan output + §0.1 baseline verification record
```

**额外 main 分支 commit（hotfix 分离协议）：**

```
ddc1e3c (main)  build: -Wl,--start-group/--end-group for binutils 2.46+ (hotfix/binutils-2.46-link-group, fast-forwarded to main)
```

后续将追加：
- archive commit：`docs(archive): add archive for TASK-20260503-01`
- 收尾 commit：`chore(workflow): complete TASK-20260503-01 and reset to idle`

---

## 11. 后续任务依赖估时调整

基于 Phase C 实测数据（0.31× 极窄档加速衰减区下沿候选新子档）+ 5 大范式 100% 复用 + lazy-attach C ABI 模式扩展 + dogfood 路径第三次复用，对下游任务的估时调整：

| 任务 | 蓝图估时（plan ×0.6）| Phase B archive 校准 | Phase C archive 进一步校准 |
|---|---|---|---|
| **DevTool 三件套主线** | — | — | **✅ 完整闭环 — Phase A/B/C 全部归档** |
| #35 阶段 2 拆 LayoutEngine | 未估 | ~2-3 h plan ×0.6 推荐 Level 3 | **持平** — 涉及 LayoutEngine 子系统局部改造 + 既有契约维护，与 Phase C 范式重叠少 |
| R9 EventManager HitTest 改造 | 未估 | ~1.5-2 h plan ×0.6 推荐 Level 2-3 | **持平** — pointer-events 真支持改造，与 Phase C 范式重叠少 |
| DomBindings R2 P3 三连补全 | 未估 | ~3-5 h plan ×0.6 | **可下调到 ~2-4 h plan ×0.6**（-30%）— 受益于 Phase A/B/C dom_bindings.cc 三次扩展经验 + JS native binding 范式成熟 |
| R3+ #1 image_decoder 安全三件套（F-049/F-050/F-051）| 4-6 h | — | **可下调到 ~3-4 h plan ×0.6**（-30%）— 受益于 T2 8 步守卫 + max_size 模式 + dual-probe 反向探针 16 测范式直接复用 |
| TASK-30-04-D Console JS REPL（V1=B 扩展段）| 未估 | — | **~3-5 h plan ×0.6 推荐 Level 3** — 涉及 QuickJS REPL 设计 + T1 任意 eval mitigation |
| TASK-30-04-E JS Debugger backend | 未估 | — | **~6-10 h plan ×0.6 推荐 Level 4** — QuickJS Debug API 对接 + 触及技术债 #44 + T6 callback budget 威胁 |
| TASK-30-04-F CDP 远程调试 port | 未估 | — | **~4-8 h plan ×0.6 推荐 Level 3-4** — T4 HMAC token + nonce + loopback only + default off mitigation |
| TASK-30-04-G 完整 UI 编辑器 | 未估 | — | **~10-20 h plan ×0.6 推荐 Level 4 多 Phase** — dogfood 完整闭环，spec §11 长期愿景 |

---

## 12. 任务时长统计

| 阶段 | 时长 | 备注 |
|:--|--:|---|
| VAN | ~5 min | F1-F8 grep 实证 + 6 项 push-back 决策 + 前置验证 6/6 |
| Plan | ~8 min | Phase 0 13 子段实测 + B1-B8 决策表 + plan ~700 行落盘 |
| Build | ~104 min | 11 子任务 + CP1/CP2/CP3 + 双绿 verify finalize（含 binutils hotfix ~7 min） |
| Reflect | ~7 min | reflection-TASK-20260503-01.md ~720 行 + Memory Bank 长期知识库升级 |
| Archive | ~10 min | 本文档 + 收尾 |
| **合计** | **~134 min** | **Phase 0 投入定律 7.6× ROI** — 30 min Phase 0 投入 → 节省 ~229 min build phase |
