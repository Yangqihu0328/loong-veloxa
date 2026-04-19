# 归档：修复 main Release `-Werror` 编译失败（2 处）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-07
**复杂度级别：** Level 1
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260419-07-release-werror-fixes`（5 commits）→ 合并到 main
**相关任务：** TASK-20260419-04（GCC IPA `-Warray-bounds` 修复，detemplatize `Lookup<N>`）— **同元模式不同手法**；TASK-20260419-03 P6（fresh build 副发现暴露本任务）

---

## 1. 任务概述

修复 main 分支已存在的两个 Release `-Werror` 编译失败，由 TASK-20260419-03 Phase 6 fresh Release `cmake --build build-bench -j` 全量构建首次暴露：

| # | 文件 / 位置 | 错误 |
|---|------|------|
| (a) | `tests/platform/memory_surface_test.cc:102/105/108` | `std::fgets` 忽略返回值 → `-Werror=unused-result` |
| (b) | `veloxa/foundation/strings/string.h:69`（拷贝构造内 memcpy）→ 在 `tests/foundation/strings/string_test.cc:59 String b(a);` 调用点报错 | `BasicString` 拷贝构造 `memcpy` `-Werror=array-bounds` GCC IPA 误报 |

**目标**：消除两个 -Werror，使 `cmake --build build-bench -j` 全量 Release 构建无错误，Debug ctest 全量回归不变（基线 890/890）。

---

## 2. 技术方案

### Fix (a) `fgets` `-Wunused-result`

**方案 A1**：用 `ASSERT_NE(std::fgets(...), nullptr)` 包裹 3 处调用。
**理由**：测试代码中读失败本应是硬失败而非依赖后续 `EXPECT_STREQ` 兜底；既消除编译警告又强化测试语义。

### Fix (b) GCC IPA `-Werror=array-bounds` 误报

**方案 B2**：在 `BasicString(const BasicString&)` 拷贝构造上加 `[[gnu::noinline]]`，用 `#if defined(__GNUC__) && !defined(__clang__)` 守卫。

**根因诊断**：
- 报错：`error: ‘void* __builtin_memcpy(...)’ forming offset [32, 41] is out of the bounds [0, 32] of object ‘a’ with type ‘vx::String’`
- GCC `-Warray-bounds` 由**中端 IPA pass** 发出（`-fipa-modref` + range derivation）
- IPA 内联拷贝构造 → 内联 `other.data()`（可返回 SSO 内 pointer 或 heap pointer）→ 错误地用 `&storage_ + sizeof(Storage)` 推导 `data()` 上界 → 当 `sz=42` 时算出 [32, 41] 超出 32 字节对象 → 误报
- `[[gnu::noinline]]` 阻止内联，IPA 看到 noinline 函数边界 → 无法把"对象 size 32"与"runtime sz=42"关联 → 误报消失

**为什么不选 B3 `__builtin_memcpy`（VAN 阶段曾推荐）**：试验失败。GCC 诊断在 IPA 中端发出，**先于 fortify 展开**。`__builtin_memcpy` 跳过 `__memcpy_chk` fortify wrapper 但不跳过 IPA range derivation，仍报同样错误（消息函数名变 `__builtin_memcpy`）。详见 reflection 文档 §4.1。

**为什么不选 B1 `#pragma`**：可行但治标不治本，需在 `string.h` 留下 5 行污染，未来 GCC 修复后需手动清理；B2 是根因消除（阻断 IPA 跨函数关联），与 TASK-04 detemplatize 同根。

**性能影响评估**：拷贝堆 String 必伴随 1 次 allocator 调用，indirect call 的 1-2ns 被 allocator 路径稀释；move ctor（实际热路径）未受影响。`#if` 守卫保证 clang 仍内联，无跨编译器影响。

---

## 3. 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `tests/platform/memory_surface_test.cc` | +3/-3：3 处 `std::fgets(...)` → `ASSERT_NE(std::fgets(...), nullptr)` |
| 修改 | `veloxa/foundation/strings/string.h` | +6/0：拷贝构造前加 `#if __GNUC__ && !__clang__ [[gnu::noinline]] #endif` + 4 行注释（含 TASK-07 引用） |
| 修改 | `.gitignore` | +1：`build-*/` 通配，避免 `build-bench/` / `build-debug/` 污染 `git status`（chore，scope-creep 但 1 行受益所有 bench 任务）|
| 创建 | `memory-bank/reflection/reflection-TASK-20260419-07.md` | Level 1 反例：写独立 reflection 因有 P1 级方法论教训 + 与 TASK-04 横向同源链接 |
| 创建 | `memory-bank/archive/archive-TASK-20260419-07.md` | 本文件 |
| 修改 | `memory-bank/{tasks,activeContext,progress,systemPatterns}.md` | MB 状态推进 + 沉淀 GCC IPA 元模式诊断表 |

### 提交清单（5 commits, ahead of main）

| # | SHA | 描述 |
|---|------|------|
| 1 | `8b57f8d` | fix(tests/platform): assert fgets return value to clear -Werror=unused-result |
| 2 | `51d6ff1` | fix(foundation/strings): mark BasicString copy ctor noinline to dodge GCC IPA -Warray-bounds false positive |
| 3 | `6fca7cb` | chore(build): finalize TASK-20260419-07 memory bank state |
| 4 | `0d749c1` | docs(reflect): add reflection for TASK-20260419-07 |
| 5 | `466ba01` | chore: ignore build-*/ directories |

### 关键决策

1. **方案 (b) 从 B3 → B2 的转向**：VAN 阶段推荐 B3（`__builtin_memcpy`）基于「`__memcpy_chk` 是根因」假设，BUILD 试验失败后转 B2（`[[gnu::noinline]]`）。代价：1 次 build round-trip（~30s）。**教训**：方案表必须先做 30 秒根因验证再推荐，已沉淀为 `systemPatterns.md` 新模式
2. **`[[gnu::noinline]]` 加 GCC 守卫**：用 `#if defined(__GNUC__) && !defined(__clang__)` 包裹，clang 不受影响（clang 不触发该误报）
3. **副发现主动记录**：`string.h` 还有 3 处 runtime-size memcpy（line 45 SSO ctor / 150 Append / 230 GrowAndCopy）共享同一根因架构但当前 GCC 11.4 未触发，commit body + tasks.md 候选 + 长期项 P3 三处留下未来回归的迁移指南
4. **chore: `.gitignore` `build-*/`**：注意到 `build-bench/` 在本任务及 TASK-03 都污染 `git status`，1 行修改通配化，scope-creep 合理（益所有 bench 任务）
5. **独立 reflection 文件（Level 1 反例）**：Level 1 通常将回顾追加到 `progress.md` 即可，但本任务有 P1 方法论教训 + TASK-04 横向同源链接，独立文件便于后续被引用

### 安全决策

**本任务不涉及安全变更。** 修改 1 行 `[[gnu::noinline]]` attribute + 3 行 `ASSERT_NE` 包装 + 1 行 `.gitignore` 通配，无外部输入 / 认证 / 数据保护 / 新依赖。

---

## 4. 测试覆盖

无新增测试 — 本任务修复**编译错误**（编译器自身充当 RED 信号）和**测试代码语义强化**（fgets 失败之前是隐式期望，A1 改为硬失败显式化）。覆盖通过完成验证证明：

| 验证项 | 命令 | 结果 |
|------|------|------|
| 单 target 验证 (a) | `cmake --build build-bench --target memory_surface_test -j` | ✅ 0 errors → `9/9 PASSED` |
| 单 target 验证 (b) | `cmake --build build-bench --target string_test -j` | ✅ 0 errors → `26/26 PASSED` |
| Release 全量 build | `cmake --build build-bench -j` | ✅ exit 0（38.7s 全 target 编译通过）|
| Debug ctest 全量回归 | `cd build && ctest -j --output-on-failure` | ✅ **890/890 PASSED**（2.46s, 零回归）|
| 7 bench sanity（防 TASK-03 baseline 受损）| 7 bench 各跑 1ms | ✅ 全 exit 0，BM 数字正常 |

---

## 5. 经验教训（5 项关键，详见 reflection 文档）

1. **GCC `-Warray-bounds` 修复方法论**：诊断由 3 个不同编译阶段发出（前端 / 中端 IPA / 后端 fortify），方案选错则白做工。**30 秒判定法**：错误消息函数名含 `_chk` 后缀 → fortify；不含 → 中端 IPA；试做 `std::memcpy → __builtin_memcpy` 1 步实验：消息变名仍报 → 100% IPA。已沉淀为 `systemPatterns.md` 新段诊断表
2. **「阻断 IPA 关联」元模式**（TASK-04 + TASK-07 横向归纳）：GCC IPA 误报的治本手法是**强制 IPA 在某个边界停止跨函数推导**。两个任务的不同手法（去模板化 / `[[gnu::noinline]]` / 隐藏指针来源）共享同一元原理。已沉淀为 `systemPatterns.md` 「阻断 IPA 关联方案族」决策表
3. **方案表「根因验证步骤」要求**（新方法论教训）：候选方案表 ≥ 3 项时，每方案必须附 ≤ 30 秒根因验证操作（grep / 读 1 个头 / 跑 1 个 build），而非靠错误消息表面假设。本任务 B3 假设失败正是反例。已沉淀为 `systemPatterns.md` 段，待固化到 `writing-plans.mdc`
4. **VAN「无需 FetchContent」预判完美绕开 P0 反复模式**：累计 9+ 次的 git proxy 踩坑本次零损耗（`build-bench/` 复用 → 不触发 `FetchContent` → 无需 git proxy）。证明 P0 升级 + checklist 设计有效
5. **副发现强制记录习惯延续低成本高回报**：`string.h` 行 45/150/230 同根但未触发，commit body + tasks.md + 长期项 P3 三处留迁移指南，是 TASK-03 反思的延续应用

---

## 6. 改进建议落实状态

来自 reflection 文档 §6（5 项）：

| # | 建议 | 优先级 | 落实状态 | 后续承接 |
|---|------|--------|---------|---------|
| 1 | GCC IPA 诊断 3 阶段表 + 阻断关联方案族 → `systemPatterns.md` | P1 | ✅ 已落实（commit `0d749c1`）| — |
| 2 | 候选方案表附根因验证步骤 → `writing-plans.mdc` | P1 | ⏳ 已加 `activeContext.md` 长期项 P1 | 待固化（下个涉及方案表的 plan 任务前） |
| 3 | `string.h` 剩余 3 处 memcpy 防御性 noinline | P3 触发型 | ⏳ 已加候选 TASK-08（触发条件：未来 GCC 升级回归）| tasks.md 待立项区 |
| 4 | 「方案根因假设未先验证」加入 `/reflect` 反复模式表 | P2 | ⏳ 已加 `activeContext.md` 长期项 P2 | 下次 `/reflect` 命令文档维护时落实 |
| 5 | TASK-04 ↔ TASK-07 横向链接 | P2 | ✅ 本归档文档「相关任务」段已实现单向链接（TASK-04 archive 保持不可变性，未回填）| — |

**P0 落实情况**：本任务无 P0 建议；预防性应用了 P0「FetchContent VAN proxy 检查」— 实地验证 0 损耗。

---

## 7. 参考文档

- 设计规格：N/A（Level 1 无独立设计文档）
- 实现计划：N/A（Level 1 直接 BUILD）
- 创意设计：N/A
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260419-07.md`
- 关联归档：`memory-bank/archive/archive-TASK-20260419-04.md`（GCC IPA `-Warray-bounds` 修复，detemplatize `Lookup<N>`，**同元模式不同手法**）
- 触发归档：`memory-bank/archive/archive-TASK-20260419-03.md`（CSS 基准，P6 fresh build 暴露本任务）
- 模式沉淀：`memory-bank/systemPatterns.md` 「来自 TASK-20260419-07 — GCC IPA `-Warray-bounds` 「阻断关联」元模式」段
