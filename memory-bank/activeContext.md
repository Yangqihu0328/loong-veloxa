# 活跃上下文

## 当前阶段

**规划中** — TASK-20260505-02 Performance Overlay 持续 invalidate 机制（B-G4 — MVP-B 收口最后一项）Plan ✅，待 `/build`。

**Plan 阶段产出（2026-05-05 ~15:15）：**

- **设计文档：** `docs/specs/2026-05-05-perf-overlay-invalidate-api-design.md`（11 段 / 完整设计 + D1+D2+D3+D4 决策完整定义 + 4 风险登记）
- **实现计划：** `docs/plans/2026-05-05-perf-overlay-invalidate-api.md`（5 Phase / 8 任务 / Phase 0 含 11 audit 子段 / 4 单测 TDD 设计 / commit 范本）
- **跨决策协同度 100% 第 10 次连续命中** — 1 次 AskQuestion 锁定 D1-A + D2-A + D3-A + D4-A 四决策（累计 100/100 跨决策一次锁定纪录 / nona → **dec-evidence** 升级）
- **决策矩阵：**
  - **D1-A** 仅 target update_manager_ 路由（DevTool 独立状态机 / 实现最简）
  - **D2-A** main thread only（与 LoadHTML/InjectInput 一致 / Doxygen 明示）
  - **D3-A** hello_devtool on_frame_end hook 调 vx_view_invalidate（PerfSmokeUd struct + userdata 通道 / 0 新机制）
  - **D4-A** 完整 4 单测（null / fresh INVALID_STATE / 正常路径 / idempotent）
- **关键 Phase 0 audit 发现（11 子段）：**
  1. CSS animation 不可行（引擎不支持 `@keyframes`）→ 决定路径 (a)
  2. UpdateManager::Invalidate 已就绪（update_manager.cc:14）/ Application 仅缺 `Invalidate()` 公开方法
  3. on_frame_end hook 时序严格安全（dirty_=false reset → transition rearm → on_frame_end fire）→ hook 内调 invalidate 不依赖中间状态变更
  4. A14 守门不受影响（公开 ABI / 不属 DevTool subsystem）
  5. 既有 hello_devtool_perf_smoke regex `[1-9][0-9]*` ≥1 帧 → 升级到 `([2-9]|[1-9][0-9]+)` ≥2 帧

**当前任务：** TASK-20260505-02 — `vx_view_invalidate()` 公开 C ABI / Level 2 / 分支 `feature/TASK-20260505-02-perf-overlay-invalidate-api`（基于 main `8caa9ba`）

**预期：**
- ctest 期望 DEVTOOL=ON 1298 → 1302（+4）/ DEVTOOL=OFF 1105 → 1109（+4）
- plan ×0.6 实测期待 ~0.20-0.31×（落极速区 0.10-0.20× 续延候选 / quint → sext-evidence 候选）
- 反复模式预期 0/8 全抑制

**下一步：** `/build` — 进入构建阶段，按 Phase A → B → C → D → E 5 个 phase 严格 TDD 实施。

---

## 上次任务（已归档闭环）

### TASK-20260505-01 DomBindings R2 收口（B-G1 children + B-G3 innerHTML + B-G2 audit）— ✅ 已归档（commit `6f924fd`）

**最近闭环（保留供下游任务参考）：** **MVP-B 完成度 90% → 95%** ✅ + dogfood 视觉自动恢复链路三件齐 ✅ + Veloxa JS API surface 显著扩张（children getter + innerHTML setter + 4 mouse alias）+ `CloneNodeInto` helper 就位（未来 cloneNode/Range/Fragment 复用基础）+ 协议三件套里程碑（Phase 0 极速区 **quint-evidence** 5 数据点 / 跨决策协同度 **nona-evidence** 第 9 次连续 100% / 反向探针强度梯度 **dual-evidence** 三档全谱）+ 反复模式 **#8 spec 数据回归 dual-evidence 入库定型** ✅。

**MVP-B 即将收口** — 仅剩 B-G4 Performance Overlay 持续 invalidate 机制（~30 min-2 h plan ×0.6 / Level 1-3 任务）。

---

## 上次任务（详细归档信息）

### TASK-20260505-01 DomBindings R2 收口（Level 3）— ✅ 已归档（commit `6f924fd`）

- **归档文档：** `memory-bank/archive/archive-TASK-20260505-01.md`（~340 行 / Level 3 详细归档 / 9 段 / 8 度量表）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260505-01.md`（~265 行 / Level 3 详细回顾 / 8 段 / 13 度量数据）
- **主交付：** 9 commits / +2820 行 / -28 行 / 净 +2792 行（含 spec + plan + reflection + archive + systemPatterns 4 新段 + techContext 同步）
- **跨决策协同度：** 3/3 决策 1 次 AskQuestion 锁定 / 第 9 次连续 100% 命中 / 累计 96/96 / sept → **nona-evidence** 升级
- **plan ×0.6 比值：** 0.14-0.18×（实测 ~35 min vs plan ×0.6 190-250 min）/ 落极速区 0.07-0.20× 第 4-5 数据点 / quad → **quint-evidence** 升级
- **反向探针：** 9/9 精准有效 / 强度梯度三档全谱（A.1 过高 4/4 UB 双重加固 / B.1 合适 2/7 文本路径 / C.1 平衡 3/3 alias 路径）
- **反复模式命中：** 0/7 已知模式 + 1 新候选 dual-evidence **入库定型为反复模式 #8（spec 数据回归 audit 协议）**
- **ctest：** DEVTOOL=ON 1298/1298 + DEVTOOL=OFF 1105/1105（+14/+14）/ dogfood smoke 14/14 PASS

**8 项 P0+P1+P2 改进建议落实情况：**

| # | 建议 | 优先级 | 落实位置 |
|:-:|---|:-:|---|
| 1 | systemPatterns「跨 Document arena 节点转移 — deep clone 必选范式」 | **P0** | ✅ reflect 阶段立即沉淀 |
| 2 | systemPatterns「Phase 0 投入 / 极速区 quint-evidence」 | **P0** | ✅ reflect 阶段立即沉淀 |
| 3 | systemPatterns「反复模式 #8 spec 数据回归 audit」 | **P0** | ✅ reflect 阶段立即沉淀 |
| 4 | systemPatterns「反向探针强度梯度三档解读」 | **P0** | ✅ reflect 阶段立即沉淀 |
| 5 | systemPatterns「视觉链路三件齐识别协议」 | **P1** | 📋 已迁移到「待处理事项」段 |
| 6 | `/plan` 命令固化「plan/spec docs 落盘即 commit」步骤 | **P1** | 📋 已迁移到「待处理事项」段 |
| 7 | writing-plans.mdc Phase 0「既有 getter/setter 语义对齐」子条 | **P2** | 📋 长期沉淀 |
| 8 | systemPatterns「跨决策协同度 100%」升级到 nona/dec-evidence | **P2** | 📋 长期沉淀 |

**P0 4/4 100% reflect 阶段直接落实 ✅** + **P1 2/2 已迁移待处理事项** + **P2 2/2 长期沉淀**（沿用 [TASK-20260504-01 P0+P1+P2×4 archive 全落实范式](memory-bank/archive/archive-TASK-20260504-01.md)）

---

## 下一推荐任务（基于 spec §11.2 + §6.2 推荐立项顺序）

> ✅ **TASK-20260505-01「DomBindings R2 三连补全」已闭环**（commit `6f924fd` / B-G1+G2+G3 全 ✅ / MVP-B 90% → 95%）— 推荐序号已下移。

| 优先 | 候选任务 | MVP 档 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|:-:|
| **1** | **Performance Overlay 持续 invalidate 机制（B-G4）— MVP-B 收口最后一项** | MVP-B 收口 | L1-3 | ~30 min-2 h |
| 2 | 资源加载策略蓝图（HTTP / file:// / data: URI 完整支持）| MVP-C 过渡 | L3 蓝图 + 实施 | ~5-10 h |
| 3 | R9 EventManager HitTest 改造（HUD pointer-events 真支持）| MVP-C | L2-3 | ~1.5-2 h |
| **4** | **G1 OpenGL ES 硬件渲染后端蓝图**（核心目标 #2 嵌入式硬件加速主线 P0 第一刚需）| MVP-C 核心 | **L4 多 Phase 蓝图** | ~30-60+ h |
| 5 | G2 DRM/KMS 嵌入式后端 | MVP-C 核心 | L3-4 | ~10-20 h |
| 6 | DomBindings 节点动态创建删除 | MVP-C | L3 | ~3-5 h |
| 7 | CSS 高级特性 5 项 | MVP-C | 5 × L2-3 | ~10-20 h |
| 8 | 图像扩展 3 项（GIF / WebP / 异步加载）| MVP-C | 3 × L2 | ~6-12 h |
| 9 | 性能优化收口（含 #35 阶段 2 / R3+ 13 项）| MVP-C | 多 L2-3 | ~10-30 h |

**用户决策点：** 下一任务建议从 #1（MVP-B 完整收口 / 估时小）或 #4（MVP-C 核心 P0 第一刚需 / 战略长期目标）启动 / 详见 `docs/specs/2026-05-04-mvp-scope.md` §11.2

---

## 待处理事项 — 跨任务沉淀（按优先级）

### 留下次工作流元任务批量落地（P1）

> 累计 P1 改进建议待批量沉淀到 `.cursor/rules/skills/*.mdc` — 等待下次工作流/规则类技术债清理任务（沿用 [TASK-20260503-02 工作流元任务范式](memory-bank/archive/archive-TASK-20260503-02.md)）。

- **P1 #1（来自 TASK-20260503-04 reflection §5）writing-plans.mdc Phase 0 段补强「JS context 归属与 host binding 注册 ctx 一致性 audit」子条** — 反复模式 #1 第 4 个新形式（panel JS / 用户脚本 ctx 归属未实证）；本任务 plan-fact reconcile #1（C2 wiring）即此模式实证。**预估**：~10 min。
- **P1 #2（来自 TASK-20260503-04 reflection §5）writing-plans.mdc「资源类反向探针 SOP」新子段** — 资源反向探针应限定到非注释区域 + comment policy 推荐；本任务 D.3 console_panel.html 注释里 `<input` 字面量触发反向探针 false positive。**预估**：~10 min。
- **P1 #2（来自 TASK-20260503-05 reflection §5）brainstorming.mdc 加新段「Phase 0 grep 实证驱动的主动 push-back 模式」** — D8b 实证（brainstorm scope 已被 core_only 限定后，Phase 0 grep 发现 creative 文档 10⁷ 检查点字面值会导致 100-1000s 死循环灾难）→ 必须**主动**抛出而非等用户问到；触发条件清单：(1) brainstorm scope 已被用户限定 + (2) Phase 0 grep / audit 阶段发现偏差 + (3) 偏差**显著**（默认值差 10³+ 倍）。**预估**：~10 min。
- **P1 #5（来自 TASK-20260505-01 reflection §5 #5）systemPatterns.md 新沉淀「视觉链路三件齐识别协议」段** — 当 dogfood UI 行为依赖 ≥3 个独立缺陷修复才能完整工作时，必须**单任务集中闭环**而非分多任务拆分；plan 阶段 §UI 行为验收表是识别工具（本任务 plan §0.11「视觉恢复链路」表是范式）。**预估**：~30-40 行 systemPatterns 段 / ~10 min。
- **P1 #6（来自 TASK-20260505-01 reflection §5 #6）`/plan` 命令固化「plan/spec docs 落盘即 commit」步骤** — 防止 build 阶段 collateral commit 补齐；落实位置 `.cursor/commands/plan` 或 `.cursor/rules/skills/writing-plans.mdc`。**预估**：~10 min。
- **P2 #4（来自 TASK-20260503-03 reflection §6）commit body Source 溯源 + 实测数据格式固化** — 累计 ~39 commits quad-evidence（远破 git-workflow.mdc 固化阈值 / 本任务延续协议）/ 建议下次工作流元任务批量落地时同步固化到 `.cursor/rules/skills/git-workflow.mdc`。**预估**：~10-15 min。

### 长期沉淀（P2 — 不强制 archive）

- **P2 #3（来自 TASK-20260503-02 reflection）GoogleTest `ASSERT_TRUE(x.ok()) << x.status().message()` 短路评估易错模式 P3** — A-P1#6 audit CP2 扩展发现 tests/ 中 8 处该模式；建议 codebase guideline「测试中也用三元守卫显式化」。**预估**：~30 min audit + ~1 h codebase 修正。
- **P2 #4（来自 TASK-20260503-02 reflection）plan §文档段落 LOC 预估系数 ×1.5-2× 修正** — 与 TASK-20260503-04 P2 #1（writing-plans LOC 估算附录加「隐性附加工作类型清单」+ ×1.3-1.5 buffer 范本）同源；建议合并落地。**预估**：~10 min。

---

## P3 候选清单（用户优先级排期）

### 来自 TASK-20260504-01 spec §11.2 + 各历史任务 — 与上方「下一推荐任务」表对应

详见上方「下一推荐任务」段 + `docs/specs/2026-05-04-mvp-scope.md` §11.2。

### R2 P3 候选 — 来自 TASK-20260502-01 dogfood 暴露（3 项 — ✅ TASK-20260505-01 已完整闭环）

| # | 缺陷 | 文件位置 | 闭环状态 |
|:-:|---|---|:-:|
| 1 | DomBindings 缺 `Element.children` 集合 getter（HTMLCollection 风格）| `veloxa/script/dom_bindings.cc` | ✅ TASK-20260505-01 commit `6c36dc7` |
| 2 | DomBindings 缺 `element.addEventListener` 事件别名 | `veloxa/script/dom_bindings.cc` | ✅ TASK-20260505-01 commit `fb88288`（audit + 4 alias）|
| 3 | DomBindings 缺 `element.innerHTML` setter | `veloxa/script/dom_bindings.cc` | ✅ TASK-20260505-01 commit `986e978`（deep-clone）|

**闭环成果：** 单 Level 3 任务一次性闭环 ✅ / 14 单测全 PASS / dogfood smoke 14/14 PASS / inspector tab 切换 + HUD 数字 + DOM tree 渲染**视觉完整工作**。

### 8 项 P3 触发型候选（codebase review R1 已分析）

- TASK-26-02-full（clearance 完整版）
- TASK-26-03（LayoutInline IFC 递归 + bidi）
- TASK-20260424-02（Layout 残余 super-linear ~40%）
- CSS 4 标准逻辑属性 shorthand（`border-block` / `border-inline`）
- `border-image` / `border-radius` 简写
- TASK-20260419-06（HashMap Hash Mixing）
- TASK-20260419-08（`string.h` 剩余 memcpy noinline 化）
- TASK-20260419-12（DrawText 真路径优化，K7 隐式闭环待评估）

---

## 收尾清理（可选）

- ✅ `feature/TASK-20260505-01-dombindings-r2-closure` 分支已合并 + 删除（archive 阶段完成）
- ✅ `feature/TASK-20260504-01-mvp-scope-doc` 分支已合并 + 删除
- 早期 feature 分支（TASK-20260430-* / TASK-20260502-* / TASK-20260503-*）如未删除可批量清理

---

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260505-01.md`（**DomBindings R2 收口 — B-G1 children + B-G3 innerHTML setter + B-G2 audit Level 3，2026-05-05**）— **本批最新 / MVP-B 完成度 90% → 95% / dogfood 视觉自动恢复链路三件齐 ✅ / 协议三件套里程碑（Phase 0 极速区 quint-evidence + 跨决策协同度 nona-evidence + 反向探针强度梯度三档 dual-evidence）/ 反复模式 #8 入库定型 / P0×4 reflect 全落实**
- `archive-TASK-20260504-01.md`（MVP-scope 文档蓝图 Level 4 V2=a 完整变体，2026-05-04）— DevTool 4 件套主线收官标识 🎉 / 三档分级 MVP-A/B/C 体系建立 / 路线图按 MVP 档分层重写 / 核心目标 #1+#2 路径量化 / P0+P1+P2×4 archive 全落实 / 跨决策协同度 100% 第 8 次连续命中 / plan ×0.6 矩阵第 4 蓝图数据点 0.25-0.38× 入库
- `archive-TASK-20260503-04.md`（DevTool Phase D · Console JS REPL Level 3 [安全相关]，2026-05-04）— DevTool 4 件套全部完整闭环 ✅ / spec §11.1 完整闭环 ✅ / T1 5 维度首次完整暴露 ✅ / plan ×0.6 0.07-0.10× 创历史新低 / Phase 0 sept-evidence / P0×3 archive 阶段全落实
- `archive-TASK-20260503-05.md`（QuickJS Interrupt Handler + SetEvalInterruptBudget API Level 2 [安全相关]，2026-05-03）
- `archive-TASK-20260503-03.md`（DevTool 三件套主线收官 — 4 项 P3 候选批量清零 Level 2，2026-05-03）
- `archive-TASK-20260503-02.md`（工作流/规则类技术债批量清理 Level 2，2026-05-03）
- `archive-TASK-20260503-01.md`（DevTool Phase C · Hot Reload Level 3，2026-05-03）
- `archive-TASK-20260502-02.md`（DevTool Phase B · Performance Overlay Level 3，2026-05-03）
- `archive-TASK-20260502-01.md`（DevTool Phase A · Inspector 实施 Level 4，2026-05-02）
- `archive-TASK-20260430-04.md`（DevTool 三件套蓝图设计 Level 4 V2=a，2026-05-01）
- `archive-TASK-20260430-03.md`（全代码库 Code Review Level 4，2026-05-01）
- `archive-TASK-20260430-02.md`（CSS border shorthand 补全 Level 2，2026-04-30）
- `archive-TASK-20260430-01.md`（first/last child margin collapse with parent Level 3，2026-04-30）
- `archive-TASK-20260426-01.md`（Layout 正确性消化 Level 4，2026-04-30）
- `archive-TASK-20260425-01.md`（SDL2 窗口后端 + 输入事件桥接 Level 3，2026-04-26）
- `archive-TASK-20260424-04.md`（DrawText warm 残余优化 Level 2 D 纯收尾，2026-04-25）
- `archive-TASK-20260424-03.md`（DrawText warm 优化 Level 2-3 K7 Resolved，2026-04-24）
- `archive-TASK-20260424-01.md`（Layout super-linear knee 根因调查，2026-04-24）
- `archive-TASK-20260419-13.md`（流程规则 P0/P1 沉淀冲刺，2026-04-19）
- `archive-TASK-20260419-11.md`（ImageCache::Load HashMap 化，2026-04-19）
- 更早归档见 `memory-bank/archive/` 目录与 `tasks.md §任务历史`
