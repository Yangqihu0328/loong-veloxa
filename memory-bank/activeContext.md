# 活跃上下文

## 当前阶段

**规划完成** — TASK-20260505-05 G1.1 CMake `VX_RENDERER` flag（Level 2 / GLES 蓝图实施首步 / **MVP-C 战略主线第一个实施任务**）VAN ✅ + Plan ✅，待 `/build`。

**Plan 阶段产出（2026-05-05 ~19:00 / 实测 ~15-25 min / 极速区 ~0.5-0.8× 子档）：**

- **7/7 D 决策 1 次 AskQuestion all_recommended 锁定** — **跨决策协同度 100% 第 14 次连续命中** / dec → endec → doudec → 第 13 次 → **第 14 次** / 累计 121 → **128/128 历史最高 streak 刷新 ✅**
  - **D1=A** 暂不引入 GLES dep（YAGNI / 推迟 G1.2）
  - **D2=A** 顶层 CMakeLists.txt 校验
  - **D3=B** cmake -P 脚本（与 a14 一致）
  - **D4=B** tests/smoke/ 路径
  - **D5=A** 子进程 invalid assert（自动化 + 集成 ctest）
  - **D6=A** 仅 plan（引用 GLES 蓝图 spec / Level 2 实施类豁免）
  - **D7=A** P0 协议单 commit（quad → quint-evidence 候选 / 第 5 数据点）

- **蓝图 plan §3.1 偏差校正（brainstorming P1.3 主动 push-back 模式首次实战 ✅）：**
  - 偏差 #1：`find_package(OpenGLES/EGL REQUIRED)` → D1=A 暂不引入 dep（CMake 4.2.3 未自带 Find 模块）
  - 偏差 #2：`tests/cmake/*.sh` → D3=B + D4=B `tests/smoke/*.cmake`（与既有 a14 pattern 一致）
  - 偏差 #3：临时改宏反向探针 → D5=A 自动化子进程 invalid assert

- **主交付物（plan + Memory Bank ×3 / D7=A 自吃狗粮单 commit）：**
  - `docs/plans/2026-05-05-cmake-vx-renderer-flag.md`（~400 行 / 11 段全覆盖 / 含步骤 1-5 完整代码片段 + 双 build 矩阵 ctest + 4 沉淀候选）
  - `memory-bank/activeContext.md`（更新 Plan 阶段产出）
  - `memory-bank/tasks.md`（加 Plan 阶段决策矩阵 + 估时）
  - `memory-bank/progress.md`（加 Plan 阶段时间线）

- **Phase 0 audit 7/7 实证 ✅**（既有 option pattern + add_compile_definitions 范式 + SDL2 双轨 find pattern + EGL/GLES dev headers + find_package 不可用偏差 + pkg-config 替代 + a14 cmake -P 集成 pattern）

- **estimaate（plan ×0.6）：** ~65-100 min 总线（plan ~20-30 + build ~30-45 + reflect ~10-15 + archive ~5-10）/ vs GLES 蓝图 plan §4 估时 ~120-180 min = **预期总线极速区 0.36-0.55×**

- **反复模式预审 0/8 命中**（VAN + Plan 两阶段全程保持 / 累计 19 模式连续抑制 / 历史新高继续刷新）

- **沉淀候选（reflect 阶段处理 / 4 项 P1）：**
  - systemPatterns 新段「CMake 依赖引入时机 YAGNI 原则」（D1=A 实证）
  - systemPatterns 升级「brainstorming P1.3 主动 push-back 模式 dual-evidence」（TASK-04 落地 + TASK-05 首次实战）
  - systemPatterns 升级「writing-plans P1.6 spec vs code 一致性 audit dual-evidence」
  - writing-plans P1.5 段 P0 协议 quint-evidence 实证表升级（quad → quint / 第 5 数据点）

**当前任务：** TASK-20260505-05 — `G1.1 CMake VX_RENDERER flag` / Level 2 / 分支 `feature/TASK-20260505-05-cmake-vx-renderer-flag`（基于 main `0a90481` ✅ 创建）

**下一步：** `/build` — 进入构建阶段，按 plan §3 步骤 1-5 实施（步骤 1 Phase 0 audit 已完成 ✅ → 步骤 2 编写 cmake -P 脚本 → 步骤 3 实现顶层 CMakeLists flag → 步骤 4 双 build 矩阵 ctest 验证 → 步骤 5 single commit）。

**任务范围（来自 [GLES 蓝图 plan §3.1](docs/plans/2026-05-05-gles-renderer-blueprint.md)）：**

- **目标：** 落地 `VX_RENDERER=software|gles` CMake flag / 编译期分支 / `VX_RENDERER_GLES=1` / `VX_RENDERER_SOFTWARE=1` 宏 / 双 build 矩阵 ctest 验证
- **文件影响：** 2 修改 + 1 创建
  - 修改：`CMakeLists.txt`（顶层 / +~30 行 / option + 校验 + compile definitions）
  - 修改：`veloxa/graphics/CMakeLists.txt`（+~20 行 / VX_RENDERER 分支 / pkg_check_modules egl + glesv2）
  - 创建：`tests/cmake/vx_renderer_flag_test.sh`（CMake flag 验证脚本 / 双 build 矩阵）
- **B5 默认行为：** `VX_RENDERER=software`（不退化既有 ctest 1302/1109 baseline）
- **B6 嵌入策略：** 暂不实施（仅 flag 落地 / shader 静态嵌入留 G1.4）

**Phase 0 audit 预跑（VAN 阶段 7/7 实证 + 1 plan 偏差点发现 ✅）：**

| # | 项 | 结果 |
|:-:|---|:-:|
| 1 | 既有 `option()` flag pattern | ✅ 5 项 / 模式一致 |
| 2 | 既有 `add_compile_definitions()` 范式 | ✅ 顶层 line 17 |
| 3 | 既有 `pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)` pattern | ✅ graphics line 3 |
| 4 | EGL/GLES dev headers 可用性 | ✅ libegl-dev 1.7 + libgles-dev 1.7 |
| 5 | `find_package(OpenGLES/EGL)` Find 模块可用性 | ⚠️ **不可用** / **蓝图 plan §3.1 步骤 2 代码示例偏差** |
| 6 | `pkg-config egl glesv2` 替代方案 | ✅ 可用 / 与既有 HARFBUZZ pattern 一致 |
| 7 | `tests/cmake/` 目录 | ❌ 不存在 / 需 plan 阶段创建 |

**plan 阶段需处理的偏差点（来自 P1.3 brainstorming 主动 push-back 模式 / 刚 TASK-05-04 落地）：**

- ⚠️ **plan §3.1 步骤 2 代码示例偏差：** `find_package(OpenGLES REQUIRED)` + `find_package(EGL REQUIRED)` 在 CMake 4.2.3 **未自带 Find 模块**；建议 /plan 阶段切换到 `pkg_check_modules(GLESv2 REQUIRED glesv2)` + `pkg_check_modules(EGL REQUIRED egl)`，与既有 HARFBUZZ pattern 一致。
- 偏差度评估：**显著但限定范围**（仅影响实施代码片段 / 不动整体架构 / 0 倒退既有 build）→ /plan 阶段处理（修正实施代码示例 + 加 systemPatterns 沉淀「CMake Find 模块 vs pkg-config 选择规约」候选）。

**估时（plan ×0.6）：** ~2-3 h（来自 GLES 蓝图 plan §4 估时表 G1.1）/ 预期实测 ~30-60 min（极速区 0.2-0.4× 系数 / Level 2 + 单一目标 + 既有 pattern 复用）

**前置验证通过 ✅（4 维度）：**

| 维度 | 结果 |
|---|---|
| 依赖可获取性 | ✅ EGL 1.7 + GLES3 + pkg-config egl/glesv2 全在 |
| 环境就绪 | ✅ CMake 4.2.3 + GCC 14+ + ctest 1302/1109 baseline |
| 已有 artifact | ✅ 2 CMakeLists 已存在（待修改）/ tests/cmake/ 待创建（plan 阶段确认）|
| 待处理事项关联 | ✅ activeContext「下一推荐任务」#1 + GLES 蓝图 plan §3.1 完整规格化 |

**安全相关：** ❌ 否（仅构建系统 flag / 0 输入处理 / 0 新威胁面）

**最近闭环：** TASK-20260505-04 工作流元任务批量落地（Level 2-3 工作流元任务 / **dual-evidence 第 2 实证** / 沿用 [TASK-20260503-02 范式](memory-bank/archive/archive-TASK-20260503-02.md)）✅ 已归档闭环 / 分支 `feature/TASK-20260505-04-workflow-meta-batch` 已合并 main 并删除。

**TASK-04 总产出：**

- **14.5 / 14.5 子项 100% 落地 ✅**（10 P1 + 4 P2 / 6 文件 / +897 行 build / +407 行 reflect / +568 行 plan / ~250 行 archive ≈ ~2122 行总产出）
- **5 个 systemPatterns 沉淀**（2 新段 + 2 累计升级 + 1 既有段升级 / 全部已落实）
- **5 个范式里程碑**（dual-evidence + quad-evidence + 13 次连续命中 + oct-evidence + 极致 dogfooding）
- **跨决策协同度：** 8/8 D 决策 1 次 AskQuestion all_recommended 锁定 / 第 13 次连续命中 / 累计 121/121 历史最高 streak
- **plan ×0.6 实测系数：** 全任务 ~0.36-0.48× 极速区（Build 阶段 0.11-0.19× 极致极速区 / 工作流元任务子档新低）
- **反复模式抑制：** 0/8 全程 4 阶段保持（VAN + Plan + Build + Reflect）+ 累计 17 模式连续抑制 / 历史新高
- **极致 dogfooding 三层闭环 ✅**（D8=A P0 协议自吃狗粮 + P2.3 重复 anchor VAN 即时启用 + P2.2 LOC ×1.3 buffer 实测印证）

**ctest baseline 保持：** DEVTOOL=ON 1302/1302 + DEVTOOL=OFF 1109/1109（仅文档/规则改动 / 0 编译影响）

---

## 下一推荐任务（基于 spec §11.2 + §6.2 推荐立项顺序）

> 🚀 **MVP-C 战略主线已启动** — TASK-20260505-03 G1 OpenGL ES 蓝图已完整闭环（spec + plan + creative ×3 / 18 子任务规格化）/ TASK-20260505-04 工作流元任务批量清零完成 → **可立即进入 G1 OpenGL ES 实施阶段**。

| 优先 | 候选任务 | MVP 档 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|:-:|
| **1** | **G1.1 CMake VX_RENDERER flag**（GLES 蓝图首步实施 / 闭环架构开关）| MVP-C 核心 | **L2** | ~1-2 h |
| 2 | G1.2 GLESDisplay + Sdl2EGLDisplay（GLES context 创建）| MVP-C 核心 | L3 | ~2-3 h |
| 3 | R9 EventManager HitTest 改造（HUD pointer-events 真支持）| MVP-C | L2-3 | ~1.5-2 h |
| 4 | 资源加载策略蓝图（HTTP / file:// / data: URI 完整支持）| MVP-C 过渡 | L3 蓝图 + 实施 | ~5-10 h |
| 5 | G2 DRM/KMS 嵌入式后端蓝图 | MVP-C 核心 | L3-4 V2=a | ~10-20 h |
| 6 | DomBindings 节点动态创建删除 | MVP-C | L3 | ~3-5 h |
| 7 | CSS 高级特性 5 项 | MVP-C | 5 × L2-3 | ~10-20 h |
| 8 | 图像扩展 3 项（GIF / WebP / 异步加载）| MVP-C | 3 × L2 | ~6-12 h |
| 9 | 性能优化收口（含 #35 阶段 2 / R3+ 13 项）| MVP-C | 多 L2-3 | ~10-30 h |
| **元** | **下次工作流元任务批量落地**（累计 3 项 P1/P2 待处理事项 — 与 TASK-03-02 + TASK-05-04 范式 triple-evidence 候选）| 工作流 | L2 | ~30-60 min |

**当前焦点：** 进入 G1 OpenGL ES 实施阶段（建议从 G1.1 CMake VX_RENDERER flag 开始 / 详见 [docs/plans/2026-05-05-gles-renderer-blueprint.md](../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3 18 子任务清单）

---

## 待处理事项 — 跨任务沉淀（按优先级）

### 留下次工作流元任务批量落地（P1 / 累计 1 项 / dual → triple-evidence 候选）

> TASK-05-04 已批量清零 14.5 项 P1+P2 累计沉淀 ✅。本段只列 TASK-05-04 reflect 阶段新发现的 P1 项 — 等待下次工作流元任务批量清零（累计 ≥ 4 项时立项 / 沿用工作流元任务 dual-evidence 范式）。

- **P1 #1（来自 TASK-20260505-04 reflection §5 #1 / 新发现）`writing-plans.mdc` 「附录：LOC 估算 — 隐性附加工作类型清单」段补「表格密度系数」子条** — plan 阶段对「commit body 范本表」+「触发条件矩阵」+「实证表」+「交叉引用清单」类结构化内容的行数 underestimate（单段 4 表格 ~30-40 行 / plan 仅按段长 base 估算未计表格行数 / TASK-05-04 P2.1 段实际 75 行 vs 估 30 行 = ×2.5 偏差）；建议加表格密度系数子条：散文段 ~30-40 行 / 单表格 ~5-15 行/表 / **多表段（≥ 4 表格）×2-2.5 base 行数**。**预估**：~10 min。

### 长期沉淀（P2 — 不强制 archive / 累计 2 项）

- **P2 #1（来自 TASK-20260505-04 reflection §5 #2 / 新发现）`git-workflow.mdc` 「commit body Source 溯源 + 实测数据格式」段补「实测数据采集协议」子条** — TASK-05-04 Phase B.6 commit body 写「+44 行」/ 实际 git 显示 +39 行 / -5 行偏差，根因 commit body 写在 add 之前 / 凭目测估算 / 未做 `git diff --cached --stat` 二次确认；建议加「实测数据采集协议」子条：commit 前必须运行 `git diff --cached --stat` 实测后再写 commit body 数据。**预估**：~10 min。
- **P2 #2（来自 TASK-20260505-04 reflection §8.2 / 新发现）systemPatterns 「lazy-attach C ABI 容错模式 quad-evidence」段加 TASK-05-04 头部 doc 落地标注** — TASK-05-04 P1.7-half 在 `veloxa/api/veloxa_api.h` 顶部 doc 段追加「lazy-attach contract」节统一引用 4 个 quad-evidence ABI（vx_view_set_pipeline_hooks / vx_view_attach_devtool / vx_devtool_get_console_output / vx_view_invalidate）；建议在 systemPatterns quad-evidence 段加「头部 doc 已落地」标注 + 引用 commit `4765224`。**预估**：~5 min。

---

## P3 候选清单（用户优先级排期）

### 来自 TASK-20260504-01 spec §11.2 + 各历史任务 — 与上方「下一推荐任务」表对应

详见上方「下一推荐任务」段 + `docs/specs/2026-05-04-mvp-scope.md` §11.2。

### 8 项 P3 触发型候选（codebase review R1 已分析）

- TASK-26-02-full（clearance 完整版）
- TASK-26-03（LayoutInline IFC 递归 + bidi）
- TASK-20260424-02（Layout 残余 super-linear ~40%）
- CSS 4 标准逻辑属性 shorthand（`border-block` / `border-inline`）
- `border-image` / `border-radius` 简写
- TASK-20260419-06（HashMap Hash Mixing）
- TASK-20260419-08（`string.h` 剩余 memcpy noinline 化）
- TASK-20260419-12（DrawText 真路径优化，K7 隐式闭环待评估）

### 来自 TASK-20260503-02 reflection（codebase guideline 候选）

- **GoogleTest `ASSERT_TRUE(x.ok()) << x.status().message()` 短路评估易错模式 P3** — A-P1#6 audit CP2 扩展发现 tests/ 中 8 处该模式；建议 codebase guideline「测试中也用三元守卫显式化」。**预估**：~30 min audit + ~1 h codebase 修正。

---

## 收尾清理（可选）

- ✅ `feature/TASK-20260505-04-workflow-meta-batch` 分支已合并 + 删除（archive 阶段完成）
- ✅ `feature/TASK-20260505-03-gles-renderer-blueprint` 分支已合并 + 删除
- ✅ `feature/TASK-20260505-01-dombindings-r2-closure` 分支已合并 + 删除
- ✅ `feature/TASK-20260504-01-mvp-scope-doc` 分支已合并 + 删除
- 早期 feature 分支（TASK-20260430-* / TASK-20260502-* / TASK-20260503-*）如未删除可批量清理

---

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260505-04.md`（**工作流元任务批量落地 — 14.5 项 P1+P2 跨任务沉淀清零 Level 2-3，2026-05-05**）— **本批最新 / 工作流元任务 dual-evidence 第 2 实证 ✅ / 跨决策协同度 100% 第 13 次连续命中（累计 121/121 历史最高）/ 极致 dogfooding 三层闭环 first-evidence ✅ / plan ×0.6 oct-evidence + 双子档分化（V2=a 蓝图 0.02-0.05× + 工作流元 0.11-0.19×）/ P0 协议 quad-evidence 已固化 / 5 个范式里程碑 + 5 个 systemPatterns 沉淀 + 6/8 改进建议已落实 / 反复模式 0/8 4 阶段全程抑制（累计 17 模式连续抑制 / 历史新高）**
- `archive-TASK-20260505-03.md`（**G1 OpenGL ES 硬件渲染后端蓝图 Level 4 V2=a，2026-05-05**）— **🚀 MVP-C 战略主线启动里程碑** — G1 OpenGL ES 硬件渲染后端蓝图（核心目标 #2「嵌入式硬件加速」第一刚需）/ 13 决策矩阵 + 18 实施子任务规格化 / 单 commit P0 协议首次完整实施 / 3 个范式升级同时落地（doudec-evidence + 极致极速区 0.02-0.05× + V2=a triple-evidence）/ Level 4 V2=a 蓝图 / 总投入 ~30-40 min vs plan ×0.6 ~17-25 h = **0.02-0.04× 极致极速区**
- `archive-TASK-20260505-02.md`（Performance Overlay 持续 invalidate 机制 Level 2，2026-05-05）— 🎉 **MVP-B 100% 闭环里程碑达成** / B-G4 / 5 个范式同时升级（plan ×0.6 sext + 跨决策协同度 dec + 反向探针 triple + 反复模式 #8 triple + lazy-attach quad）
- `archive-TASK-20260505-01.md`（**DomBindings R2 收口 — B-G1 children + B-G3 innerHTML setter + B-G2 audit Level 3，2026-05-05**）— MVP-B 完成度 90% → 95% / dogfood 视觉自动恢复链路三件齐 ✅ / 协议三件套里程碑（Phase 0 极速区 quint-evidence + 跨决策协同度 nona-evidence + 反向探针强度梯度三档 dual-evidence）/ 反复模式 #8 入库定型 / P0×4 reflect 全落实
- `archive-TASK-20260504-01.md`（MVP-scope 文档蓝图 Level 4 V2=a 完整变体，2026-05-04）— DevTool 4 件套主线收官标识 🎉 / 三档分级 MVP-A/B/C 体系建立 / 路线图按 MVP 档分层重写 / 核心目标 #1+#2 路径量化 / P0+P1+P2×4 archive 全落实 / 跨决策协同度 100% 第 8 次连续命中 / plan ×0.6 矩阵第 4 蓝图数据点 0.25-0.38× 入库
- `archive-TASK-20260503-04.md`（DevTool Phase D · Console JS REPL Level 3 [安全相关]，2026-05-04）— DevTool 4 件套全部完整闭环 ✅ / spec §11.1 完整闭环 ✅ / T1 5 维度首次完整暴露 ✅ / plan ×0.6 0.07-0.10× 创历史新低 / Phase 0 sept-evidence / P0×3 archive 阶段全落实
- `archive-TASK-20260503-05.md`（QuickJS Interrupt Handler + SetEvalInterruptBudget API Level 2 [安全相关]，2026-05-03）
- `archive-TASK-20260503-03.md`（DevTool 三件套主线收官 — 4 项 P3 候选批量清零 Level 2，2026-05-03）
- `archive-TASK-20260503-02.md`（**工作流/规则类技术债批量清理 Level 2，2026-05-03**）— **工作流元任务范式 first-evidence**
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
