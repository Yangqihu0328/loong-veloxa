# 回顾：TASK-20260503-03 DevTool 三件套主线收官 — 4 项 P3 候选批量清零

**日期：** 2026-05-03
**任务 ID：** `TASK-20260503-03`
**复杂度级别：** Level 2（多文件修改 / 需求清晰 / 4 项小型清理 / 无新组件 / 无设计决策）
**安全相关：** ❌ 否（无新外部输入处理 / 无认证 / 无数据存储 / P3 #3 三元守卫属代码 robustness 改进）
**任务定位：** **主线收官 + P3 候选批量清零混合任务** — DevTool 三件套（A/B/C）已 archive 闭环后的「装裱 + 文档化 + 反模式清理」收官批次

---

## 1. 计划 vs 实际

### 1.1 总览对比

| 维度 | 计划（B1-B9 锁定） | 实际 | 偏差原因 |
|------|------|------|---------|
| 子任务数 | 4 P3 + 1 finalize | **3 P3 完成 + 1 P1 取消 + 1 finalize** | **P1 实施失败 → 拆细化为新 P3 候选** |
| Checkpoint 数 | CP1 + CP2 | CP1 ✅ + **CP2 跳过**（P1 取消后合并到 P2 末步）| P1 取消连锁 |
| 估时（plan ×0.6） | ~50-80 min（调整后 3/4 项） | **~46 min** | **0.30-0.45× 极窄档延续高效区下沿** ✅ |
| commit 数 | 4 P3 + 1 chore = 5 | 4 P3 + 1 chore = 5 | ✅（P3.1 / P3.2 / P2 / P4 / finalize） |
| 文件变更 | 5 文件 | 5 文件 + plan + MB（finalize chore 内）| ✅ 范围 |
| ctest baseline | DEVTOOL=ON 1247 不退化 | DEVTOOL=ON 1247/1247 PASS + SDL2=ON 3/3 hello_devtool_*_smoke PASS | ✅ + 主动扩大 SDL2 验证 |
| Lint 错误 | 0 | 0（9 文件 全清）| ✅ |
| **反复模式命中** | **0**（VAN 锁定「连续第 4 次零反复」目标）| **1 次（#1 命中）** | **❌ 破坏纪录** |

### 1.2 子任务实测耗时矩阵

| # | 子任务 | plan ×0.6 估时 | 实测 | 比值 | 备注 |
|:-:|---|:-:|:-:|:-:|---|
| 1 | P3.1 三元守卫 fix `devtool_dogfood_smoke_test.cc:106` | ~10 min | ~3 min | 0.30× | 单次 StrReplace + ctest 4/4 + commit |
| 2 | P3.2 三元守卫 fix `file_watcher_test.cc:314` | ~10 min | ~3 min | 0.30× | 同 P3.1 模式 |
| - | CP1 自审 | ~3 min | ~2 min | 0.67× | grep 验证 + ctest 1247 + 2 commits Source |
| 3 | ~~P1 多帧验证~~ | ~30-60 min | **~12 min（含失败发现 + 回退 + 调整）** | — | **❌ 取消（dirty_ 机制硬约束）** |
| 4 | P2 三件套装裱 | ~15-30 min | ~8 min | 0.27-0.53× | 双文件 StrReplace + cmake build + ctest 1247 + SDL2=ON 3/3 |
| 5 | P4 README 章节补强 | ~15-20 min | ~6 min | 0.30-0.40× | Write 72 行 + python3 验证 |
| 6 | finalize | ~10-15 min | ~12 min | 0.80-1.20× | activeContext + progress + tasks + plan 调整较多 |
| **合计（去除 P1 取消代价）** | - | **50-80 min** | **~46 min** | **~0.58×** | **若 P1 不踩坑约 30-35 min（0.42×）— 与 0.30-0.45× 极窄档延续高效区一致** |

### 1.3 文件变更对比

| 文件 | plan §1.1 预估 | 实际 | 偏差原因 |
|---|:-:|:-:|---|
| `tests/integration/devtool_dogfood_smoke_test.cc` | +1/-1 行 | +2/-1 行 | 三元守卫多行格式（一致性）|
| `tests/devtool/hot_reload/file_watcher_test.cc` | +1/-1 行 | +2/-1 行 | 同上 |
| `tests/CMakeLists.txt` | +15-25 行（P1+P2 合）| **+19/-3 行（仅 P2，P1 已回退）** | P1 取消减负 |
| `examples/hello_devtool.cc` | +20-30 行（P1+P2 合）| **+30/-16 行（仅 P2 三件套 docstring + cross-links）** | P1 取消减负 |
| `README.md` | +50-80 行 | **+72/-2 行** | ✅ 在预估范围 |
| `docs/plans/2026-05-03-devtool-trio-finalize.md` | +500-600 行 | **+779 行（plan 创建 + P1 失败说明 ~30 行追加）** | plan 文档加 P1 调整说明 |

### 1.4 设计变更（plan vs 实施偏差）

| # | 项 | plan | 实际 | 评估 |
|:-:|---|---|---|:-:|
| 1 | P1 子任务实施 | autoquit 600ms + regex `frames=([3-9]\|[1-9][0-9]+)` + frames>=3 守门 | **完整回退 + 拆细化为 P3 候选 #0** | ❌ **P1 plan 阶段假设错误**（dirty_ 机制硬约束未识别） |
| 2 | CP2 自审执行 | P1+P2 完成后 ctest 验证不退化 + 三件套综合 smoke | **跳过**（P1 取消后合并到 P2 末步 ctest verify） | ✅ 良性偏差（避免重复验证） |
| 3 | P3 task #3 audit | VAN 阶段已预跑 8→2 减负 | 5 处命中 / 2 处实际反模式 / 全部 fix | ✅ 与预跑完全一致 |
| 4 | commit body 格式 | 未规定 | 5 commits 全部含 `Source: TASK-XXXXXXXX-XX §X` 前置溯源 + 实测数据 + 自然语言根因 | ✅ 自发实践 TASK-20260503-02 范式 |
| 5 | P2 内容 | ctest 三段注释 + hello_devtool.cc 文件头 docstring | 同 plan + **追加「dirty_-guard short-circuit 注释段」**（P1 失败教训内联到 ctest 注释） | ✅ failure-as-data 即时沉淀 |

---

## 2. 回顾检查清单（代码变更 + 反复模式抑制）

### 2.1 代码变更类维度

- [x] **计划精确度** — 4 项 P3 中 3 项文件清单与实际变更完全一致；P1 因 plan 假设错误失败（plan 内文件清单准确但实施前提错误）
- [N/A] **TDD 执行情况** — 4 项 P3 全部 [覆盖补充] / [文档调整模式]，无新逻辑，无 RED-GREEN-REFACTOR
- [N/A] **子代理质量** — 本任务未使用子代理（Level 2 + 子任务粒度小）
- [x] **测试隔离** — DEVTOOL=ON build/ 与 SDL2=ON build-sdl2/ 独立配置（避免污染 1247 baseline 测试集）✅
- [x] **提交粒度** — 5 commits 严格 1 commit/子任务（B6 锁定）✅
- [x] **非默认路径** — DEVTOOL=ON 1247 + SDL2=ON 3 hello_devtool_*_smoke 双配置验证（plan §3.1 仅要求 DEVTOOL=ON，主动扩展）✅

### 2.2 反复模式抑制

- [ ] **现有引擎/库 runtime 行为假设是否已实证？** ❌ **本次失败**（P1 假设「调 autoquit ms = 自然多帧」未做 5min baseline smoke 实测）
- [x] **依赖/环境/API 存在性** ✅ VAN 阶段全 grep 验证（rg/jq/python3 + cmake/ninja/gcc/ld 工具链 + FetchContent _deps 预置）
- [x] **commit body 溯源** ✅ 5/5 commits 含 Source 前缀

### 2.3 安全评估

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | 无新外部输入处理 |
| 认证/授权 | N/A | 无认证逻辑 |
| 数据保护（加密/脱敏） | N/A | P4 README 不含敏感数据 |
| 依赖审计 | N/A | 无新依赖（plan §3.1 已确认 build/_deps 预置）|
| 错误信息脱敏 | N/A | 测试代码 ASSERT_TRUE message 已三元守卫规范 |
| 敏感数据处理 | N/A | hello_devtool.cc dogfood 范例数据均为示例 |

**结论：** 本任务不涉及安全变更（与 plan §B 安全相关 ❌ 标注一致）。P3.1/P3.2 三元守卫修复属代码 robustness 改进（防止 GoogleTest 短路评估变更时崩溃），不在安全边界内。

---

## 3. 做得好的（5 项）

### 3.1 VAN/Plan Phase 0 audit 预跑 — 第 5 次实证 quad → quint-evidence

**做得好：** VAN 阶段对 P3 task #3「GoogleTest 三元守卫 audit」做了完整预跑：
- grep 5 个 ASSERT_TRUE pattern 命中
- 逐个 Read 上下文判定（仅 2 处实际反模式 / 1 处三元守卫范本 / 2 处安全模式）
- 把 audit 表写入 `activeContext.md` + plan 文档 — build 阶段直接拿表执行，0 audit 时间消耗

**收益：** P3 task #3 plan 估时 ~10 audit + ~20-30 fix → 实际 fix ~6 min 完成（仅 plan 估时的 0.20×）。

**模式实证：** 「Phase 0 投入越深 / build phase 越快定律」第 5 次实证（前 4 次：TASK-20260502-01 / TASK-20260502-02 / TASK-20260503-01 / TASK-20260503-02）— **quint-evidence**（5 任务连续验证），从「定律 hypothesis」固化为「定律 law」。

### 3.2 P1 失败后快速回退 + 实证 + 用户决策路径明确

**做得好：** P1 实施失败发现路径清晰：
1. **手测发现（~3 min）** — autoquit 600ms / 1500ms 均 frames=1
2. **根因定位（~2 min）** — Read `update_manager.cc:17` 找到 `if (!dirty_) return;` 短路 + `transition_mgr_.HasActive()` 是唯一 rearm 源
3. **完整回退（~1 min）** — StrReplace 双文件恢复 main 5667c8c 原文，git status 验证干净
4. **AskQuestion 3 候选方案（~5 min 含等待用户）** — A 最诚实 / B 文档化 / C 新 ABI 超 P3 范围，用户 0 ambiguity 选 A
5. **plan + activeContext 调整（~5 min）** — P1 标取消 + 拆细化为 P3 候选 #0「Performance Overlay 持续 invalidate 机制」

**收益：** 失败到调整完成总耗时 ~16 min，避免「强行让测试 PASS」的伪绿陷阱。

**沉淀价值：** 实证「failure-as-data 模式」在 build 阶段实施失败场景下的标准链路 — 失败 → 根因定位 → 回退 → AskQuestion → 拆细化 → 沉淀（5 步）。可与 TASK-20260424-03「负结果资产化模式」叠加。

### 3.3 commit body Source 溯源 + 实测数据格式 — 5/5 全实践

**做得好：** 5 commits 全部 commit body 含：
- 第 1 行：`Source: TASK-XXXXXXXX-XX §X reflection / build / candidate` 溯源前缀
- 中段：实测数据（如 "DEVTOOL=ON 1247 + SDL2=ON 3/3 PASS" / "+72 行 / 7 cross-links"）
- 末段：自然语言根因 / 设计决策记录（如 P3.1 解释 GoogleTest 短路评估机制 / P2 解释 dirty_-guard short-circuit 注释段的 failure-as-data 来源）

**收益：** commit-as-mini-log 模式 — 不打开 reflection / archive 即可知 commit 全貌，git log --oneline 可读性 ×3。

**模式延续：** TASK-20260503-02 6 docs commits 已自发实践 Source 溯源；本任务 5/5 进一步实践 → 模式成熟度 **double-evidence**（2 任务 11 commits 累计），可固化到 `git-workflow.mdc`（见 §6 改进建议 #4）。

### 3.4 P2 failure-as-data 即时沉淀 — ctest 注释内联 P1 失败教训

**做得好：** P2 子任务实施时，主动把 P1 失败发现的 dirty_ 机制硬约束**内联到 ctest hello_devtool_perf_smoke 的 PROPERTIES 注释段**（不仅仅是 Phase B 标识装裱）：
> Note: in dummy SDL + static CSS the UpdateManager::Update() dirty_ guard short-circuits after the first frame (transition_mgr_.HasActive() is the only rearm source), so frames=1 is the natural steady state — it validates the ABI surface end-to-end, not the multi-frame ring-buffer aggregation. Multi-frame validation tracked as a P3 candidate.

**收益：** 下次任务无需查 reflection/plan 文档即可在 ctest 现场看到「为什么 frames=1 是 OK 的」+ 「多帧验证为什么是 P3 候选」 — 知识就近沉淀，避免「为什么这个 regex 是 N>=1 不是 N>=3」的下次再被问起。

**模式实证：** 「就近沉淀模式」第 N 次实证 — 与 TASK-20260503-01 hot_reload_manager.cc 注释内联 warning 语义层教训同源。

### 3.5 SDL2=ON 主动扩展验证 — 超 plan §3.1 默认

**做得好：** plan §3.1 明确「默认仅 verify 当前 build 目录的 DEVTOOL=ON 1247（避免 cmake reconfigure 成本）；OFF / SDL2 配置在 /reflect 阶段如有时间可补 verify」。但 P1 实施过程中已经新建 build-sdl2/（SDL2=ON 配置）来跑 hello_devtool_perf_smoke；P2 完成后**主动复用 build-sdl2/** 验证三件套 hello_devtool_*_smoke 全 3/3 PASS。

**收益：** P2 三件套装裱（注释 + docstring）的实证扩大到「三件套 SDL2 ctest 全通过」级别 — plan §3.1 acceptance 矩阵的 SDL2=ON 行从「N/A skip」升级为「3/3 PASS」。

**反思：** 本来是 P1 的「副产品」（SDL2=ON build-sdl2 已配置 + hello_devtool 已编译），未浪费机会沉没。

---

## 4. 遇到的挑战（核心：反复模式 #1 第 9 次命中）

### 4.1 P1 实施失败 — 反复模式 #1「前置依赖/环境/API 能力未验证」第 9 次命中

**挑战陈述：**

VAN/plan 阶段假设「调 `VX_HELLO_DEVTOOL_AUTOQUIT_MS` 200→600ms + 收紧 PASS_REGULAR_EXPRESSION 到 `frames=([3-9]|[1-9][0-9]+)` 即可获得多帧验证」。实测：autoquit 600ms / 1500ms 均 frames=1。

**根因定位（build 阶段实证）：**

```16:18:veloxa/core/update_manager.cc
void UpdateManager::Update() {
  if (!dirty_ || !config_.document) return;
```

```69:73:veloxa/core/update_manager.cc
  display_list_ = std::move(new_list);
  dirty_ = false;

  if (transition_mgr_.HasActive()) {
    dirty_ = true;
  }
```

`UpdateManager::Update()` 第 17 行 `if (!dirty_) return;` 是 frame hooks 触发的硬约束 — `dirty_` 在每帧末尾 reset（第 69 行），仅当 `transition_mgr_.HasActive()` 才 rearm（第 71-73 行）。`hello_devtool` 的 CSS 静态（无 transition / animation）→ 第 1 帧后 `dirty_=false` 永久阻断 hooks。

**反复模式 #1 历史频率：**

| 历史命中 | 任务 | 维度 |
|---|---|---|
| 1-8 次 | TASK-20260405-* / TASK-20260413-* / TASK-20260418-01 / TASK-20260419-* / TASK-20260424-* / TASK-20260430-04 / TASK-20260502-01 等 | 主要为「依赖包未装 / API 未实现 / 环境工具缺失」 |
| **9 次** | **TASK-20260503-03 P1**（本任务） | **「现有实现的 runtime 行为」假设未实证** |

**新维度发现：** 反复模式 #1 的覆盖范围**应当扩展**到包括「**调参/调阈值/调时间类假设**的 runtime 行为未验证」 — 不只是「依赖/API/环境是否存在」。

**根因深挖：**

- VAN/plan 阶段对反复模式 #1 的检查机制：grep 看到 `set_pipeline_hooks` C ABI + `OnFrame` 调用链 + `update_manager_->Update()` 调用 — 全部存在 ✅
- **但 `update_manager.cc:17` 的 dirty_ 短路语义需要 Read 源码 + 语义理解才能识别**，grep 检查器不能发现
- 旧 mental model：plan 阶段做 grep 检查 → 「API 能力存在 = 假设可成立」
- **修正后 mental model**：plan 阶段对「调参类子任务」必须额外做 **5min baseline smoke 实测**（哪怕粗糙地跑一次旧 baseline 看输出，也能立即发现 frames=1 是天然结果）

**破坏纪录：** TASK-20260503-02 后「连续第 4 次零反复」纪录被破坏（实际：1/7 部分命中）— 提醒「零反复」是脆弱状态，需要持续主动机制（不只是被动 checklist）。

### 4.2 P1 cmake reconfigure 成本未在 plan §3.1 计入

**挑战陈述：** plan §3.1 acceptance 矩阵把 SDL2=ON 配置标为「N/A skip + reflect 阶段补」，但 P1 子任务实际**强依赖** SDL2=ON 配置才能跑 hello_devtool_perf_smoke（hello_devtool example 仅 SDL2=ON 编译）。build 阶段被迫新建 build-sdl2/ 配置，cmake reconfigure 75s + build hello_devtool 33s = ~108s 隐藏成本未在 plan 估时中。

**收益侧面：** P2 末步主动复用 build-sdl2/ 扩大 SDL2 验证（见 §3.5），部分摊薄了成本。

**教训：** Plan 估时矩阵需要把「子任务依赖的 build 配置」明确 — 若需新配置，cmake reconfigure 时间应单独计入（~60-90s for clean reconfigure with FetchContent 预置）。

### 4.3 「连续第 4 次零反复」纪录脆弱性提醒

**挑战陈述：** TASK-20260502-01 / TASK-20260502-02 / TASK-20260503-01 / TASK-20260503-02 连续 4 次零反复 → TASK-20260503-03 反复模式 #1 命中。「零反复」状态对**新任务类型**（本任务首次涉及「runtime 行为调参」）的脆弱性暴露。

**根因：** 反复模式 #1 的 SOP 主要针对「依赖/API/环境存在性」(已成熟)，对「现有实现 runtime 行为假设」缺乏专项检查器。

**教训：** 「零反复」不是稳态而是**主动维护态** — 当任务涉及新维度时（如本任务的「调参/调阈值类」），需要**预先扩展反复模式 #1 的检查器矩阵**，而不是等到失败后回顾时才扩展。

---

## 5. 经验教训（4 项）

### 5.1 L1 — 调参/调阈值/调时间类子任务需 5min baseline smoke 实证（升级反复模式 #1 适用范围）

**新教训：**

- **触发条件：** plan 阶段子任务涉及「调整现有参数 / 阈值 / 时间常量 / 配置值」假设可获得某种 runtime 行为变化
- **专项检查：** plan 阶段必须做最小 baseline smoke 实测（5 min 投入），跑一次现有 baseline 看实测输出，确认假设可成立
- **替代检查：** 若无法做 baseline smoke，必须 Read 相关 runtime 行为源码（不只 grep API 存在性）确认机制路径
- **反例（本任务）：** P1 假设 autoquit 200ms→600ms 自然获得 frames=1→~36；实际 dirty_ 机制硬约束下 autoquit 多大都是 frames=1
- **正例（VAN 阶段做对的）：** P3 task #3 audit 预跑 — 假设「8 处反模式」做实证后修正为「2 处反模式」，build 阶段 0 浪费

**ROI 公式：**

```
plan 5min baseline smoke 投入 → 节省 build 阶段 (反复模式命中代价 ~30-60min + 用户 AskQuestion ~5-10min + plan 调整 ~5-10min) = ~40-80min ROI 8-16×
```

### 5.2 L2 — frame hooks 多帧验证 = update_manager.dirty_ 机制依赖（沉淀引擎语义到 techContext）

**引擎语义沉淀：**

- `UpdateManager::Update()` 第 17 行 `if (!dirty_ || !config_.document) return;` 短路 → 静态 CSS 场景第 1 帧后永久 frames=1
- `dirty_` rearm 路径 4 个：
  1. `Application::InjectInput → event_manager.HandleInput → UpdateManager::Invalidate()`（用户输入）
  2. `transition_mgr_.HasActive()`（active CSS transition；第 71-73 行 rearm）
  3. CSS animation（如果引擎支持，本任务未验证）
  4. **公开 `vx_view_invalidate()` C ABI**（待立项 — TASK-20260503-03 P3 候选 #0）
- **影响：** 所有 frame-hook based 子系统（perf overlay / hot reload / 任何 PipelineHooks 用户）的 smoke 测试必须确保有持续 invalidate 源
- **设计 trade-off：** dirty_ 短路是 Veloxa 「脏区优化」的核心 — 节省静态场景 CPU 占用（车载嵌入式场景关键）；持续 invalidate 是付费扩展能力，不是默认行为

### 5.3 L3 — failure-as-data，拒绝伪绿（强化既有原则）

**强化教训：**

- P1 实施失败发现 frames=1 是 dirty_ 机制硬约束后，本能反应是「调到 PASS 为止」（如保留 PASS_REGULAR_EXPRESSION 为 N>=1 + autoquit 600ms 看起来更宽松「装作」是多帧）
- **正确选择：** 完整回退 + AskQuestion 让用户决策 + 拆细化为 P3 候选
- **收益：**
  - 诚实记录「dirty_ 机制硬约束」(失败发现) 是后续 P3 候选 #0 的设计前提
  - P2 ctest 注释内联失败教训（见 §3.4），知识就近沉淀
  - 留出 P3 候选 #0「Performance Overlay 持续 invalidate 机制」深度方案空间（2 candidate paths：(a) 新 vx_view_invalidate API / (b) hello_devtool 注入 CSS animation）

**模式叠加：** TASK-20260424-03「负结果资产化模式」+ 本任务「failure-as-data 即时沉淀」= **失败处理双层模式**（事后回顾 + 事中即时沉淀）。

### 5.4 L4 — commit body Source 溯源 + 实测数据格式可固化

**新模式实证：**

- TASK-20260503-02 已自发实践 6 docs commits Source 前置溯源
- TASK-20260503-03 进一步实践 5 commits 全部含 Source + 实测数据 + 自然语言根因
- **double-evidence**（2 任务 11 commits 累计） → 模式成熟度足够固化

**建议固化格式（git-workflow.mdc）：**

```
<type>(<scope>): <subject> (<task-tag>)

Source: TASK-XXXXXXXX-XX <reflection §X | build | candidate | etc>.

<2-4 句根因/设计决策记录>

<可选：实测数据，如 "DEVTOOL=ON N/N PASS" / "+L 行 / +R 重构">
```

---

## 6. 改进建议（4 项）

| # | 建议 | 优先级 | 落实方式 | 目标 | 状态 |
|---|------|--------|---------|------|:-:|
| 1 | `.cursor/rules/skills/writing-plans.mdc` 新增「调参/调阈值/调时间类子任务专项 checkbox」段：「现有 baseline 是否已做 5min smoke 实测？预期偏移机制是否已 Read 源码确认（不只 grep）？」 | **P0**（**反复模式 #1 第 9 次命中需固化**）| 修改规则文件 → 在 plan §3 子任务定义模板中追加 checkbox | 防止反复模式 #1 在「调参」子维度复发 | ⏸️ archive 前落实 |
| 2 | `memory-bank/systemPatterns.md` 新增段「调参/调阈值类子任务的 5min baseline smoke 实证规则」（含决策矩阵 + 反例本任务 + 正例 P3 audit 预跑） | **P1** | 追加 systemPatterns 新段 + 引用本 reflection | 长期沉淀 + 反复模式 #1 适用范围扩展 | ⏸️ archive 阶段 |
| 3 | `memory-bank/techContext.md` 新增「Veloxa 引擎核心机制注意点」段 — 含 `update_manager.dirty_` 短路 + transition rearm + frame hooks 触发要求 + 4 个 invalidate 源 | **P1** | 追加 techContext 新段 | 防止 frame-hook based 子系统 smoke 设计踩坑（perf overlay / hot reload / 任何 PipelineHooks 用户）| ⏸️ archive 阶段 |
| 4 | `.cursor/rules/skills/git-workflow.mdc` 固化 commit body Source 溯源 + 实测数据格式 | **P2** | 追加 commit message 范本段 | commit-as-mini-log 长期模式（双 evidence） | ⏸️ 下次工作流元任务批量落地 |

**P0 落实方式（archive 前必做）：**

`writing-plans.mdc` plan §3 子任务定义模板新增：

```markdown
**子任务类型分类（必填一项 + 触发对应 checklist）：**

- [ ] [新逻辑实施] — 执行 RED-GREEN-REFACTOR
- [ ] [覆盖补充] — 既有代码新增测试覆盖
- [ ] [文档调整模式] — 纯注释/规则/spec 修改
- [ ] **[调参/调阈值/调时间] — ⚠️ 必须叠加 5min baseline smoke 实证：**
  - [ ] 现有 baseline 是否已实测？（输出：[实测命令] + [实测结果]）
  - [ ] 预期偏移机制是否已 Read 源码确认（不只 grep API 存在性）？
  - [ ] 若 baseline 无法跑（依赖大型 reconfigure 等），是否已识别 + plan 估时计入？
  - 反例：TASK-20260503-03 P1（dirty_ 机制硬约束未识别）
  - 正例：TASK-20260503-03 P3 task #3（audit 预跑 8→2 减负）
```

**反复模式表升级（落实 P1 #2）：**

| 已知模式 | 历史频率 | 适用子维度 |
|---------|---------|---------|
| 前置依赖/环境/API 能力未验证 | **9+ 次**（TASK-20260503-03 P1 升级）| (a) 依赖包/编译器/工具链存在性 + (b) **现有实现 runtime 行为假设** ⚠️ |

---

## 7. 技术改进建议

### 7.1 P3 候选 #0「Performance Overlay 持续 invalidate 机制」立项策略

**已入 activeContext.md「P3 候选 — 来自 TASK-20260503-03 build 阶段 P1 实施失败拆细化」段。**

**2 candidate 路径权衡：**

| 路径 | 复杂度 | 通用性 | 实施代价 | 推荐 |
|---|:-:|:-:|:-:|:-:|
| (a) 新增公开 `vx_view_invalidate()` C ABI | Level 2-3 | ⭐⭐⭐（embedder 通用） | ~1-2h（含 testability + A14 黑名单更新 + 文档）| 真实 embedder 需求驱动时立项 |
| (b) `hello_devtool` 注入 CSS animation | Level 1-2 | ⭐（仅 dogfood）| ~30-60 min（注入 `@keyframes` + `animation`）| **建议先做** — 验证机制 + 低成本闭环 |

**建议次序：** 先做 (b) 让 dogfood smoke 升级到真多帧验证 → 后续若 embedder 有持续 invalidate 需求再做 (a)。

### 7.2 frame-hook based 子系统 smoke 设计 SOP

**新 SOP（沉淀到 techContext.md「Veloxa 引擎核心机制注意点」段）：**

任何 frame-hook based 子系统（perf overlay / hot reload / debug overlay / 用户 PipelineHooks）的 smoke 测试设计必须满足以下任一条件：

1. **持续输入源** — 测试 fixture 周期注入用户输入（`Application::InjectInput`）触发 `UpdateManager::Invalidate()`
2. **持续 transition 源** — 测试 fixture 在 DOM 中注入 active CSS transition（如 :hover 触发 + 周期 hover/unhover）
3. **公开 invalidate API**（待立项）— 测试 fixture 在 hook 回调中调用 `vx_view_invalidate()` force rearm
4. **明确接受 frames=1 ABI smoke 语义** — 在 ctest 注释中**显式说明** smoke 仅验证 ABI 表面，不验证多帧 ring buffer 聚合（如本任务 P2 末步处理）

---

## 8. 反复模式识别（27+ 份历史回顾累计统计）

| 已知模式 | 历史频率 | 本次是否重复？ | 升级动作 |
|---------|:-:|:-:|---|
| 计划文件清单与实际变更不一致 | 9+ 次 | ❌ 否（5 文件清单准确）| — |
| 子代理产出需大量返工（CMake/编译/上下文不足） | 7+ 次 | N/A（未用子代理）| — |
| **前置依赖/环境/API 能力未验证** | **8 次 → 9+ 次** | ✅ **是**（P1 dirty_ 机制硬约束未识别）| **P0 改进建议 #1 + P1 #2** |
| 非默认路径（流式/错误/缓存）遗漏验证 | 4+ 次 | ❌ 否（DEVTOOL=ON + SDL2=ON 双配置）| — |
| 测试隔离问题（flaky/并行冲突/环境依赖）| 7+ 次 | ❌ 否（build/ + build-sdl2/ 独立配置）| — |
| 提交粒度偏离计划（大杂烩提交） | 7+ 次 | ❌ 否（5 commits 1 commit/子任务）| — |
| TDD 严格度与场景不匹配 | 11+ 次 | N/A（4 项全 [覆盖补充] / [文档调整模式]）| — |

**本次新沉淀（独立条目）：**

- 反复模式 #1 适用范围扩展到「**现有实现 runtime 行为假设未实证**」子维度（不只是依赖/API/环境）

---

## 9. 安全评估

**结论：** 本任务不涉及安全变更（与 plan §B 安全相关 ❌ 一致）。

详见 §2.3 表。

---

## 10. plan ×0.6 矩阵新数据点

| 任务 | 类型 | plan ×0.6 估时 | 实测 | 比值 | 备注 |
|---|---|:-:|:-:|:-:|---|
| TASK-20260503-03 整体 | 主线收官 + P3 批量清零（混合：覆盖补充 + 文档调整模式 + 失败回退） | 50-80 min | ~46 min | **0.58×** | 含 P1 失败 ~12min 代价 |
| TASK-20260503-03 去除 P1 代价 | 同上 | 50-80 min | ~30-35 min | **0.42×** | **0.30-0.45× 极窄档延续高效区** ✅ |

**矩阵桶位：**

- 「P3 批量清理」桶：TASK-20260503-02（0.21×）+ TASK-20260503-03（0.42-0.58×）= **0.21-0.58× 区间**（首次双数据点）
- 「极窄档延续高效区 0.30-0.45×」桶：TASK-20260503-03 去除 P1 代价 0.42× ✅ **首次数据点入库**
- 「失败回退代价」桶：TASK-20260503-03 P1 ~12min（含手测 ~5min + 根因定位 ~2min + 回退 ~1min + AskQuestion 等待 ~5min + plan 调整 ~5min）— **新数据点入库**，可作为反复模式 #1 命中代价基线

---

## 11. 总结

**关键成果：**
- 5 commits 完成 3 P3 + 1 finalize + 1 P2 装裱（DevTool 三件套主线收官 ✅ 装裱完成）
- ctest 双配置全 PASS（DEVTOOL=ON 1247/1247 + SDL2=ON 3/3 hello_devtool_*_smoke）
- README 从 2 行扩展到 72 行 + 7 处交叉链接
- audit 表预跑 5→2 减负（quint-evidence 实证 Phase 0 投入定律）

**关键失败：**
- P1 子任务取消 — 反复模式 #1「前置依赖/环境/API 能力未验证」第 9 次命中（**新维度**：现有实现 runtime 行为假设）
- 破坏 TASK-20260503-02 后「连续第 4 次零反复」纪录

**关键沉淀：**
- L1 「调参/调阈值/调时间类子任务需 5min baseline smoke 实证」 — P0 改进建议（archive 前必落实到 writing-plans.mdc）
- L2 「frame hooks 多帧验证 = update_manager.dirty_ 机制依赖」 — P1 改进建议（archive 阶段沉淀到 techContext）
- L3 failure-as-data 双层模式（事后回顾 + 事中即时沉淀） — 强化既有原则
- L4 commit body Source + 实测数据格式 — double-evidence，可固化到 git-workflow.mdc（P2）

**下一步：** 调用 `/archive` 进入归档阶段（重点：P0 #1 落实到 writing-plans.mdc + P1 #2 sysPatterns + P1 #3 techContext 三段沉淀同步落地，**确保反复模式 #1 适用范围升级到 9+ 次后被规则文件吃掉**）。
