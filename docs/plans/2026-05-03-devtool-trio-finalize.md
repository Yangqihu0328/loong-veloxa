# DevTool 三件套主线收官 — 4 项 P3 候选批量清零 实现计划

**目标：** 通过 4 项 P3 候选批量清零，让 DevTool 三件套（Inspector + Performance Overlay + Hot Reload）的 ctest dogfood、注释一致性、测试代码 robustness、对外 README 文档达到「公开发布可读」标准。

**架构：** 4 项独立小变更分别落地，互无依赖；按文件分组最小化 IDE 切换成本；每项独立 commit 应用 `git-workflow.mdc`「Multi-subtask commit 拆分」推荐范式。

**技术栈：** 既有 — C++17 / GoogleTest / ctest / CMake / Markdown；无新依赖；无新编译单元；无新 C ABI。

**复杂度级别：** Level 2（多文件修改 / 需求清晰 / 4 项小型清理 / 无新组件 / 无设计决策）

**任务 ID：** `TASK-20260503-03`

**分支：** `feature/TASK-20260503-03-devtool-trio-finalize`（基于 main `5667c8c`）

**安全相关：** ❌ 否（无新外部输入处理 / 无认证 / 无数据存储 / 无部署；P3 task #3 三元守卫属代码 robustness 改进，非安全边界）

---

## 范围检查

4 项 P3 全部落地于既有 5 文件，互无依赖，单一计划处理足够。无需拆分子项目。

---

## writing-plans skill 强制段触发情况自检

| 段 | 触发？| 处理方式 |
|---|:-:|---|
| 公开 API testability 检查清单 | ❌ | 无新公开 C ABI / public API |
| CMake 链接方向约束分析 | ❌ | 无新编译单元 / 无新 target_link_libraries |
| 静态库循环依赖审计 | ❌ | 无新符号导出 |
| Web 标准 API 多重载形态清单 | ❌ | 无 WHATWG/DOM/Fetch API |
| FetchContent C 子项目编译选项审计 | ❌ | 无 FetchContent 改动 |
| FetchContent 网络代理守卫 | ✅ 跳过条件命中 | `build/_deps` 已预置完整 |
| 测试基础设施审计 | ❌ | 不写新测试，仅 fix 2 处既有测试反模式 + ctest 配置调整 |
| **ctest 数量预期 config 矩阵** | ✅ **必填** | 见下方「§3.1 验收 ctest config 矩阵」 |
| 边界输入清单 | ❌ | 无解析 / 查找 / 转换变更 |
| 调用链端到端验证 | ❌ | 无函数签名 / 默认值 / 返回类型变更 |
| 管线注入点代码级可行性验证 | ❌ | 无新 hook 注入 |
| 既有测试隐式契约 fingerprint | ❌ | 无 layout / parser / event 核心算法变更 |
| CSS shorthand 能力 grep 表 | ❌ | 无 CSS 解析任务 |
| 性能基准任务必检项 | ❌ | 无 google/benchmark 新 BM |
| **smoke 工具链可用性检查** | ✅ **必做** | VAN 已 grep；Phase 0 §0.2 复述结论 |
| **Toolchain 版本激进升级检查** | ✅ **必做** | Phase 0 §0.1 工具链快照 |
| 测试文件 include 卫生 grep | ❌ | 不新增 test 文件 / 不引入新第三方 header |
| **验收用例与 example 一致性检查** | ⚠️ **部分相关** | P1 PASS regex 必须对齐 `hello_devtool.cc:254` 既有 `PERF SMOKE: frames=N hud_visible=M` 输出 — 已在子任务 P1 步骤 1 锁定 |
| Layout 类任务必检项 | ❌ | 无 layout 算法变更 |

---

## 文件结构

| 文件 | 职责 | 修改类型 |
|---|---|---|
| `tests/integration/devtool_dogfood_smoke_test.cc` | DevTool dogfood smoke test fixture | P3 task #3 修复 1 处反模式（第 106 行）|
| `tests/devtool/hot_reload/file_watcher_test.cc` | Hot Reload file watcher T2 安全测试 | P3 task #3 修复 1 处反模式（第 314 行）|
| `tests/CMakeLists.txt` | DevTool 三件套 ctest 配置 | P1 修改 hello_devtool_perf_smoke autoquit + PASS regex（第 376-384 行）+ P2 三段注释装裱（第 359-403 行）|
| `examples/hello_devtool.cc` | DevTool 三件套 dogfood example | P1 同步注释更新（第 250-255 行）+ P2 文件头 docstring 更新（第 1-20 行）|
| `README.md` | 项目根 README | P4 由 2 行扩展到 ~50-80 行 |

**5 文件，零新建。** Memory Bank 三件套（`tasks.md` / `activeContext.md` / `progress.md`）由 plan / build / reflect / archive 阶段各自更新。

---

## Phase 0 — 状态核验（极简 1 子段）

### §0.1 工具链版本快照（已采集 / 与上次任务一致 → ✅ 跳过差异检查）

```text
gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
GNU ld (GNU Binutils for Ubuntu) 2.46     # binutils 2.46+ link group hotfix 已落地（systemPatterns 已沉淀）
cmake version 4.2.3
ninja 1.13.2
```

判读：版本与 TASK-20260503-01 / TASK-20260503-02 完全一致 → ✅ 无新激进升级风险，跳过差异检查。

### §0.2 smoke 工具链可用性（VAN 阶段已 grep）

| 工具 | 状态 | 兜底 |
|---|:-:|---|
| `jq` | ✅ `/usr/bin/jq` | — |
| `rg` | ❌ MISS | Grep 工具（cursor 内置）兜底 |
| `awk` | ✅ `/usr/bin/awk` | — |
| `python3` | ✅ `/usr/bin/python3` | — |

本任务实际使用：仅需 `Read` / `StrReplace` / `Shell ctest` / `Grep`，工具链充分。

### §0.3 ctest baseline 快照（已采集）

- **DEVTOOL=ON（当前配置）**：`ctest -N` 显示 **Total Tests: 1247** PASS（与 productContext 2026-05-03 记录一致 ✅）
- DEVTOOL=OFF / SDL2=ON 配置不重新 reconfigure（避免 cmake 重跑成本，且 P3 fix 不应影响 OFF 路径 / SDL2 路径，CP2 仅验证 ON 配置即可）

### §0.4 P3 task #3 audit 预跑结论（VAN 阶段已固化）

GoogleTest `ASSERT_TRUE(x.ok()) << x.status().message()` 模式 audit 5 命中：

| # | 文件:行 | 模式 | 评估 | 范本来源 |
|:-:|---|---|:-:|---|
| 1 | `tests/platform/memory_surface_test.cc:96` | `ASSERT_TRUE(status.ok()) << status.message();` | ✅ 安全 | status 已先取出 |
| 2 | **`tests/integration/devtool_dogfood_smoke_test.cc:106`** | `ASSERT_TRUE(json.ok()) << "...: " << json.status().message().data();` | ⚠️ **反模式 — 需 fix** | 子任务 P3.1 |
| 3 | `tests/graphics/drawtext_shape_cache_test.cc:39,41` | `ASSERT_TRUE(...) << "string literal";` | ✅ 字面量安全 | 字符串字面量 |
| 4 | **`tests/devtool/hot_reload/file_watcher_test.cc:314`** | `ASSERT_TRUE(resolved.ok()) << "...: " << resolved.status().message();` | ⚠️ **反模式 — 需 fix** | 子任务 P3.2 |
| 5 | `tests/script/quickjs_engine_test.cc:18` | `ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");` | ✅ **三元守卫范本** | **复用模板** |

→ 实际 fix 范围 **2 处**，远低于 activeContext 假设的 8 处（audit 缩减 ~75% 工作量）；fix 模板直接复制 `tests/script/quickjs_engine_test.cc:18` 的 `<< (!x.ok() ? x.status().message() : "")` 三元守卫语义。

### §0.5 反复模式核对（来自 systemPatterns 反复模式渐进式抑制段）

本任务 7 项反复模式抑制核对：

| # | 反复模式 | 本任务命中？ | 抑制策略 |
|:-:|---|:-:|---|
| 1 | 前置依赖/环境/API 能力未验证 | ❌ | 4 文件全部已读取上下文 |
| 2 | 测试隔离问题 | ❌ | 不新增 test 文件 / 不引入新 header |
| 3 | 候选方案根因假设未验证 | ❌ | 4 项 P3 全部既有功能小补强，无候选方案 |
| 4 | 计划文件清单与实际变更不一致 | ❌ | plan §文件结构表 5 文件全部已锁定 |
| 5 | 非默认路径遗漏验证 | ❌ | 无算法分支 |
| 6 | ctest 数量声明未明示 config 矩阵 | ✅ **必抑制** | §3.1 验收矩阵显式列 DEVTOOL=ON 1247 → 1247（不退化）|
| 7 | example UI 行为与 plan 验收 drift | ✅ **必抑制** | P1 PASS regex 与 `hello_devtool.cc:254` `PERF SMOKE: frames=N` 输出对齐核对 |

7 项反复模式预期 **0/7 命中**（连续第 4 次零反复 — Phase A → B → C → TASK-20260503-02 → 本任务）。

---

## B1-B9 决策表（用户 1 次 AskQuestion 选 `all_recommended` → 9/9 锁定）

| # | 维度 | 锁定 | 理由 |
|:-:|---|---|---|
| **B1** | 子任务执行顺序 | P3 反模式 fix → P1 ctest 配置 → P2 注释装裱 → P4 README → finalize | P3 fix 后即可一次 ctest verify；P1+P2 同 `tests/CMakeLists.txt` 文件 batch；P4 独立 |
| **B2** | 测试模式标记 | P1 [覆盖补充] / P2 [文档调整模式] / P3 [覆盖补充] / P4 [文档调整模式] | P1 P3 调整既有测试行为/契约；P2 P4 纯注释/文档（TASK-20260503-02 已沉淀）|
| **B3** | P1 PASS regex 阈值 | N≥3（`frames=([3-9]\|[1-9][0-9]+)`）+ autoquit 600ms | 200→300→600ms 渐进；N≥3 验证 60 帧 ring buffer 至少 3 个 sample；N≥10 风险慢 CI 误失败 |
| **B4** | P2 装裱范围 | 仅 ctest 三段注释一致化 + `hello_devtool.cc` 文件头 docstring 更新（不改函数体）| 与 P4 README 互补；不扩散到产品代码 |
| **B5** | P4 README 篇幅 | ~50-80 行（项目简述 + 核心功能表 + DevTool 三件套段 + Build & Run + 交叉链接）| productContext 已沉淀可直接复用 |
| **B6** | Phase 0 检查范围 | 极简 1 子段（工具链快照 + smoke 工具 + audit 预跑结论引用）| Level 2 不需重型 Phase 0；audit 已 75% 减负 |
| **B7** | Checkpoint | CP1（P3 fix 完成 → 2 测试继续 PASS）+ CP2（P1+P2 完成 → hello_devtool_perf_smoke 通过新 regex）| 主交付物双绿守门 |
| **B8** | commit 粒度 | 4 commits（1 commit/项）+ 1 finalize commit | 应用 `git-workflow.mdc`「Multi-subtask commit 拆分」推荐范式（来自 TASK-20260503-02 任务 4）|
| **B9** | 估时假设 | plan ×0.6 ~70-110 min → 实测 ~25-50 min 期望 | 落「纯文档/规则极速区延伸」+ audit 已减负 75% |

---

## 子任务清单（按 B1 顺序）

### 子任务 1（P3.1）：fix `devtool_dogfood_smoke_test.cc:106` 三元守卫 [覆盖补充]

**文件：**
- 修改：`tests/integration/devtool_dogfood_smoke_test.cc`（第 106 行）

- [ ] **步骤 1：定位现状**
  当前代码（第 106 行）：
  ```cpp
  ASSERT_TRUE(json.ok()) << "EvalDevtoolScript: " << json.status().message().data();
  ```
  反模式：`json.ok()` 为 true 时仍调用 `json.status()` — 依赖 GoogleTest 短路评估保证安全（独立语句 `auto err = json.status();` 在 OK 时 abort）。

- [ ] **步骤 2：替换为三元守卫**
  改为（复用 `tests/script/quickjs_engine_test.cc:18` 范本）：
  ```cpp
  ASSERT_TRUE(json.ok()) << "EvalDevtoolScript: "
                         << (!json.ok() ? json.status().message().data() : "");
  ```

- [ ] **步骤 3：运行测试验证不退化**
  ```bash
  cmake --build build --target devtool_dogfood_smoke_test -j
  cd build && ctest -R DevtoolDogfoodSmokeTest --output-on-failure
  ```
  预期：PASS（既有 7 个 testcase 全部继续通过）

- [ ] **步骤 4：commit**
  ```bash
  git add tests/integration/devtool_dogfood_smoke_test.cc
  git commit -m "test(integration): use ternary guard for StatusOr.status() (P3 #1)

  Source: TASK-20260503-02 reflection §6 P2 #3 / TASK-20260503-03 P3 #1.

  ASSERT_TRUE(x.ok()) << x.status().message() relies on GoogleTest short-
  circuit evaluation. Replace with the explicit ternary guard pattern
  established by tests/script/quickjs_engine_test.cc:18 so the OK path
  never reaches into status()."
  ```

---

### 子任务 2（P3.2）：fix `file_watcher_test.cc:314` 三元守卫 [覆盖补充]

**文件：**
- 修改：`tests/devtool/hot_reload/file_watcher_test.cc`（第 314 行）

- [ ] **步骤 1：定位现状**
  当前代码（第 314 行）：
  ```cpp
  ASSERT_TRUE(resolved.ok()) << "resolve failed: " << resolved.status().message();
  ```

- [ ] **步骤 2：替换为三元守卫**
  ```cpp
  ASSERT_TRUE(resolved.ok()) << "resolve failed: "
                             << (!resolved.ok() ? resolved.status().message() : "");
  ```

- [ ] **步骤 3：运行测试验证不退化**
  ```bash
  cmake --build build --target file_watcher_test -j
  cd build && ctest -R InotifyFileWatcherT2Test --output-on-failure
  ```
  预期：PASS（T2 8 步守卫 dual-probe 16 测全部继续通过）

- [ ] **步骤 4：commit**
  ```bash
  git add tests/devtool/hot_reload/file_watcher_test.cc
  git commit -m "test(devtool/hot_reload): use ternary guard for StatusOr.status() (P3 #2)

  Source: TASK-20260503-02 reflection §6 P2 #3 / TASK-20260503-03 P3 #2.

  Same ternary-guard normalization as P3 #1; this is the second of two
  call sites identified by the VAN audit (5 hits / 2 actual anti-pattern
  / 1 ternary-guard exemplar at quickjs_engine_test.cc:18)."
  ```

---

### 🔍 CP1：P3 fix 完成自审

- [ ] **CP1.1**：`grep -n "\.status()\.message()" tests/` 确认 audit 表第 2/4 行已修正，第 5 行三元守卫范本未变
- [ ] **CP1.2**：DEVTOOL=ON 1247 baseline 不退化 — `cd build && ctest 2>&1 | tail -3`
- [ ] **CP1.3**：commit 消息含 `Source: TASK-20260503-02 reflection §6 P2 #3` 溯源前缀（应用 TASK-20260503-02 沉淀的 commit message convention）

---

### 子任务 3（P1）：hello_devtool_perf_smoke 多帧验证 [覆盖补充] — ❌ **build 阶段取消（拆细化为 P3 候选）**

> **build 阶段实证发现（2026-05-03 ~20:35）**：plan 假设「调 autoquit ms 即可获得多帧」错误。`UpdateManager::Update()` 第 17 行 `if (!dirty_) return;` 是 frame hooks 触发的硬约束 — `dirty_` 在每帧末尾 reset，仅当 `transition_mgr_.HasActive()` 才 rearm。`hello_devtool` 的 CSS 静态（无 transition / animation）→ 第 1 帧后 `dirty_=false` 永久阻断 hooks。实测 autoquit 600ms / 1500ms 均 frames=1。
>
> **根因归类**：反复模式 #1「前置依赖/环境/API 能力未验证」第 N 次（VAN/plan 阶段未深读 `update_manager.cc:17` 的 dirty_ 短路）。
>
> **用户决策（AskQuestion p1_fix=A）**：回退 P1 + 拆细化为 P3 候选。本任务实际完成 3/4 P3（P3.1 / P3.2 / P2 / P4），P1 留下次任务深入（需新 `vx_view_invalidate` C ABI 或 `hello_devtool` 注入 CSS animation 等持续 invalidate 源）。
>
> **改动状态**：已回退 — `tests/CMakeLists.txt` + `examples/hello_devtool.cc` 恢复 main 5667c8c 原文（git status 已确认）。

**原始 P1 设计文本（保留供下次任务参考）：**

**文件：**
- 修改：`tests/CMakeLists.txt`（第 376-384 行）
- 修改：`examples/hello_devtool.cc`（第 250-255 行 docstring 同步）

- [ ] **步骤 1：定位现状 + 一致性核对**
  `tests/CMakeLists.txt` 第 376-384 行：
  ```cmake
  add_test(
    NAME hello_devtool_perf_smoke
    COMMAND $<TARGET_FILE:hello_devtool>
  )
  set_tests_properties(hello_devtool_perf_smoke PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;VX_HELLO_DEVTOOL_AUTOQUIT_MS=300"
    PASS_REGULAR_EXPRESSION "PERF SMOKE: frames=[1-9][0-9]* hud_visible=1"
    TIMEOUT 10
  )
  ```
  `examples/hello_devtool.cc` 第 250-255 行注释：
  ```cpp
  /* B.3.2 — verify the perf hooks fired during the run. Auto-quit at 200ms
   * @ 60fps target FPS = ~12 frames; we accept >=1 to keep the smoke
   * resilient on slow CI hardware. ... */
  ```
  **一致性核对**：example 输出 `PERF SMOKE: frames=N hud_visible=M`（hello_devtool.cc:254），ctest regex 必须匹配此格式。

- [ ] **步骤 2：修改 ctest 配置**
  ```cmake
  set_tests_properties(hello_devtool_perf_smoke PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;VX_HELLO_DEVTOOL_AUTOQUIT_MS=600"
    PASS_REGULAR_EXPRESSION "PERF SMOKE: frames=([3-9]|[1-9][0-9]+) hud_visible=1"
    TIMEOUT 15
  )
  ```
  Regex 解析：`([3-9]|[1-9][0-9]+)` POSIX ERE — 单个 3-9 数字 OR ≥10 的多位数字 = N≥3。

- [ ] **步骤 3：同步 example 注释更新**
  `examples/hello_devtool.cc:250-255`：
  ```cpp
  /* B.3.2 — verify the perf hooks fired during the run. Auto-quit at
   * 600ms @ 60fps target FPS = ~36 frames; we accept >=3 to validate
   * the 60-frame ring-buffer aggregation has at least 3 samples in
   * flight. CI hardware that cannot reach 3 frames in 600ms is too
   * starved to run a perf overlay anyway. */
  ```

- [ ] **步骤 4：rebuild + ctest 验证**
  ```bash
  cmake --build build -j
  cd build && ctest -R hello_devtool_perf_smoke --output-on-failure -V 2>&1 | tail -20
  ```
  预期：PASS — 输出含 `PERF SMOKE: frames=N hud_visible=1`（N≥3，典型 ~36 实测）

- [ ] **步骤 5：commit**
  ```bash
  git add tests/CMakeLists.txt examples/hello_devtool.cc
  git commit -m "test(devtool): hello_devtool_perf_smoke validate >=3 frames (P1)

  Source: TASK-20260502-02 reflection §5 P3 #3 / TASK-20260503-03 P1.

  Bump VX_HELLO_DEVTOOL_AUTOQUIT_MS 300 -> 600 and tighten the PASS
  regex from frames>=1 to frames>=3 to validate the 60-frame perf
  ring-buffer aggregation has multiple samples in flight, not just
  ABI smoke. Sync the matching docstring in hello_devtool.cc."
  ```

---

### 子任务 4（P2）：三件套 dogfood 路径装裱 [文档调整模式]

**文件：**
- 修改：`tests/CMakeLists.txt`（第 359-403 行三段 ctest 注释）
- 修改：`examples/hello_devtool.cc`（第 1-20 行文件头 docstring）

- [ ] **步骤 1：tests/CMakeLists.txt 三段注释装裱**

  在第 362 行 `if(VX_BUILD_DEVTOOL)` 行**之后**插入 1 行块头注释：
  ```cmake
    # ---- DevTool 三件套 dogfood smoke ctest 集 ----
    # Phase A Inspector  (TASK-20260502-01) — hello_devtool_smoke
    # Phase B Perf Overlay (TASK-20260502-02) — hello_devtool_perf_smoke
    # Phase C Hot Reload   (TASK-20260503-01) — hello_devtool_hot_reload_smoke
    # Three-piece DevTool main line: TASK-20260503-03 finalize.
  ```

  随后将三段单注释统一为 `# Phase X — <子系统名> smoke (<子句>):` 格式：
  - 第 359 行 `# TASK-20260502-01 A.3.1: end-to-end smoke for hello_devtool example.` → `# Phase A — Inspector smoke (DevTool attach + dogfood UI baseline; TASK-20260502-01 A.3.1).`
  - 第 372-375 行 `# TASK-20260502-02 B.3.2: Performance Overlay smoke variant ...` → `# Phase B — Performance Overlay smoke (PipelineHooks 5 hooks + 60-frame ring buffer; TASK-20260502-02 B.3.2 + TASK-20260503-03 P1 frames>=3).`
  - 第 386-394 行 `# TASK-20260503-01 C.4.2: Hot Reload end-to-end smoke ...` → `# Phase C — Hot Reload smoke (inotify + LoadCSS triggered; TASK-20260503-01 C.4.2).`

- [ ] **步骤 2：hello_devtool.cc 文件头 docstring 装裱**

  第 1-20 行 docstring 头部更新为：
  ```cpp
  /*
   * Veloxa — Hello DevTool Example (DevTool 三件套主线收官)
   *
   * Single user-facing example exercising the full DevTool trio:
   *   - Phase A Inspector     (TASK-20260502-01): vx_view_attach_devtool +
   *                                                JS-binding cross-document DOM
   *   - Phase B Perf Overlay  (TASK-20260502-02): VxPipelineHooks +
   *                                                vx_view_set_pipeline_hooks +
   *                                                F11 HUD toggle (B.3.1)
   *   - Phase C Hot Reload    (TASK-20260503-01): VxDevtoolOptions.hot_reload_dir
   *                                                + vx_view_hot_reload_tracked_count
   *
   * Same scene as hello_sdl2.cc but with the DevTool Inspector dogfood UI
   * attached on the right-hand splitter dock. Demonstrates:
   *   - vx_view_attach_devtool() with explicit options
   *   - VxDevtoolOptions { devtool_width, enable_f12_hotkey, hot_reload_dir }
   *   - F12 hotkey toggle (live attach/detach without reload)
   *   - F11 hotkey toggle HUD (B.3.1 — vx_view_is_hud_visible C API)
   *   - vx_view_set_pipeline_hooks() Performance Overlay smoke (B.3.2)
   *   - VX_HELLO_DEVTOOL_HOT_RELOAD_TEST=1 hot reload smoke (C.4.2)
   *   - vx_inspector_set_redaction_policy() T3 policy switch
   *   - SDL2 input forwarding to BOTH target Document and DevTool Document
   *     via Application::InjectInput's hotkey interceptor (A.1.7)
   *
   * Build:  cmake -B build -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON
   *         cmake --build build --target hello_devtool
   * Run:    ./build/examples/hello_devtool
   *         (Press F11 to toggle HUD; F12 to toggle DevTool.)
   */
  ```

- [ ] **步骤 3：rebuild + ctest 验证不退化**
  ```bash
  cmake --build build -j
  cd build && ctest -R "hello_devtool" --output-on-failure 2>&1 | tail -10
  ```
  预期：3 个 hello_devtool_*_smoke ctest 全部 PASS（注释改动不影响构建）

- [ ] **步骤 4：commit**
  ```bash
  git add tests/CMakeLists.txt examples/hello_devtool.cc
  git commit -m "docs(devtool): unify Phase A/B/C smoke comments (P2)

  Source: TASK-20260503-03 P2 — DevTool 三件套主线收官.

  Add a 5-line block header above the three hello_devtool ctest
  registrations naming each phase explicitly, and unify the inline
  comments to 'Phase X — <subsystem> smoke (<subclause>; TASK-id)'
  format. Update hello_devtool.cc file-header docstring to mention
  all three phases up front so readers see the full trio in one
  glance. Pure comment / docstring changes, no functional impact."
  ```

---

### 🔍 CP2：P1+P2 完成自审

- [ ] **CP2.1**：`hello_devtool_perf_smoke` 通过新 regex（实测帧数记录到 progress.md）
- [ ] **CP2.2**：3 个 `hello_devtool_*_smoke` 全部 PASS（regex 改动 + 注释装裱不破坏既有契约）
- [ ] **CP2.3**：DEVTOOL=ON 1247 baseline 不退化 — `cd build && ctest 2>&1 | tail -3`
- [ ] **CP2.4**：example 文件头注释列出三件套与子任务编号交叉链接

---

### 子任务 5（P4）：DevTool README 章节补强 [文档调整模式]

**文件：**
- 修改：`README.md`（由 2 行扩展到 ~50-80 行）

- [ ] **步骤 1：写入完整 README**

  ```markdown
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
  - 编译开关：`-DVX_BUILD_DEVTOOL=ON`（关闭时 A14 链接闭包零字节增长）
  - dogfood 范例：`examples/hello_devtool.cc`
  - 设计 spec：`docs/specs/2026-04-30-devtool-design.md`
  - 蓝图 plan：`docs/plans/2026-04-30-devtool.md`
  - 归档文档：`memory-bank/archive/archive-TASK-20260502-01.md`（Phase A）/ `archive-TASK-20260502-02.md`（Phase B）/ `archive-TASK-20260503-01.md`（Phase C）

  ## 构建与运行

  ```bash
  # 基础构建（headless / 测试 / CI）
  cmake -B build
  cmake --build build -j
  cd build && ctest -j

  # SDL2 真窗口 + DevTool 三件套
  cmake -B build -DVX_PLATFORM_SDL2=ON -DVX_BUILD_DEVTOOL=ON
  cmake --build build --target hello_devtool
  ./build/examples/hello_devtool
  ```

  详细环境（FetchContent 代理、SDL2 安装、benchmark 等）见 `memory-bank/techContext.md`。

  ## 项目状态

  - 当前 ctest baseline：DEVTOOL=ON 1247 PASS / DEVTOOL=OFF 1082 PASS / SDL2=ON 1265 PASS
  - 主线收官里程碑：DevTool 三件套 ✅（2026-05-03）
  - 后续路线：见 `memory-bank/activeContext.md`「待处理事项」段
  ```

  > 注：上面 README 中的二级 ```bash 代码块要在实际写入 `README.md` 时使用单层三反引号（避免嵌套渲染问题）。

- [ ] **步骤 2：手验 markdown 渲染**
  ```bash
  python3 -c "
  with open('README.md') as f:
      content = f.read()
  print(f'Lines: {len(content.splitlines())}')
  print(f'Has DevTool section: {\"DevTool 三件套\" in content}')
  print(f'Has Build section: {\"构建与运行\" in content}')
  print(f'Cross-links count: {content.count(\"docs/\") + content.count(\"examples/\") + content.count(\"memory-bank/\")}')"
  ```
  预期：Lines ≥ 50；3 个 True；交叉链接 ≥ 6 处

- [ ] **步骤 3：commit**
  ```bash
  git add README.md
  git commit -m "docs(readme): add DevTool 三件套 section + project overview (P4)

  Source: TASK-20260503-03 P4 — DevTool 三件套主线收官.

  Expand the root README from 2 lines to ~70 lines with project
  overview, core capabilities table, the DevTool 三件套 section
  (Phase A Inspector + B Performance Overlay + C Hot Reload),
  build & run quick-start, and cross-links to docs/specs/, plans/,
  examples/, and memory-bank/archive/. Pure documentation, no
  code changes."
  ```

---

### 子任务 6（finalize）：activeContext 待处理事项段迁移 + finalize commit

**文件：**
- 修改：`memory-bank/activeContext.md`（4 项 P3 待处理事项段标 ✅ 并迁移到「长期沉淀」段）
- 修改：`memory-bank/progress.md`（最终里程碑记录）

- [ ] **步骤 1：activeContext.md 4 项 P3 标 ✅**
  - TASK-20260502-02 reflection §5 三项 P3 中 #3（hello_devtool_perf_smoke 多帧）→ ✅
  - TASK-20260503-02 reflection §6 P2 #3（GoogleTest 三元守卫）→ ✅
  - 同步更新「长期沉淀」段记录 4 项 P3 闭环

- [ ] **步骤 2：progress.md 最终里程碑**
  追加：「2026-05-03：TASK-20260503-03 build 完成 — 4/4 P3 子任务 ✅ + CP1 + CP2 通过 + DEVTOOL=ON 1247 不退化」

- [ ] **步骤 3：finalize commit**
  ```bash
  git add memory-bank/activeContext.md memory-bank/progress.md
  git commit -m "chore(workflow): finalize TASK-20260503-03 memory bank state

  Mark 4 P3 candidates as completed in activeContext (3 from
  TASK-20260502-02 §5 P3 + 1 from TASK-20260503-02 §6 P2 #3) and
  migrate to the 长期沉淀 section. Append progress milestone."
  ```

---

## §3.1 验收 ctest config 矩阵（writing-plans skill 必填段）

| Config | DEVTOOL | SDL2 | Benchmarks | 期望 ctest | 实测 ctest | diff |
|---|:-:|:-:|:-:|:-:|:-:|:-:|
| baseline（当前 main 5667c8c）| ON | OFF/默认 | OFF | **1247** | - | - |
| 本任务 ON path | ON | OFF | OFF | **1247（不退化）** | - | - |
| OFF path（A14 验证）| OFF | OFF | OFF | **1082（不退化，黑名单不变）** | - | - |
| SDL2 path（example smoke）| ON | ON | OFF | **1265（含 3 个 hello_devtool_*_smoke）** | - | - |

**最小标注**：DEVTOOL=ON 1247 / DEVTOOL=OFF 1082 / SDL2=ON 1265，**本任务零数量增减**（仅修改既有测试 robustness + ctest 配置 + 注释 + README，无新增 / 删除单测）。

**判读：** diff = 0 → ✅ PASS；diff ≠ 0 → 触发 systematic-debugging。

**实施策略：** 默认仅 verify 当前 build 目录的 DEVTOOL=ON 1247（避免 cmake reconfigure 成本）；OFF / SDL2 配置在 `/reflect` 阶段如有时间可补 verify，否则信任 P3 / P4 改动天然不影响 OFF 路径与 A14 黑名单。

---

## §3.2 总体验收要点

- 5 文件 git diff 可见（`git diff main..HEAD --stat` 应显示 5 文件 + activeContext + progress）
- 6 commits（4 P3 + 1 finalize + 0 reflect）+ 1 reflect commit（reflect 阶段）
- DEVTOOL=ON 1247 不退化（CP1 + CP2 + 子任务 5 后三次 verify）
- `hello_devtool_perf_smoke` 实测多帧（≥3）
- 反复模式 0/7 命中（连续第 4 次零反复）
- README 渲染正确（手验交叉链接 + ≥ 50 行）
- activeContext 4 项 P3 标 ✅ 并迁移到长期沉淀段

---

## §3.3 plan ×0.6 估时假设（第 62-66 数据点群组入库）

| 子任务 | plan 蓝图估时 | plan ×0.6 | 实测期望 | 比值假设 |
|---|:-:|:-:|:-:|:-:|
| P3.1 三元守卫 fix | ~5 min | ~3 min | ~2-3 min | 0.40-0.60× |
| P3.2 三元守卫 fix | ~5 min | ~3 min | ~2-3 min | 0.40-0.60× |
| P1 hello_devtool_perf_smoke | ~30 min | ~18 min | ~5-8 min | 0.20-0.30× |
| P2 三件套装裱 | ~25 min | ~15 min | ~5-8 min | 0.20-0.40× |
| P4 README 补强 | ~20 min | ~12 min | ~6-10 min | 0.30-0.50× |
| finalize | ~5 min | ~3 min | ~2-3 min | 0.40-0.60× |
| **合计** | **~90 min** | **~54 min** | **~22-35 min** | **平均 0.30-0.45×** |

**比值预测**：~0.30-0.45×（落「极窄档延续高效区 0.30-0.45×」候选续期 / 介于 TASK-20260503-02 「纯文档/规则极速区 0.21×」与 TASK-20260503-01 「极窄档加速衰减区下沿 0.31×」之间，因本任务 P1 P3 是 [覆盖补充] 含 ctest 等待 + rebuild + 1 处 regex 验证）

**与 systemPatterns plan ×0.6 矩阵的关系**：本任务 6 子任务作为第 62-67 数据点群组入库（如实测落 0.30× 区间 → 补强「纯文档/规则极速区」段「混合扩展子档 0.30-0.40×（含少量 [覆盖补充] ctest verify 子任务）」候选）。

---

## §3.4 systemPatterns 协同度自我对照（9 项）

| # | systemPatterns 段 | 本 plan 应用情况 |
|:-:|---|---|
| 1 | plan ×0.6 矩阵 — 纯文档/规则极速区 0.15-0.25× | §3.3 估时假设直接续延，新增「混合扩展子档」候选 |
| 2 | reflection 沉淀回流模式 | 本任务 4 项 P3 全部来自 activeContext §P3 候选段，正是该模式的第二次实证 |
| 3 | 反复模式渐进式抑制 | §0.5 反复模式核对 — 7 项预期 0/7 命中（连续第 4 次零反复）|
| 4 | Phase 0 audit 预跑模式 | §0.4 task #3 audit 5 命中已 VAN 阶段固化（缩减 75% 工作量）|
| 5 | Multi-subtask commit 拆分（git add -p）| B8 锁 4+1+1 = 6 commits 1/项，应用 `git-workflow.mdc` 推荐范式 |
| 6 | commit message 含 `Source: TASK-XXXXXXXX-XX reflection §X` 溯源前缀 | 6 commits 全部含 Source 前缀（TASK-20260503-02 沉淀的 commit convention）|
| 7 | ctest baseline 配置矩阵 | §3.1 验收矩阵显式列 DEVTOOL/SDL2/Benchmarks 配置 |
| 8 | lazy-attach C ABI 容错模式 | ❌ 不触发（无新 C ABI）|
| 9 | A14 链接闭包黑名单 | ❌ 不触发（无新 DevTool 内部符号）|

---

## §3.5 与 brainstorming skill 决策跳过率监控段的关系

本任务 V1-V5 + B1-B9 共 14 项决策，用户全部跳过 → AskQuestion 选 all_recommended（VAN 推荐默认锁定）。但本任务复杂度 = Level 2 + 决策完全独立 → **不适用** 决策跳过率监控段（该段适用 Level 3+ + 决策依赖图非平凡的任务）。reflect 阶段无需重审决策合理性。

---

## 执行交接

计划完成并保存到 `docs/plans/2026-05-03-devtool-trio-finalize.md`。准备执行？

- 在当前会话中使用 `/build` 命令执行 — 6 子任务按 B1 顺序串行（P3.1 → P3.2 → CP1 → P1 → P2 → CP2 → P4 → finalize）
