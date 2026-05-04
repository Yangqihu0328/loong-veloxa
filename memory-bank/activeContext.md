# 活跃上下文

## 当前阶段

**空闲** — TASK-20260504-01 已归档闭环 ✅。等待用户启动新任务（`/van`）或恢复 P3 候选清单中的下游任务。

**最近闭环：** TASK-20260504-01 MVP-scope 文档（Level 4 蓝图 V2=a 完整变体）— 三档分级 MVP-A/B/C 体系建立 ✅ + 路线图按 MVP 档分层重写 ✅ + DevTool Phase E/F/G「超 MVP plus」标识范式确立 ✅ + 核心目标 #1+#2 路径量化（C-G1 OpenGL ES ~30-60+ h + C-G2 DRM/KMS ~10-20 h）✅ + P0×1 reflect 阶段立即沉淀 + P1+P2×4 archive 阶段全部落实 ✅（详见下方「上次任务」段）。

**DevTool 4 件套主线收官 🎉** — Phase A Inspector + Phase B Performance Overlay + Phase C Hot Reload + Phase D Console JS REPL 已全部完整闭环 / Phase E/F/G 标识为「超 MVP plus」由用户需求驱动决定立项时机。

---

## 上次任务（已归档闭环）

### TASK-20260504-01 MVP-scope 文档（Level 4 蓝图 V2=a 完整变体）— ✅ 已归档（commit `4e35cea`）

- **归档文档：** `memory-bank/archive/archive-TASK-20260504-01.md`（~440 行 / Level 4 全面归档）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260504-01.md`（~290 行 / Level 4 全面回顾）
- **主交付：** spec + plan + creative ×3 + 4 处文档全量同步 / 7 commits / +2798 行 / -46 行 / 净 +2752 行
- **跨决策协同度：** 11/11 1 次 AskQuestion all_recommended 锁定 / 第 8 次连续 100% 命中 / 累计 93/93
- **plan ×0.6 比值：** 0.25-0.38× standalone-AI 实测 / 落「蓝图任务 V2=a」子档下沿延伸第 4 数据点
- **反复模式命中：** 0/7 已知模式 + 1 新候选定型（中文文档 StrReplace 全角/半角字符差异 — 已 reflect 阶段立即沉淀）

**5 项 P0+P1+P2 改进建议落实情况：**

| # | 建议 | 优先级 | 落实位置 |
|:-:|---|:-:|---|
| 1 | systemPatterns「中文文档编辑安全 audit」+ writing-plans.mdc 同源 audit 子条 | **P0** | ✅ reflect 阶段立即沉淀 |
| 2 | systemPatterns「蓝图任务规模估算公式」段（实证 6 数据点）| **P1** | ✅ archive 阶段直接落实 |
| 3 | plan ×0.6 矩阵第 4 数据点入库（蓝图任务 V2=a 子档）| **P2** | ✅ archive 阶段直接落实 |
| 4 | systemPatterns「超 MVP plus 标识范式」段 | **P2** | ✅ archive 阶段直接落实 |
| 5 | systemPatterns「跨决策协同度 100% sept-evidence」段（8 次实证）| **P2** | ✅ archive 阶段直接落实 |

**5/5 100% archive 阶段直接落实** ✅（沿用 [TASK-20260503-04 archive 范式](memory-bank/archive/archive-TASK-20260503-04.md) — 不留待下次工作流元任务）

---

## 下一推荐任务（基于 spec §11.2 + §6.2 推荐立项顺序）

| 优先 | 候选任务 | MVP 档 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|:-:|
| **1** | **DomBindings R2 三连补全**（`Element.children` + `addEventListener` + `innerHTML` setter）| MVP-B 收口 | L3 | ~2-4 h |
| 2 | Performance Overlay 持续 invalidate 机制 | MVP-B 收口 | L1-3 | ~30 min-2 h |
| 3 | 资源加载策略蓝图（HTTP / file:// / data: URI 完整支持）| MVP-C 过渡 | L3 蓝图 + 实施 | ~5-10 h |
| 4 | R9 EventManager HitTest 改造（HUD pointer-events 真支持）| MVP-C | L2-3 | ~1.5-2 h |
| **5** | **G1 OpenGL ES 硬件渲染后端蓝图**（核心目标 #2 嵌入式硬件加速主线 P0 第一刚需）| MVP-C 核心 | **L4 多 Phase 蓝图** | ~30-60+ h |
| 6 | G2 DRM/KMS 嵌入式后端 | MVP-C 核心 | L3-4 | ~10-20 h |
| 7 | DomBindings 节点动态创建删除 | MVP-C | L3 | ~3-5 h |
| 8 | CSS 高级特性 5 项 | MVP-C | 5 × L2-3 | ~10-20 h |
| 9 | 图像扩展 3 项（GIF / WebP / 异步加载）| MVP-C | 3 × L2 | ~6-12 h |
| 10 | 性能优化收口（含 #35 阶段 2 / R3+ 13 项）| MVP-C | 多 L2-3 | ~10-30 h |

**用户决策点：** 下一任务建议从 #1（MVP-B 收口）或 #5（MVP-C 核心 P0 第一刚需）启动 / 详见 `docs/specs/2026-05-04-mvp-scope.md` §11.2

---

## 待处理事项 — 跨任务沉淀（按优先级）

### 留下次工作流元任务批量落地（P1）

> 累计 P1 改进建议待批量沉淀到 `.cursor/rules/skills/*.mdc` — 等待下次工作流/规则类技术债清理任务（沿用 [TASK-20260503-02 工作流元任务范式](memory-bank/archive/archive-TASK-20260503-02.md)）。

- **P1 #1（来自 TASK-20260503-04 reflection §5）writing-plans.mdc Phase 0 段补强「JS context 归属与 host binding 注册 ctx 一致性 audit」子条** — 反复模式 #1 第 4 个新形式（panel JS / 用户脚本 ctx 归属未实证）；本任务 plan-fact reconcile #1（C2 wiring）即此模式实证。**预估**：~10 min。
- **P1 #2（来自 TASK-20260503-04 reflection §5）writing-plans.mdc「资源类反向探针 SOP」新子段** — 资源反向探针应限定到非注释区域 + comment policy 推荐；本任务 D.3 console_panel.html 注释里 `<input` 字面量触发反向探针 false positive。**预估**：~10 min。
- **P1 #2（来自 TASK-20260503-05 reflection §5）brainstorming.mdc 加新段「Phase 0 grep 实证驱动的主动 push-back 模式」** — D8b 实证（brainstorm scope 已被 core_only 限定后，Phase 0 grep 发现 creative 文档 10⁷ 检查点字面值会导致 100-1000s 死循环灾难）→ 必须**主动**抛出而非等用户问到；触发条件清单：(1) brainstorm scope 已被用户限定 + (2) Phase 0 grep / audit 阶段发现偏差 + (3) 偏差**显著**（默认值差 10³+ 倍）。**预估**：~10 min。
- **P2 #4（来自 TASK-20260503-03 reflection §6）commit body Source 溯源 + 实测数据格式固化** — 累计 ~34-35 commits quad-evidence（远破 git-workflow.mdc 固化阈值）/ 建议下次工作流元任务批量落地时同步固化到 `.cursor/rules/skills/git-workflow.mdc`。**预估**：~10-15 min。

### 长期沉淀（P2 — 不强制 archive）

- **P2 #3（来自 TASK-20260503-02 reflection）GoogleTest `ASSERT_TRUE(x.ok()) << x.status().message()` 短路评估易错模式 P3** — A-P1#6 audit CP2 扩展发现 tests/ 中 8 处该模式；建议 codebase guideline「测试中也用三元守卫显式化」。**预估**：~30 min audit + ~1 h codebase 修正。
- **P2 #4（来自 TASK-20260503-02 reflection）plan §文档段落 LOC 预估系数 ×1.5-2× 修正** — 与 TASK-20260503-04 P2 #1（writing-plans LOC 估算附录加「隐性附加工作类型清单」+ ×1.3-1.5 buffer 范本）同源；建议合并落地。**预估**：~10 min。

---

## P3 候选清单（用户优先级排期）

### 来自 TASK-20260504-01 spec §11.2 + 各历史任务 — 与上方「下一推荐任务」表对应

详见上方「下一推荐任务」段 + `docs/specs/2026-05-04-mvp-scope.md` §11.2。

### R2 P3 候选 — 来自 TASK-20260502-01 dogfood 暴露（3 项 — DomBindings R2 三连补全已是「下一推荐任务」#1）

| # | 缺陷 | 文件位置 | 当前 panel 临时 mitigation | 优先级 |
|:-:|---|---|---|:-:|
| 1 | DomBindings 缺 `Element.children` 集合 getter（HTMLCollection 风格）| `veloxa/script/dom_bindings.cc` | inspector_panel.js inline `if (!tabs.children) return` 防御 | P3 |
| 2 | DomBindings 缺 `element.addEventListener` | `veloxa/script/dom_bindings.cc` | inspector_panel.js inline `if (typeof btn.addEventListener !== "function") return` 防御 | P3 |
| 3 | DomBindings 缺 `element.innerHTML` setter | `veloxa/script/dom_bindings.cc` | renderDomTree silent no-op；vx_devtool_get_dom_json JSON 已覆盖核心数据契约 | P3 |

**建议立项形态：** 单一 Level 3 任务 `TASK-2026MMDD-NN: DomBindings R2 三连补全`（plan ×0.6 ~3-5 h），TDD 三连实施 + 单测覆盖 + DevTool dogfood 视觉自动恢复验证。

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

- ✅ `feature/TASK-20260504-01-mvp-scope-doc` 分支已合并 + 删除（archive 阶段完成）
- 早期 feature 分支（TASK-20260430-* / TASK-20260502-* / TASK-20260503-*）如未删除可批量清理

---

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260504-01.md`（**MVP-scope 文档蓝图 Level 4 V2=a 完整变体，2026-05-04**）— **本批最新 / DevTool 4 件套主线收官标识 🎉 / 三档分级 MVP-A/B/C 体系建立 / 路线图按 MVP 档分层重写 / 核心目标 #1+#2 路径量化 / P0+P1+P2×4 archive 全落实 / 跨决策协同度 100% 第 8 次连续命中 / plan ×0.6 矩阵第 4 蓝图数据点 0.25-0.38× 入库**
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
