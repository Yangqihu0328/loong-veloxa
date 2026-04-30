# 归档：规划 UI 编辑器 + 调试器（DevTool 三件套蓝图设计）

**日期：** 2026-04-30 → 2026-05-01
**任务 ID：** TASK-20260430-04
**复杂度级别：** Level 4（多子系统蓝图 + 8 决策矩阵 + 8 威胁面 + V2=a 纯蓝图主交付 + [安全相关]）
**安全相关：** ✅ 是
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260430-04-ui-editor-debugger`（基于 main `2445990`，归档时**已 `--no-ff` 合入 main**）

---

## 1. 任务概述

Veloxa 引擎首次 **Level 4 蓝图主交付任务**（V2=a）— spec / plan / creative ×N 文档产出即任务终态，build 由用户后续基于 plan 独立立项。范围覆盖：

| 维度 | V1 选择 | V2 选择 | V3 选择 | V4 选择 | V5 安全 |
|---|---|---|---|---|---|
| 子系统范围 | **B 三件套**：Inspector + Hot Reload + Performance Overlay | — | — | — | — |
| 输出形态 | — | **a 纯蓝图**（spec + plan + creative ×N，不强制实施代码）| — | — | — |
| UI 渲染层 | — | — | **A Veloxa 自渲染**（dogfood 模式）| — | — |
| 复杂度 | — | — | — | **Level 4** | — |
| 安全标注 | — | — | — | — | ✅ 是（含 T1-T8 威胁建模）|

**意图判读**：用户用「**规划**」二字（非「实现」）→ 主交付物是**蓝图级 spec + plan + creative**，build 由用户基于产出 plan 拆出独立子任务。VAN 阶段 push back 引入「6 子系统全做」陷阱收敛 + 「规划」语义明确化 + V3 自渲染锁定，封顶任务范围避免 Level 4 失控。

---

## 2. 技术方案

### 2.1 工作流路径（V2=a 蓝图任务变体首次实践）

```
/van → /plan → /reflect → /archive
        ↑ 跳过 /build
        ↑ /creative 内联到 /plan
```

| 阶段 | plan 估时 | 实测 | × plan | × ×0.6 |
|---|---|---|---|---|
| **VAN** | 30 min / 18 min ×0.6 | ~25 min | 0.83× | 1.39× |
| **/plan**（含 brainstorming + 4 篇文档批量产出）| 180-240 min / 108-144 min ×0.6 | ~49 min | **0.20-0.27×** ⚡ | **0.34-0.45×** |
| **/reflect** | 60 min / 36 min ×0.6 | ~40 min | 0.67× | 1.11× |
| **/archive** | 45 min / 27 min ×0.6 | ~50 min（含 P1/P2 改进 + merge）| 1.11× | 1.85× |
| **主线合计** | 315-345 / 189-225 ×0.6 | **~164 min** | **0.48-0.52×** | **0.73-0.87×** |

**plan ×0.6 第 17 数据点入库**：核心轮次（VAN + Plan）~74 min 落 **0.27-0.35× plan / 0.46-0.59× plan ×0.6** —「极窄档 + review 类下限交界」。详见 reflection §1.2 + systemPatterns 「批量决策 + 批量文档」极窄档段。

### 2.2 D1-D8 决策矩阵（详见 spec §4）

| # | 维度 | **锁定值** | 关联文档段 |
|:-:|---|---|---|
| **D1** | 三件套实施优先级 | **B** Inspector → Overlay → Hot Reload | spec §6 Phase 划分 + plan Phase A/B/C |
| **D2** | Inspector 数据采集协议 | **B** 半结构化（JSON tree + DisplayList overlay + C API JSON）| spec §5.3 数据流 + plan A.0.3-A.0.6 |
| **D3** | DevTool UI 主屏布局 | **B** 同窗口 splitter dock + Overlay HUD 子模式 | creative #1 §决策 1-5 |
| **D4** | DevTool 隔离边界 | **B** 单进程共享容器（双 Document + 共享 EventLoop / Application / ImageCache）| spec §5.1 模块边界 + plan A.0.1（I1 改造）|
| **D5** | Hot Reload file watcher + 增量策略 | **A** 嵌入式专注（Linux inotify + CSS-only 增量重载）| creative #2 §决策 1-5 + plan Phase C |
| **D6** | Performance Overlay 数据采集点 | **B** Chrome DevTools 风格（五钩子 + 滑动 60 帧 + dirty rect 边框高亮）| spec §5.1 perf_overlay + plan Phase B |
| **D7** | C API 扩展边界 | **C** 双层 API（内部 C++ 核心 + 公开 C API 薄封装）| spec §5.1 vx_devtool / vx_api + plan A.0.5/A.0.6 |
| **D8** | 安全威胁建模 | **A** T2/T3/T5/T6/T7/T8 完整 + T1/T4 扩展段占位 | spec §7 + plan Phase A.2 / B.3 / C.5 |

### 2.3 注入点核对表 I1-I8（grep 实证就位）

| # | 注入点 | 当前位置 | 可行性 | 改造 |
|:-:|---|---|:-:|---|
| **I1** | Application 双 Document 槽 | `application.cc::document_` 单字段 | 🟡 需扩展 | 重命名 → `target_document_` + 加 `devtool_document_`（callsite 漏改编译期暴露 — R1 mitigation）|
| **I2** | UpdateManager 五钩子 | `update_manager.h` 无 hook | 🔴 需新建 | OnFrameStart / OnLayout / OnPaint / OnPresent / OnFrameEnd（function pointer + nullptr branch predictor，闭环 #35）|
| **I3** | DisplayList overlay 注入 | `display_list.h::PaintCommand` | 🟡 需扩展 | +kOverlayHighlight 命令 + 双线宽渲染（闭环 D3 创意决策 4）|
| **I4** | LayoutBox.Dump | `layout_box.h` 无方法 | 🔴 需新建 | 闭环 #26 |
| **I5** | C API introspection | `veloxa_api.h` 仅运行时接口 | 🟡 需扩展 | `vx_view_serialize_dom` / `vx_node_get_computed_style` / `vx_node_get_layout_box`（双层 API D7=C，闭环 #40）|
| **I6** | EventManager hover/active/focus | `event_manager.h` 已暴露 | 🟢 已就绪 | Inspector hover-to-select 直接复用 |
| **I7** | ImageCache namespace | `image_cache.h` 全局 key | 🟡 需扩展 | `devtool:` prefix 隔离（闭环 #4）|
| **I8** | FileWatcher 抽象 | 无 | 🔴 需新建 | `platform/file_watcher_inotify.h`（Linux 优先 + 跨平台 abstract base）|

### 2.4 安全威胁建模 T1-T8（详见 spec §7）

| # | 威胁 | 严重度 | mitigation 摘要 | 落地阶段 |
|:-:|---|:-:|---|---|
| **T1** | Console JS REPL 任意 eval | 🔴 高 | 扩展段占位 — sandbox isolate + namespace whitelist + reflect 阶段全审计 | 扩展段（不一期）|
| **T2** | Hot Reload 路径穿越 | 🔴 高 | **8 步守卫**：绝对路径 / canonicalize / boundary / extension filter / max_size / debounce / 归一化 / 拒绝相对（详见 creative #2 §决策 4）| Phase C.5 |
| **T3** | Inspector 敏感数据回显 | 🟡 中 | `<input type="password">` 默认 redact + `vx_inspector_set_redaction_policy` API + JSON 输出标 `[Local Only]` metadata | Phase A.2 |
| **T4** | CDP 远程调试 port 暴露 | 🔴 高 | 扩展段占位 — HMAC token + nonce + loopback only + default off | 扩展段（不一期）|
| **T5** | DevTool 自渲染容器越权 | 🟡 中 | 单 Application 实例验证 + mutation API 编译期禁止 | Phase A.0 |
| **T6** | Callback 任意代码执行 | 🟡 中 | 1 ms/frame 执行预算 + QuickJS Interrupt Handler（与 #44 同源）| Phase B.3 |
| **T7** | 公开 C API Buffer overflow | 🟡 中 | 双调用模式（先查 size 再 alloc）+ 内部默认 max_size（DOM 16 MiB / Style 1 MiB）| Phase A.0.5 |
| **T8** | Mutation propagation（双 Document）| 🟡 中 | 严格独立 stylesheet / DOM tree（无共享 shared_ptr）+ ImageCache namespace 隔离 | Phase A.0.1 |

---

## 3. 实现摘要

### 3.1 文件变更清单

| 操作 | 文件 | 说明 |
|---|---|---|
| 创建 | `docs/specs/2026-04-30-devtool-design.md` | spec 主文档 12 段 / D1-D8 / I1-I8 / T1-T8 / R1-R6 / 30+ systemPatterns 自我对照 |
| 创建 | `docs/plans/2026-04-30-devtool.md` | plan 主文档 / Phase 0/A/B/C/D / CMake + 静态库 + 测试基础设施审计 / 子任务 ~40 项 / plan ×0.6 估时矩阵 |
| 创建 | `memory-bank/creative/creative-devtool-screen-layout.md` | DevTool UI 主屏布局 5 决策 / splitter dock + HUD overlay 双层结构 |
| 创建 | `memory-bank/creative/creative-devtool-hot-reload.md` | Hot Reload 5 决策 / FileWatcher 跨平台抽象 / InotifyFileWatcher T2 8 步守卫 / DOM 状态保留协议 |
| 创建 | `memory-bank/reflection/reflection-TASK-20260430-04.md` | 10 节全面回顾 / Level 4 含架构评估 + 长期影响 / 10 项改进建议 P0/P1/P2 |
| 创建 | `memory-bank/archive/archive-TASK-20260430-04.md` | 本文档 |
| 修改 | `.cursor/rules/main.mdc` | P0 #1：加 § Level 4 蓝图任务 V2=a 工作流变体段 |
| 修改 | `.cursor/rules/skills/brainstorming.mdc` | P1 #2 + P2 #9：加「与已锁定决策的协同度」段 + 决策跳过率监控段 |
| 修改 | `memory-bank/systemPatterns.md` | P0 #3 + P1 #4 + P2 #5：plan ×0.6 矩阵第 17 数据点 + Level 4 蓝图任务 V2=a 工作流变体段 + dogfood 路径段 |
| 修改 | `memory-bank/techContext.md` | P1 #6：加 TASK-30-04 蓝图主交付摘要段 |
| 修改 | `docs/reports/2026-04-30-codebase-review.md` | P1 #8：F-025 段加强依赖交叉记录（Hot Reload C.2 ↔ TASK-30-04-C HTML hot reload 扩展段）|
| 修改 | `memory-bank/{tasks,activeContext,progress}.md` | 三阶段 MB 同步（VAN / plan / reflect / archive）|

**总计**：6 创建（4 蓝图文档 + reflection + archive）+ 6 修改（4 规则/知识库 + codebase-review.md + 3 MB）= 12 文件。

### 3.2 关键决策（设计决策）

8 项 D1-D8 决策见 §2.2 表，均按 VAN 推荐默认锁定（用户连续 8 次跳过 AskQuestion 隐式批准）。

### 3.3 关键决策（工作流决策）

| # | 决策 | 理由 |
|---|---|---|
| W1 | V2=a 锁定 — 蓝图任务跳过 build 直进 reflect | 与「规划」语义最匹配；build 由用户后续基于 plan 拆出独立子任务 |
| W2 | brainstorming 13 决策（V1-V5 + D1-D8）允许批量跳过 | 触发「Checkpoint 推荐默认 + 隐式批准协议」+ VAN 推荐基于 grep 实证保证质量 |
| W3 | 4 篇文档批量产出（1 个 session 内连续完成）| 避免上下文切换成本，触发「批量文档产出 batch 协议」 |
| W4 | spec §10 强制 ≥ 30 systemPatterns 自我对照 | 蓝图任务架构耦合度审计标准动作，识别跨任务依赖与潜在缺陷 |
| W5 | reflect 阶段「广义构建 = 文档产出」灵活解读 | 蓝图任务无 build → reflect 命令前置条件「构建中」语义 = 「文档产出中」 |

### 3.4 安全决策

✅ [安全相关] 任务，含 T1-T8 完整威胁建模 + 8 步路径穿越守卫 + 双调用 API 协议 + redaction policy + namespace 隔离。

| 安全决策 | 实施 |
|---|---|
| 路径穿越 8 步守卫（绝对路径 / canonicalize / boundary / extension / max_size / debounce / 归一化 / 拒绝相对）| ✅ creative #2 §决策 4 详细规范 |
| Inspector 敏感数据 redaction policy（`<input type="password">` 默认 redact + JSON `[Local Only]` 标记）| ✅ spec §7 T3 |
| 双调用 C API（先查 size 再 alloc）+ 默认 max_size（DOM 16 MiB / Style 1 MiB）| ✅ spec §7 T7 |
| 双 Document 严格独立 + ImageCache namespace 隔离（`devtool:` prefix）| ✅ spec §7 T8 + plan A.0.1 |
| Callback 1 ms/frame 执行预算（与 #44 同源）| ✅ spec §7 T6 |
| Console JS REPL / CDP 远程 port — **不一期**（占位段）| ✅ spec §11 扩展段，威胁面提前识别 |

**威胁面控制**：本任务一期不引新依赖 + 不开远程 port + 不允许 JS REPL → 攻击面控制在「**本机 DOM 状态读取 + 本地文件监听**」最小集。8/8 安全维度通过 reflection §6 评估。

---

## 4. 测试覆盖

V2=a 蓝图任务**无代码改动 / 无新单测**。ctest 基线保持 main `2445990` 终态 1062/1062 PASS（隐式继承）。

测试覆盖蓝图（详见 plan §3 测试策略，由 TASK-30-04-A/B/C build 阶段实施）：

| 阶段 | 测试范围 | 估时 |
|---|---|---|
| **TASK-30-04-A** Inspector | DOM 序列化 / Style query / Layout box dump / hover 高亮注入 | ~3 h plan |
| **TASK-30-04-B** Performance Overlay | 五钩子触发 / 60 帧滑动平均 / dirty rect 渲染 | ~1.5 h plan |
| **TASK-30-04-C** Hot Reload | inotify 事件 / 8 步路径守卫 / CSS 增量重载 / DOM 状态保留 | ~3 h plan |

---

## 5. 经验教训（来自 reflection §4，详见 reflection 文档）

1. **蓝图任务（V2=a）的工作流变体值得形式化** — Level 4 任务有两种主交付形态（代码实施 vs 蓝图主交付），后者实测 plan ×0.6 系数显著低于前者（0.20-0.35× vs 0.85-1.00×）— 工作流路径不同，估时模型也不同
2. **「Checkpoint 推荐默认 + 隐式批准协议」叠加多决策时威力惊人** — 跨 13 决策（V1-V5 + D1-D8）连续触发该协议，brainstorming 时间从典型 30-60 min 压缩到 ~5-8 min（压缩比 ~6-12×）— 关键前提是 VAN/brainstorm 推荐基于 grep 实证保证质量
3. **批量文档产出（≥ 3 篇）的 batch 效益** — 单次 session 内连续产出 4 篇文档，每篇 ~10-12 min（不含 brainstorming），分 session 写每篇需重读上一篇保一致性，单 session batch 提速近 30-50%
4. **「Checkpoint 推荐默认协议」对 V1-V5 / D1-D8 的应用边界** — VAN 决策（V1-V5）= 任务范围决策跳过相对安全；Plan 决策（D1-D8）= 架构决策跳过潜在风险更高（错误会传导到 build 阶段返工）；reflect 阶段必须重新审视 D1-D8 合理性
5. **dogfood 路径（V3=A Veloxa 自渲染）的双向价值** — 不仅是少引依赖（功能性），更是反向暴露引擎缺陷（探测性 acceptance test）— DevTool 实施时暴露的缺陷自然变成 R3+ 候选输入源，是 Veloxa 「自我应用样板」战略的最佳载体

---

## 6. 改进建议落实状态

reflection §5 列 10 项（P0×3 / P1×4 / P2×3）：

| # | 优先级 | 建议 | 落实状态 |
|---|---|---|---|
| 1 | **P0** | `main.mdc` 加 § Level 4 蓝图任务 V2=a 工作流变体段 | ✅ 已落实 |
| 3 | **P0** | `systemPatterns.md` plan ×0.6 矩阵加蓝图极窄档（第 17 数据点）| ✅ 已落实 |
| 7 | **P0** | `activeContext.md` 加 TASK-30-04 后续 7 项独立立项候选 | ✅ 已落实 |
| 2 | P1 | `brainstorming.mdc` 加「与已锁定决策的协同度」段 | ✅ 已落实 |
| 4 | P1 | `systemPatterns.md` 加 Level 4 蓝图任务 V2=a 工作流变体段 | ✅ 已落实 |
| 6 | P1 | `techContext.md` 加 TASK-30-04 蓝图主交付摘要段 | ✅ 已落实 |
| 8 | P1 | R3+ 强依赖（Hot Reload C.2 ↔ R3+ #3 F-025）显式交叉记录 | ✅ 已落实 |
| 5 | P2 | `systemPatterns.md` 加 dogfood 路径 = 探测性 acceptance test 段 | ✅ 已落实 |
| 9 | P2 | `brainstorming.mdc` 加决策跳过率监控段 | ✅ 已落实 |
| 10 | P2 | 蓝图任务子任务清单估时颗粒度回填校准 | ⏳ 待 TASK-30-04-A/B/C 立项后实测回填 |

**P0 全部落实 + P1 全部落实 + P2 2/3 落实**，剩 1 项 P2（#10）依赖后续任务实测数据回填。改进建议闭环率 90%（与 TASK-30-03 持平）。

---

## 7. 用户后续独立立项候选（7 项，按优先级排序）

**主线 3 项（基于 plan 拆出）：**

| 优先级 | Task ID | 内容 | Findings | 估时 | Level | 推荐 |
|---|---|---|---|---|---|---|
| **#1** ⭐ | TASK-30-04-A | DevTool Phase A — Inspector 实施 | I1 + I3 + I4 + I5 + I6 + I7 + 闭环 #26 #40 #4 | ~12.25 h plan / ~7.35 h plan ×0.6 | Level 3 | ⭐ Top — 必须先于 B |
| **#2** ⭐ | TASK-30-04-B | DevTool Phase B — Performance Overlay 实施 | I2 + 闭环 #35 | ~7.25 h plan / ~4.35 h plan ×0.6 | Level 2-3 | ⭐ Top — 依赖 A 的渲染管线 |
| **#3** ⭐ | TASK-30-04-C | DevTool Phase C — Hot Reload 实施（Linux only）| I8 + T2 8 步守卫 | ~10 h plan / ~6 h plan ×0.6 | Level 3 | ⭐ Top — 与 A 并行可立 |

**扩展段 4 项**（spec §11 占位，按用户优先级排期）：

| Task ID | 内容 | 估时 | Level | 备注 |
|---|---|---|---|---|
| TASK-30-04-D | Console JS REPL（威胁 T1 任意 eval mitigation）| ~6-8 h | Level 3 | sandbox isolate + namespace whitelist |
| TASK-30-04-E | JS Debugger backend（QuickJS Debug API 对接）| ~10-15 h | Level 3-4 | 触及技术债 #44 + 威胁 T6 callback budget |
| TASK-30-04-F | CDP 远程调试 port（威胁 T4）| ~8-12 h | Level 3 | HMAC token + nonce + loopback only + default off |
| TASK-30-04-G | 完整 UI 编辑器（dogfood 完整闭环）| ~3-4 周 | Level 4 大件 | 长期愿景 |

**强依赖关系（立项前置守卫）：**

- TASK-30-04-A 必须先于 TASK-30-04-B 立项（B 依赖 A 的 Inspector 渲染管线 + UI 框架基础）
- TASK-30-04-A 第一步 = I1 Application 双 Document 槽改造（R1 风险 mitigation）
- TASK-30-04-C Hot Reload Phase C.2 增量重载严格 = CSS-only（不触发 LoadHTML）→ **不踩 R3+ #3 F-025 use-after-free**
- 如未来扩展 HTML 增量重载 → 必须先做 R3+ #3 修复（强依赖，已在 spec §12.3 + codebase-review F-025 段交叉记录）
- TASK-30-04-C 可与 A 并行（无强依赖）；扩展段 4 项独立可立（无相互强依赖）

---

## 8. 触及技术债 4 项闭环 ROI 路径

| # | 技术债 | DevTool 子系统 | 闭环节奏 |
|:-:|---|---|---|
| #26 | LayoutBox.Dump 调试方法缺失 | Inspector Layout 面板 | TASK-30-04-A Phase A.0.2 实施 |
| #35 | UpdateManager 未暴露 frame hook | Performance Overlay 五钩子 | TASK-30-04-B Phase B.0.1 实施 |
| #40 | C API 缺 DOM/Style/Layout introspection | Inspector 全子系统 | TASK-30-04-A Phase A.0.5/A.0.6 实施 |
| #4 | ImageCache 命名空间隔离 | DevTool icon 防污染 | TASK-30-04-A Phase A.0.1 配套 |

**ROI 路径示范**：Veloxa 项目首次「**历史技术债通过新功能开发自然闭环**」— 不是单独立项偿还技术债（成本高，价值不直接），而是在做新功能时顺路偿还（成本边际化，价值复合）。

---

## 9. plan ×0.6 第 17 数据点入库

| 数据点 | 任务 | × plan | × ×0.6 | 任务类型 | 备注 |
|---|---|---|---|---|---|
| 1-15 | 各历史任务 | — | — | 多类型 | 详见 systemPatterns plan ×0.6 矩阵 |
| 16 | TASK-30-03（codebase review）| 0.51-0.60× | 0.85-1.00× | review + R2 quick fix 实施 | 中位档 |
| **17** | **TASK-30-04（DevTool 蓝图）** | **0.27-0.35×** | **0.46-0.59×** | **蓝图任务 V2=a + 多决策跳过 + 批量文档** | **极窄档 + review 类下限交界** ⚡ |

**新群组化「极窄档」3 数据点：**
- TASK-30-04（DevTool 蓝图）：0.20-0.27× plan
- TASK-30-02（CSS border shorthand）：0.22× plan
- TASK-30-01 P6（first/last child margin collapse）：0.21× plan

**触发条件**（三因素叠加）：
1. 批量决策跳过（≥ 5 决策按 VAN 推荐默认锁定）— 压缩比 ~6-12×
2. 批量文档产出（≥ 3 篇 1 个 session 内连续完成）— 提速 ~30-50%
3. 蓝图任务（无 build / 无 ctest / 无 debug）— 省去实施期惯例

---

## 10. 参考文档

- 设计 spec：`docs/specs/2026-04-30-devtool-design.md`
- 实施 plan：`docs/plans/2026-04-30-devtool.md`
- 创意 #1：`memory-bank/creative/creative-devtool-screen-layout.md`
- 创意 #2：`memory-bank/creative/creative-devtool-hot-reload.md`
- 回顾文档：`memory-bank/reflection/reflection-TASK-20260430-04.md`
- 关联 codebase review：`docs/reports/2026-04-30-codebase-review.md`（F-025 段含强依赖交叉记录）

---

## 11. 任务历史时间线

| 时间 | 阶段 | Commit | 备注 |
|---|---|---|---|
| 2026-04-30 22:55 | VAN 初始化（首次尝试，废弃）| — | 与 background agent 03 任务并发修改同名 MB 文件冲突；放弃旧 commit `44fc062` |
| 2026-04-30 23:00 | 重启 VAN 初始化 | — | TASK-30-03 background agent 完成 R2 + reflect + archive 并 merge main，04 分支 reset 到新 main `2445990` 重建基线 |
| 2026-04-30 23:05 | VAN 完成 | — | V1-V5 锁定（B/a/A/4/✅）+ F1-F9 grep 实证（5 ✅ / 4 ⚠️ / 6 🔴）+ 触及技术债 4 项映射 |
| 2026-05-01 01:01 | VAN commit | `b33d86f` | VAN 完成（含 MB 三件套同步）|
| 2026-05-01 01:50 | `/plan` 完成 | — | brainstorming D1-D8 全部锁定（用户连续 8 次跳过 AskQuestion 后按 VAN 推荐默认）；4 篇文档落盘（spec / plan / 2 creative）|
| 2026-05-01 02:00 | `/plan` commit | `5b802b5` | 一次性 commit 全部 4 篇文档 + 3 MB 同步 |
| 2026-05-01 02:30 | `/reflect` 完成 | `6e88a28` | reflection 文档落盘（10 节）+ plan ×0.6 第 17 数据点入库 + P0 全部 + P1 第 1 项改进落实 |
| 2026-05-01 ~03:00 | `/archive` 完成 | （本 commit） | 剩余 P1 #2 #6 #8 + P2 #5 #9 落实 + archive 文档 + MB 重置 + merge main |

---

**任务闭环 ✅** — Memory Bank 已重置为空闲状态，准备接受新任务。
