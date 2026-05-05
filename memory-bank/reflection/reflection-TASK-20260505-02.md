# 回顾：Performance Overlay 持续 invalidate 机制 — `vx_view_invalidate()` 公开 C ABI（B-G4 — MVP-B 收口最后一项）

**日期：** 2026-05-05
**任务 ID：** TASK-20260505-02
**复杂度级别：** Level 2
**分支：** `feature/TASK-20260505-02-perf-overlay-invalidate-api`
**安全相关：** ❌ 否（公开 ABI 扩展 / 仅暴露既有 UpdateManager::Invalidate / 无新威胁面）
**主交付：** 5 commits / +198 行 code + 78 行 MB / 4 新单测全 PASS / DEVTOOL=ON 1298→1302 + DEVTOOL=OFF 1105→1109 / hello_devtool_perf_smoke frames=1→**18**（10x）/ **MVP-B 100% 🎉**

---

## 1. 计划 vs 实际

### 1.1 维度对比表

| 维度 | 计划 | 实际 | 偏差 / 原因 |
|------|------|------|---------|
| **任务数** | 8（A.1 / B.1 / C.1 / D.1 / D.2 / E.1 / E.2 / E.3） | 8 | 0 偏差 ✅ |
| **预估时间（plan ×0.6）** | 95-115 min | **~30 min** | -65 min（**0.26-0.32×**）/ 落极速区 0.10-0.20× 续延档 / sext-evidence 候选 |
| **commits 数** | 5（A.1+B.1 / C.1+D.1 / E.1 + finalize + plan 阶段 docs） | **5**（`4feda52` plan + `a7e6bed` API + `929569a` smoke + `8d00aee` spec + `27875a8` finalize） | 0 偏差 ✅ |
| **代码行数** | A.1+B.1 ~85 / C.1+D.1 ~25 / 共 ~110 行 | A.1+B.1 **154** / C.1+D.1 **34**（+10/-20 净 +14）/ 共 **+188 行 + 78 MB** | A.1 **+44 行**（Doxygen 30 行 + 反向探针注释 + 4 单测扩展到 152 行）/ 实际比预估略高 |
| **新测试** | 4（null / fresh INVALID_STATE / 正常路径 / idempotent） | **4** | 0 偏差 ✅ |
| **ctest 矩阵** | DEVTOOL=ON 1298→1302 / DEVTOOL=OFF 1105→1109 | DEVTOOL=ON **1302** / DEVTOOL=OFF **1109** | **完全一致 ✅** |
| **dogfood smoke** | hello_devtool_perf_smoke ≥2 帧 | **frames=18**（10x 增益 / 远超 ≥2 阈值）| 大幅超预期 ✅ |
| **反向探针** | 4 项 / 期望 4/4 精准 | **3 项实施 / 3/3 精准 FAIL**（NULL guard 1 / dirty_ rearm 1 / regex 灵敏度 1） | -1（plan §A.1 #4 列了 4 条，实际 #1+#2 等价 SEGFAULT 同效，#3+#4 等价 hook calls=0 同效，合并为 2 项 / 不影响有效性） |
| **设计变更** | — | **0** | D1-A + D2-A + D3-A + D4-A 4 决策 brainstorm 锁死 / 0 调整 / 0 反悔 |

### 1.2 plan ×0.6 实测系数：~0.26-0.32×（落极速区续延档）

| Phase | 估时 plan ×0.6 | 实测 | 比值 |
|---|:-:|:-:|:-:|
| A.1 + B.1（API + 4 单测 + CMake）| ~40-55 min | ~10 min | **~0.20×** |
| C.1 + D.1（hello_devtool 注入 + regex 升级）| ~25 min | ~5 min | **~0.20×** |
| D.2（双 config full ctest）| ~10-15 min | ~10 min | ~0.80×（因 build + ctest 等待主导）|
| E.1 + E.2 + E.3（spec + MB + finalize）| ~20 min | ~5 min | **~0.25×** |
| **总计** | **~95-115 min** | **~30 min** | **~0.26-0.32×** |

**比值偏离极速区 0.10-0.20× 主因：** D.2 双 config full ctest 等待时间不可压缩（build 增量 + 双 config full 跑 ~25 sec + 13 sec），占总时长 ~33%，将比值上抬到极速区下沿外。剔除 D.2 后核心实施 phase 比值约 **0.20×**（命中极速区下沿），符合范式预期。

### 1.3 文件变更清单核对（对照 plan §A.1+§B.1+§C.1+§D.1+§E.1）

| 计划文件 | 计划行数 | 实际行数 | 偏差 |
|---|:-:|:-:|:-:|
| `veloxa/api/veloxa_api.h` | +30 | **+34**（Doxygen 略扩） | +4 |
| `veloxa/api/veloxa_api.cc` | +6 | **+11**（注释扩展） | +5 |
| `veloxa/core/application.h` | +5 | **+15**（注释扩展） | +10 |
| `veloxa/core/application.cc` | +7 | **+10** | +3 |
| `tests/api/invalidate_api_test.cc`（新建）| ~80 | **+118** | +38（test fixture + atomic counter + 4 完整 EXPECT 块）|
| `tests/CMakeLists.txt` | +5 | **+10**（注释扩展）| +5 |
| `examples/hello_devtool.cc` | +10/-3 | **+10/-3** | 0 ✅ |
| `docs/specs/2026-05-04-mvp-scope.md` | ±15 | **+8/-6** | -1 |

**偏差归因：** Doxygen 文档 + commit-friendly 注释扩展占 +50% 增量；测试代码扩展 +47% 因为单测 fixture（SetUp/TearDown）+ atomic counter + 4 完整 test cases 必要的样板代码。**0 个未计划文件**（plan 文件清单 100% 命中）。

---

## 2. 回顾检查清单

**代码变更类任务：**
- [x] 计划精确度 — 8 任务 100% 命中 / 0 未列入计划文件 / 实测行数比计划略高 +20-50% 但属注释/测试样板正常扩张
- [x] TDD 执行情况 — 严格 RED（编译 fail）→ GREEN（4/4 PASS）→ REFACTOR 三阶 / 反向探针 3 项强度梯度三档全谱
- [x] 子代理质量 — N/A（本任务单 agent 直跑 / 范围适合 Level 2 单 agent 执行）
- [x] 测试隔离 — 4 新单测使用静态 atomic counter（C 函数指针约束 / 无法 lambda capture）/ counter 在每个测开头 reset 0 / cleanup hooks 防止跨测污染 / 0 flaky / 0 串扰
- [x] 提交粒度 — 5 commits 全部按计划子阶段分组（plan 阶段 1 + build 阶段 4）/ 0 大杂烩提交 / commit body Source 溯源 100%
- [x] 非默认路径 — null view + fresh view（lazy attach 边界）+ idempotent N=5 多次调用 + reverse probe 3 项均验证

**配置/规则类任务：** 无文件位置 / 交叉引用 / 与其他规则一致性问题（spec 修改 4 处全在 §3.x 段一致）

**安全相关任务：** ❌ N/A — 公开 ABI 扩展 / 仅暴露既有 `UpdateManager::Invalidate` / 不引入新威胁面（详 §3.f 安全评估）

---

## 3. 结构化回顾

### 3.a 做得好的（5 项）

#### 1. Phase 0 audit 11 子段预跑 → 0 失败实施 + 反复模式 #8 第 3 次实证

VAN 阶段 11 audit 子段（CSS animation 不可行 / UpdateManager::Invalidate 既有就绪 / Application::update_manager() const getter / lazy attach 范式 / on_frame_end hook 时序 / A14 守门 / regex 范本 / dirty_ 类型 / PERF SMOKE 输出格式 / perf_hooks_api_test 测试范本 / EnsureUpdateManager 时序）全部预证 → build 阶段 0 假设修正 / 0 plan-fact reconcile / 0 设计探索成本。**反复模式 #8 第 3 次实证**（spec 列「路径 (b) CSS animation」实际不可行 → audit 阶段暴露 → 路径决策修正到 (a)）→ dual-evidence → **triple-evidence 升级** ✅

#### 2. 跨决策协同度 dec-evidence — 1 次 AskQuestion 锁 D1+D2+D3+D4 4 决策

D1-A（target update_manager only）+ D2-A（main thread only）+ D3-A（on_frame_end hook 注入）+ D4-A（4 完整单测）— 1 次 AskQuestion all_recommended 锁定 / 0 反悔 / 0 调整。**第 10 次连续 100% 命中** / 累计 100/100 跨决策一次锁定纪录 / **nona-evidence → dec-evidence 升级** ✅

#### 3. 反向探针强度梯度三档全谱 — dual → triple-evidence

| 强度档 | 探针 | 实测 | 解读 |
|---|---|---|---|
| **过高（双重加固）** | A.1 NULL guard 注释 | **SEGFAULT** in NullViewReturnsNullParam | 守门是 C ABI UB 防御层 + 业务层双重加固 |
| **合适（精准 N/M）** | A.1 `update_manager_->Invalidate()` 注释 | **2/4 精准 FAIL**（InvalidateMakesNextUpdateRunHooks + InvalidateIdempotent / null + fresh 测仍 PASS）| dirty_ rearm 是核心契约 / null + fresh 测不依赖 dirty_ |
| **平衡（等量 N/N）** | D.1 `vx_view_invalidate` 注释 | **frames=1**（1 测 FAIL / regex 灵敏度精准识别）| ctest regex `([2-9]\|[1-9][0-9]+)` 灵敏度验证 |

**强度梯度三档全谱第 2 次任务实证** → dual → **triple-evidence 升级** ✅（TASK-20260505-01 三 phase + 本任务三探针）

#### 4. 「plan/spec docs 落盘即 commit」首次成功实施 — TASK-20260505-01 P1 #6 闭环

来自 TASK-20260505-01 reflection P1 #6 的工作流改进：plan 阶段产出 spec + plan 文档应**落盘即 commit**（防止 build 阶段才补 collateral commit）。本任务在 `/plan` 阶段（commit `4feda52`）即提交了 plan + design docs + Memory Bank 更新 / build 阶段无 collateral commit 补齐 / **首次成功实施** ✅。

#### 5. lazy-attach C ABI 容错模式第 4 次复用

`vx_view_invalidate(view)` 在 `update_manager_` 为 null 时返回 INVALID_STATE 而非 crash — 完美沿用了 `vx_view_set_pipeline_hooks` 的 lazy-attach 范式（TASK-20260502-02 B.0.1 首次沉淀 → TASK-20260503-01 C.4.1 warning 语义层扩展 → TASK-20260503-04 + 本任务）。**0 新设计成本** / Doxygen 文档可直接引用「same lazy-attach contract as vx_view_set_pipeline_hooks」 / 第 4 次实证累计 → quad-evidence 升级候选。

### 3.b 遇到的挑战（3 项 / 全程小型）

#### 1. cmake reconfigure 后 ctest 第一次 -R 0 results 假象

GREEN 阶段 build invalidate_api_test 成功后，第一次 `ctest -R invalidate_api -V` 返回 `No tests were found!!!`（cmake reconfigure 仅生成 build files，gtest_discover_tests 需重新 build 后才更新 ctest 列表）。重新 `cmake --build` + `ctest -R Invalidate`（注意大小写，gtest_discover_tests 命名为类名 `InvalidateApiTest.*`）后正常显示 8/8 tests。**消耗约 ~2 min** debug 时间。**沉淀**：未来 cmake 增量加测时应直接 `cmake --build <target> && ctest -R <ClassName>` 一气呵成（避免分步触发 cmake build files 与 ctest 列表不同步状态）。

#### 2. test fixture 静态 counter 设计权衡

GoogleTest C 函数指针 `void(*)(void*)` 不能 lambda capture，必须用文件级静态变量 + namespace `g_invalidate_hook_calls` atomic counter。该 pattern 与 `perf_hooks_api_test.cc` 一致 / 0 新设计 / 但要求每个 test case 开头 reset 0 + cleanup hooks（避免跨测污染）— 这部分增加了单测样板代码 ~30 行（占 invalidate_api_test.cc 的 25%）。

#### 3. PERF SMOKE 输出契约边界

hello_devtool.cc 改动同时涉及 `s_perf_smoke_frames`（int）→ `s_perf_ud.frames`（struct member）的命名变更 + lambda userdata 类型变更（int* → PerfSmokeUd*）。需要确保 `printf("PERF SMOKE: frames=%d hud_visible=%d\n", ...)` 字符串契约不变 / commit body 显式说明「PERF SMOKE 字符契约不变」/ 0 ctest regex 重写需求。

---

### 3.c 经验教训（4 项）

#### 1. **plan ×0.6 实测系数 sext-evidence — 6 数据点**

| # | 任务 | 比值 | 子档 |
|:-:|---|:-:|---|
| 1 | TASK-20260503-05（QuickJS Interrupt）| 0.16× | 最小代码改动 + Phase 0 预跑极速区 |
| 2 | TASK-20260503-04（DevTool Phase D）| 0.07-0.10× | creative 全锁死 + 范式 100% 复用 |
| 3 | TASK-20260504-01（MVP-scope 蓝图）| 0.21× | 纯文档/规则极速区 |
| 4 | TASK-20260505-01（DomBindings R2 收口）| 0.14-0.18× | 最小代码改动极速区 quint-evidence |
| 5 | **TASK-20260505-02 本任务** | **~0.26-0.32×** | **极速区 0.10-0.20× 续延档**（D.2 ctest 等待主导）|

**子档建议：** 「极速区续延档 0.20-0.35× — D.2/D.3 等不可压缩 ctest 等待时间主导子档」/ 触发条件：(1) 主体实施 phase 已落极速区 0.10-0.20× + (2) 双 config full ctest 验证占总时长 ≥30% + (3) build 增量 + parallel 8 已开 → ctest 等待时间不可进一步压缩

#### 2. **lazy-attach C ABI 容错模式 quad-evidence**

第 4 次复用（TASK-20260502-02 + TASK-20260503-01 + TASK-20260503-04 + 本任务）/ 已是 Veloxa 公开 C ABI 的**默认设计模式**。建议下次添加新公开 ABI 时**强制核对**「是否需要 lazy-attach（即 update_manager_/script_engine_/sub-system_ 可能未初始化的情况）」/ 已成为 Veloxa C ABI 的契约一部分。

#### 3. **「plan/spec docs 落盘即 commit」P1 改进 → 首次成功实施 ✅**

TASK-20260505-01 P1 #6 改进建议本任务首次实施落地。`/plan` 阶段产出 spec + plan + MB 更新合并为单 commit（`4feda52`）/ build 阶段 0 collateral commit / 工作流流程 cleanup ✅。**建议**：将该协议正式入库到 `.cursor/rules/skills/writing-plans.mdc`「plan 阶段产出物 commit 协议」段（移到 P0 立即固化）。

#### 4. **反复模式 #8 spec 数据回归 dual → triple-evidence 升级**

第 3 次实证（TASK-20260504-01 + TASK-20260505-01 + 本任务）：

| # | 任务 | spec 标记 | 代码实际 | 修正方式 |
|:-:|---|---|---|---|
| 1 | TASK-20260504-01 P0 | 4 项「缺失」 | 部分已实现 | reflect P0 沉淀协议 |
| 2 | TASK-20260505-01 VAN | B-G2 addEventListener「缺失」 | 已实现 + 缺 alias | VAN audit 暴露 + 范围调整 |
| 3 | **TASK-20260505-02 VAN** | **路径 (b) CSS animation 推荐** | **引擎不支持 @keyframes** | VAN audit 暴露 + 路径决策修正到 (a) |

**triple-evidence 升级 ✅** / 已达 `.cursor/rules/skills/writing-plans.mdc` 固化阈值 / 建议 P1 立即落地。

---

### 3.d 流程改进（3 项）

#### 1. brainstorming 阶段 — D1+D2+D3+D4 4 决策 1 次 AskQuestion 锁死 ✅

dec-evidence 已达成熟阈值。**0 改进需要**。

#### 2. plan 阶段 — Phase 0 audit 11 子段密度仍是 ROI 最高的步骤 ✅

15 min Phase 0 投入 → 节省 build 阶段 ~65 min（**4.3× ROI**）/ 与历史 quint-evidence 范围 5.2-7.6× ROI 略低（因任务规模小，节省的绝对值小）/ **协议保持不变**。

#### 3. build 阶段 — cmake reconfigure 后 ctest 列表更新假象 ⚠️

详 §3.b #1。建议沉淀到 `techContext.md`「CMake + GTest 工作流注意事项」段（**P2 长期沉淀**）。

---

### 3.e 技术改进（2 项）

#### 1. `Application::Invalidate()` 返回 bool 的语义清晰度

当前实现：`!update_manager_` → false / 否则 true。**潜在改进**：可考虑返回 `enum class InvalidateResult { Ok, NotInitialized, ... }` 增强可扩展性。**当前评估**：P3 / 不必要 — bool 已足够 / 与 lazy-attach 范式一致 / Doxygen 已明示语义 / `vx_view_invalidate()` 在 C ABI 层已映射到 `VX_OK / VX_ERROR_INVALID_STATE` 完整 enum / 不需要内层 enum。

#### 2. PerfSmokeUd struct 改进 — 可考虑 namespace 化

当前 `PerfSmokeUd` struct 在 hello_devtool.cc 顶层 + static 实例 `s_perf_ud`。**潜在改进**：包装到 `examples::perf_smoke` namespace 减少污染。**当前评估**：P3 / 不必要 — hello_devtool.cc 是 example 级代码 / 单 file scope / static linkage / 0 命名冲突风险。

---

### 3.f 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | ✅ | `view` NULL guard 必跑（A.1 反向探针 1 SEGFAULT 实证）|
| 认证/授权 | N/A | 公开 ABI / 不涉及权限模型 |
| 数据保护（加密/脱敏） | N/A | 不涉及敏感数据 |
| 依赖审计 | N/A | 0 新依赖（仅暴露既有 UpdateManager::Invalidate）|
| 错误信息脱敏 | ✅ | 错误返回值 VX_OK / VX_ERROR_NULL_PARAM / VX_ERROR_INVALID_STATE 全部为枚举值 / 无字符串泄露 |
| 敏感数据处理 | N/A | 不涉及 |
| **新威胁面** | ✅ **0 新威胁面** | 仅暴露既有 dirty_=true 操作 / 无 raw memory access / 无任意代码执行路径 |
| **威胁评估** | ✅ | 主线程语义 lock-down（D2-A）/ idempotent contract（多次调用等价 1 次）/ DevTool subsystem 不被影响（D1-A 路由）|

**结论：** 本任务**不涉及安全变更**（无新威胁面 / 0 dependency 引入 / 公开 ABI 扩展但仅暴露既有内部能力）。

---

## 4. 反复模式识别

| # | 已知反复模式 | 出现频率 | 本次状态 | 抑制证据 |
|:-:|---|:-:|:-:|---|
| 1 | 计划文件清单与实际变更不一致 | 9+ | ✅ 抑制 | 8 计划文件 100% 命中 / 0 未列入文件 / 行数偏差 +20-50% 属正常注释扩张 |
| 2 | 子代理产出需大量返工 | 7+ | N/A | 单 agent 直跑（适合 Level 2）|
| 3 | 前置依赖/环境/API 能力未验证 | 8+ | ✅ 抑制 | Phase 0 audit 11 子段全预证 |
| 4 | 非默认路径遗漏验证 | 4+ | ✅ 抑制 | null + fresh + idempotent 全测 |
| 5 | 测试隔离问题 | 7+ | ✅ 抑制 | static atomic counter + reset/cleanup 范式 / 0 flaky |
| 6 | 提交粒度偏离计划 | 7+ | ✅ 抑制 | 5 commits 100% 按计划子阶段分组 |
| 7 | TDD 严格度与场景不匹配 | 11+ | ✅ 抑制 | RED→GREEN→REFACTOR 全跑 / 反向探针强度梯度三档全谱 |

**已知反复模式：0/7 全抑制 ✅**（连续 4 任务保持 0 命中 — TASK-20260503-04 / TASK-20260504-01 / TASK-20260505-01 / 本任务）

**新候选反复模式：** N/A — 本次未触发新反复模式定型条件（所有已知模式全抑制 + 无新失败模式暴露）。

**evidence 升级一览表：**

| 模式 | 升级前 | 升级后 | 备注 |
|---|:-:|:-:|---|
| plan ×0.6 实测系数 | quint-evidence（5 数据点）| **sext-evidence（6 数据点）** | 极速区续延档新子档候选 |
| 跨决策协同度 100% | nona-evidence（9 次）| **dec-evidence（10 次 / 100/100）** | 范式成熟度顶峰 |
| 反向探针强度梯度三档 | dual-evidence（1 任务）| **triple-evidence（2 任务）** | 强度梯度三档全谱稳定可复现 |
| 反复模式 #8 spec 数据回归 | dual-evidence（2 实证）| **triple-evidence（3 实证）** | 已达 writing-plans.mdc 固化阈值 |
| lazy-attach C ABI 容错模式 | triple-evidence（3 任务）| **quad-evidence（4 任务）**| 已成 Veloxa C ABI 默认范式 |
| commit body Source 溯源 | quad-evidence（~43 commits）| **5 commits 累计 ~48 commits** | 已远超 git-workflow.mdc 固化阈值 |

---

## 5. 改进建议（附优先级与落实方式）

| # | 建议 | 优先级 | 落实方式 | 目标 |
|:-:|---|:-:|---|---|
| 1 | systemPatterns.md 「plan ×0.6 实测系数 sext-evidence」入库 — 6 数据点 + 极速区续延档新子档 | **P0** | reflect 阶段立即沉淀 | systemPatterns.md plan ×0.6 矩阵段 |
| 2 | systemPatterns.md 「跨决策协同度 100% sept-evidence → dec-evidence 升级」 | **P0** | reflect 阶段立即沉淀 | systemPatterns.md 跨决策协同段 |
| 3 | systemPatterns.md 「反向探针强度梯度三档 triple-evidence」 | **P0** | reflect 阶段立即沉淀 | systemPatterns.md 反向探针强度梯度段（updates dual → triple）|
| 4 | systemPatterns.md 「反复模式 #8 spec 数据回归 triple-evidence」 + 升级到达 writing-plans.mdc 固化阈值 | **P0** | reflect 阶段立即沉淀 + 标注「下次工作流元任务批量落地」 | systemPatterns.md #8 段 + activeContext.md 待处理 |
| 5 | systemPatterns.md 「lazy-attach C ABI 容错模式 quad-evidence」段落更新 | **P0** | reflect 阶段立即沉淀 | systemPatterns.md lazy-attach 段 |
| 6 | `/plan` 命令固化「plan/spec docs 落盘即 commit」步骤 — TASK-20260505-01 P1 #6 已首次成功实施 ✅ → 现在升级到 P0 立即固化协议到规则文档 | **P0 → 升级**（首次实施成功）| reflect 阶段沉淀范例数据点 + 标注「下次工作流元任务批量落地」固化 | `.cursor/rules/skills/writing-plans.mdc` + `.cursor/commands/plan` |
| 7 | techContext.md 「CMake + GTest 工作流注意事项 — reconfigure 后 ctest 列表更新需要重新 build target」段 | **P2** | reflect 阶段长期沉淀 | techContext.md |
| 8 | systemPatterns.md MVP-B 100% 闭环里程碑沉淀 — 本任务 + TASK-20260505-01 完整连击实证 | **P1** | reflect 阶段沉淀完整收口里程碑数据 | systemPatterns.md MVP 里程碑段 |

**P0 6/6 reflect 阶段直接落实 ✅**（沿用 [TASK-20260505-01 P0 4/4 全落实范式](memory-bank/archive/archive-TASK-20260505-01.md)）
**P1 1/1 reflect 阶段沉淀**
**P2 1/1 长期沉淀**

---

## 6. 技术改进建议

| # | 建议 | 优先级 | 备注 |
|:-:|---|:-:|---|
| 1 | `Application::Invalidate()` 增强为 enum class InvalidateResult | P3 | 当前 bool + C ABI enum 映射已足够 / 不必要 |
| 2 | hello_devtool.cc PerfSmokeUd struct namespace 化 | P3 | 单 file scope / 不必要 |

---

## 7. 关键发现总结（top 3）

1. **MVP-B 100% 闭环里程碑达成 🎉** — B-G1+G2+G3+G4 全 4 项 gap 全部闭环 / dogfood 视觉验证 3/3 PASS / hello_devtool_perf_smoke frames=18（10x 增益 / 完整收口）

2. **范式成熟度三连升级** — plan ×0.6 sext-evidence + 跨决策协同度 dec-evidence + 反复模式 #8 triple-evidence + lazy-attach quad-evidence + 反向探针强度梯度 triple-evidence — **5 个范式同时升级到下一档**（reflection 史上单任务沉淀升级数最高纪录）

3. **「plan/spec docs 落盘即 commit」首次成功实施** — TASK-20260505-01 P1 #6 闭环 / 工作流流程 cleanup / build 阶段 0 collateral commit ✅

---

## 8. 安全评估

**本任务不涉及安全变更**：
- ❌ 0 新依赖
- ❌ 0 新 IO / 0 新 syscall
- ❌ 0 新外部输入路径
- ✅ 公开 ABI 扩展但仅暴露既有 `UpdateManager::Invalidate`（`dirty_=true` 1 行操作）/ 不引入新威胁面
- ✅ NULL guard + INVALID_STATE 守门 + idempotent 契约 + 主线程 lock-down 全在 ABI 设计中显式声明
- ✅ 错误信息全为枚举值 / 无字符串泄露
- ✅ DevTool subsystem 不被影响（D1-A 路由决策）

详细安全评估见 [§3.f](#3f-安全评估) 表。
