# 活跃上下文

## 当前阶段

**规划完成** — TASK-20260505-03 G1 OpenGL ES 硬件渲染后端蓝图（MVP-C 核心 / Level 4 V2=a 蓝图任务）VAN ✅ + Plan ✅，待 `/reflect`。

**Plan 阶段产出（2026-05-05 ~17:30）：**

- **8/8 B 决策 1 次 AskQuestion all_recommended 锁定**（跨决策协同度 100% **第 12 次连续命中** / dec → endec → **doudec-evidence 候选** / 累计 113/113）：
  - **B1-A** SDL_GL_CreateContext + EGL 嵌入式接口预留
  - **B2-A** 混合（FillRect/RoundedRect = shader / FillPath = libtess2 + VBO / Stroke = Fill 转换）
  - **B3-A** CPU 光栅化 + GPU texture atlas（GL_R8 + 复用 FreeType + GlyphCache）
  - **B4-A** ComputeDirtyRect + glScissor + glClear（沿用既有 r3 dirty rect）
  - **B5-A** software 默认（VX_RENDERER=software\|gles CMake flag / GLES opt-in）
  - **B6-A** 静态嵌入 .glsl raw string literal（编译期绑定）
  - **B7-A** 既有 BM_Replay* + 新建 BM_GLESReplay* 同 corpus 双测对照（60fps 1080p budget）
  - **B8-A** 完整预留（ContextLost/Restore + GLESDisplay 抽象 + GpuFence 接口）

- **主交付物（V2=a 蓝图 / 共 3376 行）：**
  - `docs/specs/2026-05-05-gles-renderer-blueprint-design.md`（942 行 / 13 段全覆盖）
  - `docs/plans/2026-05-05-gles-renderer-blueprint.md`（773 行 / 18 子任务详细规格 + ctest 矩阵 + commit 范本）
  - `memory-bank/creative/creative-gles-context.md`（369 行 / B1 GL context 创建 / Context Lost 处理 / 版本协商）
  - `memory-bank/creative/creative-gles-canvas.md`（527 行 / B2 Canvas trampolining / shader-based vs tessellator / Stroke = Fill 转换）
  - `memory-bank/creative/creative-gles-resources.md`（765 行 / B3 GlyphAtlas + B4 dirty rect + B6 shader 资源 / 完整生命周期协议）

- **18 个 Level 3 实施子任务拆分**（用户后续基于本蓝图独立立项 / ~68-96 h plan ×0.6 / +30% buffer = ~88-125 h）：
  - G1.1 CMake VX_RENDERER flag (L2)
  - G1.2 GLESDisplay + Sdl2EGLDisplay (L3)
  - G1.3 Sdl2GLWindowSurface (L3)
  - G1.4 GLESCanvas 骨架 (L3)
  - G1.5 FillRect + FillRoundedRect + Solid Brush (L3)
  - G1.6 FillPath via libtess2 (L3)
  - G1.7 Stroke* (L3)
  - G1.8 GlyphAtlas + DrawText (L4)
  - G1.9 ImageTexturePool + DrawImage (L3)
  - G1.10 PushClipRect/PopClip + PushLayer/PopLayer (L3)
  - G1.11 dirty rect glScissor 集成 (L2)
  - G1.12 LinearGradient / RadialGradient SDF (L3)
  - G1.13 Application 构造分支 + fallback (L3)
  - G1.14 Context Lost / Restore (L3)
  - G1.15 examples/hello_sdl2 GLES smoke (L2)
  - G1.16 DevTool dogfood GLES smoke (L3)
  - G1.17 BM_GLESReplay* 性能基准 (L3)
  - G1.18 G2 接口预留 audit + GpuFence 头注释占位 (L2)

- **P0「plan/spec docs 落盘即 commit」协议首次完整实施 ✅**（TASK-20260505-02 首次成功 → 本任务首次完整执行 / plan + spec + creative ×3 + Memory Bank 单 commit 落盘）

**VAN 阶段产出（2026-05-05 ~16:25）：**

- **任务 ID：** TASK-20260505-03
- **任务类型：** Level 4 V2=a 蓝图任务（沿用 [TASK-20260430-04 DevTool 蓝图](memory-bank/archive/archive-TASK-20260430-04.md) + [TASK-20260504-01 MVP-scope 蓝图](memory-bank/archive/archive-TASK-20260504-01.md) 范式）
- **工作流变体：** `/van → /plan（含 brainstorm + creative ×N 内联）→ /reflect → /archive` — **跳过独立 `/build` 阶段**
- **安全相关：** ⚠️ **是** — GLES context 创建涉及 GPU 驱动 / EGL display 资源生命周期 / shader 编译错误处理 / GL extension 安全枚举
- **分支：** `feature/TASK-20260505-03-gles-renderer-blueprint`（基于 main `35e0486` ✅ 创建）
- **5 个 V 决策已锁定**（VAN 阶段 1 次 AskQuestion all_recommended / **跨决策协同度 100% 第 11 次连续命中** / dec → **endec-evidence** 候选）：
  - **V1-A** GLES only（OpenGL ES 3.0+ 完整蓝图 / Vulkan 仅预留接口位置）
  - **V2-A** pure_blueprint_a（V2=a 纯蓝图 / 不含 build / 用户后续套 N 个 Level 3 子任务）
  - **V3-A** desktop_first（桌面 SDL2+EGL/GLX 完整 + 嵌入式抽象接口预留 / DRM/KMS 详设留 G2）
  - **V4-A** co_design_boundary（G1 定义 Renderer/Surface 抽象 / G2 独立蓝图 / 划界协同）
  - **V5-A** vx_renderer_flag（VX_RENDERER=software\|gles CMake flag / SoftwareCanvas 作 fallback）
- **估时（plan ×0.6）：** ~25-40 h（V1=gles_only / V3=desktop_first / V4=co_design_boundary 综合）
- **主交付物预期：** spec（GLES 后端架构设计 / ~600-800 行）+ plan（N 个 Level 3 实施子任务拆分 / ~800-1200 行）+ creative ×3-5（GL context 创建 / Canvas trampolining / 资源生命周期 / shader 管线 / 性能基线协议）

**前置验证通过 ✅（4 维度）：**

| 维度 | 结果 |
|---|---|
| 依赖可获取性 | ✅ EGL + GLES3 dev headers + libgl1-mesa-dri 全部安装（Mesa 26.0.3）|
| 环境就绪 | ✅ ctest DEVTOOL=ON 1302/1302 baseline / 既有 Graphics HAL 抽象（Canvas + Surface）作为蓝图基础 |
| 已有 artifact | ✅ `docs/specs/2026-04-05-graphics-platform-hal-design.md` 既有 Canvas 纯虚 + SoftwareCanvas impl 范式可复用 |
| 待处理事项关联 | ✅ spec §11.2 推荐 #5 / activeContext「下一推荐任务」#1（G1 OpenGL ES / L4 多 Phase / ~30-60+ h plan ×0.6）|

**当前任务：** TASK-20260505-03 — `G1 OpenGL ES 硬件渲染后端蓝图` / Level 4 V2=a / 分支 `feature/TASK-20260505-03-gles-renderer-blueprint`

**下一步：** `/plan` — 进入规划阶段，brainstorm + creative ×N 内联（V2=a 变体 / 蓝图主交付 = spec + plan + creative ×N）。

---

## 上次任务（已归档闭环）

### TASK-20260505-02 Performance Overlay 持续 invalidate 机制（Level 2）— ✅ 已归档（commit `33ebc99`）

**最近闭环（保留供下游任务参考）：** **🎉 MVP-B 100% 闭环里程碑达成** — TASK-20260505-01（B-G1+G2+G3 / DomBindings R2）+ TASK-20260505-02（B-G4 / vx_view_invalidate ABI）双任务连击实证 / 4/4 gap 全闭环 / dogfood 视觉链路完整恢复 / 5 件套 dogfood smoke 100% PASS。本次启动 G1 OpenGL ES 蓝图即开启 MVP-C 战略主线。

- **归档文档：** `memory-bank/archive/archive-TASK-20260505-02.md`（~408 行 / Level 2 详细归档 / 10 段 / 度量数据汇总表）
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260505-02.md`（~280 行 / Level 2 详细回顾 / 8 段 / 8 改进建议 P0/P1/P2 全分级）
- **主交付：** 7 commits / +198 行 code + ~640 行 MB/docs（含 reflection 280 行 + plan 1001 行 + spec 544 行）/ 4 新单测 / DEVTOOL=ON 1298→1302 + DEVTOOL=OFF 1105→1109 / hello_devtool_perf_smoke frames=1→**18**（10x 增益）
- **跨决策协同度：** 4/4 决策 1 次 AskQuestion 锁定 / 第 10 次连续 100% 命中 / 累计 100/100 / nona → **dec-evidence** 升级
- **plan ×0.6 比值：** 0.26-0.32×（实测 ~30 min vs plan ×0.6 95-115 min）/ 落极速区 0.10-0.20× 续延档 / quint → **sext-evidence** 升级
- **反向探针：** 3/3 精准有效 / 强度梯度三档全谱（A.1 NULL guard 过高 SEGFAULT / A.1 dirty_ rearm 合适 2/4 / D.1 regex 灵敏度 平衡 1/1）/ dual → **triple-evidence** 升级
- **反复模式命中：** 0/7 全抑制（连续 4 任务保持 0 命中）+ #8 spec 数据回归第 3 次实证 dual → **triple-evidence** 升级（已达固化阈值）
- **lazy-attach C ABI 容错模式：** 第 4 次复用 / triple → **quad-evidence** 升级 / 已成 Veloxa 默认范式
- **ctest：** DEVTOOL=ON 1298→1302 + DEVTOOL=OFF 1105→1109（双 +4）/ dogfood smoke 3/3 PASS

**8 项 P0+P1+P2 改进建议落实情况：**

| # | 建议 | 优先级 | 落实位置 |
|:-:|---|:-:|---|
| 1 | systemPatterns「plan ×0.6 sext-evidence」段 | **P0** | ✅ reflect 阶段立即沉淀 |
| 2 | systemPatterns「跨决策协同度 dec-evidence」段 | **P0** | ✅ reflect 阶段立即沉淀 |
| 3 | systemPatterns「反向探针强度梯度三档 triple-evidence」段 | **P0** | ✅ reflect 阶段立即沉淀 |
| 4 | systemPatterns「反复模式 #8 triple-evidence」段 | **P0** | ✅ reflect 阶段立即沉淀 |
| 5 | systemPatterns「lazy-attach C ABI quad-evidence」段 | **P0** | ✅ reflect 阶段立即沉淀 |
| 6 | `/plan` 命令固化「plan/spec docs 落盘即 commit」步骤 | **P0 → 升级** | ✅ 沉淀范例 + 📋 待处理事项 P1 #6 |
| 7 | systemPatterns「MVP-B 100% 闭环里程碑」段 | **P1** | ✅ reflect 阶段直接沉淀 |
| 8 | techContext「CMake + GTest 增量加测工作流注意事项」段 | **P2** | ✅ reflect 阶段直接沉淀 |

**P0 5/5 + P0 升级 1/1 reflect 阶段直接落实 ✅** + **P1 1/1 (#7) reflect 阶段直接沉淀** + **P2 1/1 (#8) techContext 沉淀**（沿用 [TASK-20260505-01 P0 4/4 全落实范式](memory-bank/archive/archive-TASK-20260505-01.md)）

---

<details>
<summary>历史详细产出（点开展开 / 任务已归档闭环）</summary>

**Reflect 阶段产出（2026-05-05 ~16:00）：**

- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260505-02.md`（~280 行 / Level 2 详细回顾 / 8 段 / 6 度量数据）
- **5 个范式同时升级**（reflection 史上单任务沉淀升级数最高纪录）：
  - **plan ×0.6 实测系数 sext-evidence**（quint → sext / 第 6 数据点 / 极速区续延档新子档）
  - **跨决策协同度 100% dec-evidence**（nona → dec / 第 10 次连续 / 累计 100/100）
  - **反向探针强度梯度三档 triple-evidence**（dual → triple / 第 2 任务实证）
  - **反复模式 #8 spec 数据回归 triple-evidence**（dual → triple / 已达 writing-plans.mdc 固化阈值）
  - **lazy-attach C ABI 容错模式 quad-evidence**（triple → quad / 已成 Veloxa 默认范式）
- **MVP-B 100% 闭环里程碑** — TASK-01 + TASK-02 双任务连击实证 / 4/4 gap 全闭环 / dogfood 视觉链路完整恢复 / 5 件套 dogfood smoke 100% PASS

**Build 阶段产出（2026-05-05 ~15:30 / 实测 ~30 min）：**

- **3 commits 总计 +198 lines / 6 files changed：**
  - `a7e6bed` feat(api): vx_view_invalidate() public C ABI [B-G4] — Phase A.1 + B.1 / 4 单测 4/4 PASS / 反向探针 2/2 精准
  - `929569a` feat(devtool): hello_devtool perf smoke multi-frame validation [B-G4] — Phase C.1 + D.1 / **frames=18** / 反向探针 1/1 精准
  - `8d00aee` docs(spec): MVP-B 100% — B-G4 closed [TASK-20260505-02] — Phase E.1 / B-G4 ✅ + 完成度 95% → 100%
- **ctest 实测矩阵：** DEVTOOL=ON 1298 → **1302**（+4 PASS / 100%）/ DEVTOOL=OFF 1105 → **1109**（+4 PASS / 100%）— 与 plan 预期完全一致 ✅
- **dogfood smoke：** hello_devtool 3 件套 3/3 PASS（perf_smoke `frames=18` / inspector / hot_reload 全 regression-free）
- **TDD 严格度：** 1 phase TDD 三阶（RED 编译 fail → GREEN 4/4 PASS → REFACTOR）+ 反向探针 3 项（A.1 NULL guard SEGFAULT 过高档 + A.1 dirty_ rearm 2/4 合适档 + D.1 ctest regex 灵敏度 1/1 平衡档）— 强度梯度三档全谱覆盖
- **plan ×0.6 实测系数：** ~0.26-0.32×（实测 ~30 min vs plan ×0.6 95-115 min）— 落极速区 0.10-0.20× 续延档 / **sext-evidence** 候选（第 6 次命中数据点）
- **反复模式预防：** 0/8 全抑制（含 #8 spec 数据回归 dual-evidence → triple-evidence 候选 / VAN 阶段已实证暴露 + 修正路径 b → 路径 a）

**MVP-B 100% 闭环 🎉：** B-G1+G2+G3+G4 全 4 项 gap 全部闭环 / dogfood 视觉验证 3/3 PASS / hello_devtool_perf_smoke 多帧验证 frames=18

**当前任务：** TASK-20260505-02 — `vx_view_invalidate()` 公开 C ABI / Level 2 / 分支 `feature/TASK-20260505-02-perf-overlay-invalidate-api`（基于 main `8caa9ba`）

**下一步：** `/reflect` — 进入回顾阶段，整理 Build 阶段成功/挑战/经验沉淀（dec-evidence + sext-evidence + triple-evidence 升级 + 蓝图任务节奏档案）。

---

## 上次任务（已归档闭环）

### TASK-20260505-01 DomBindings R2 收口（B-G1 children + B-G3 innerHTML + B-G2 audit）— ✅ 已归档（commit `6f924fd`）

**最近闭环（保留供下游任务参考）：** **MVP-B 完成度 90% → 95%** ✅ + dogfood 视觉自动恢复链路三件齐 ✅ + Veloxa JS API surface 显著扩张（children getter + innerHTML setter + 4 mouse alias）+ `CloneNodeInto` helper 就位（未来 cloneNode/Range/Fragment 复用基础）+ 协议三件套里程碑（Phase 0 极速区 **quint-evidence** 5 数据点 / 跨决策协同度 **nona-evidence** 第 9 次连续 100% / 反向探针强度梯度 **dual-evidence** 三档全谱）+ 反复模式 **#8 spec 数据回归 dual-evidence 入库定型** ✅。

**MVP-B 即将收口** — 仅剩 B-G4 Performance Overlay 持续 invalidate 机制（~30 min-2 h plan ×0.6 / Level 1-3 任务）。

---

</details>

## 更早任务（详细归档信息）

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

> 🚀 **TASK-20260505-03 G1 OpenGL ES 蓝图任务进行中**（VAN ✅ → 待 `/plan`）— 推荐序号已下移。

| 优先 | 候选任务 | MVP 档 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|:-:|
| **进行中** | **G1 OpenGL ES 硬件渲染后端蓝图（TASK-20260505-03）** | MVP-C 核心 | **L4 V2=a 蓝图** | ~25-40 h |
| 1 | 资源加载策略蓝图（HTTP / file:// / data: URI 完整支持）| MVP-C 过渡 | L3 蓝图 + 实施 | ~5-10 h |
| 2 | R9 EventManager HitTest 改造（HUD pointer-events 真支持）| MVP-C | L2-3 | ~1.5-2 h |
| 3 | G2 DRM/KMS 嵌入式后端 | MVP-C 核心 | L3-4 | ~10-20 h |
| 4 | DomBindings 节点动态创建删除 | MVP-C | L3 | ~3-5 h |
| 5 | CSS 高级特性 5 项 | MVP-C | 5 × L2-3 | ~10-20 h |
| 6 | 图像扩展 3 项（GIF / WebP / 异步加载）| MVP-C | 3 × L2 | ~6-12 h |
| 7 | 性能优化收口（含 #35 阶段 2 / R3+ 13 项）| MVP-C | 多 L2-3 | ~10-30 h |
| **元** | **工作流元任务批量落地**（累计 8 项 P1 待处理事项 — sept-evidence 已超固化阈值）| 工作流 | L2-3 | ~1-2 h |

**当前焦点：** 进入 G1 OpenGL ES 蓝图阶段（V1=gles_only / V2=pure_blueprint_a / V3=desktop_first / V4=co_design_boundary / V5=vx_renderer_flag）/ 详见 `docs/specs/2026-05-04-mvp-scope.md` §11.2

---

## 待处理事项 — 跨任务沉淀（按优先级）

### 留下次工作流元任务批量落地（P1）

> 累计 P1 改进建议待批量沉淀到 `.cursor/rules/skills/*.mdc` — 等待下次工作流/规则类技术债清理任务（沿用 [TASK-20260503-02 工作流元任务范式](memory-bank/archive/archive-TASK-20260503-02.md)）。

- **P1 #1（来自 TASK-20260503-04 reflection §5）writing-plans.mdc Phase 0 段补强「JS context 归属与 host binding 注册 ctx 一致性 audit」子条** — 反复模式 #1 第 4 个新形式（panel JS / 用户脚本 ctx 归属未实证）；本任务 plan-fact reconcile #1（C2 wiring）即此模式实证。**预估**：~10 min。
- **P1 #2（来自 TASK-20260503-04 reflection §5）writing-plans.mdc「资源类反向探针 SOP」新子段** — 资源反向探针应限定到非注释区域 + comment policy 推荐；本任务 D.3 console_panel.html 注释里 `<input` 字面量触发反向探针 false positive。**预估**：~10 min。
- **P1 #2（来自 TASK-20260503-05 reflection §5）brainstorming.mdc 加新段「Phase 0 grep 实证驱动的主动 push-back 模式」** — D8b 实证（brainstorm scope 已被 core_only 限定后，Phase 0 grep 发现 creative 文档 10⁷ 检查点字面值会导致 100-1000s 死循环灾难）→ 必须**主动**抛出而非等用户问到；触发条件清单：(1) brainstorm scope 已被用户限定 + (2) Phase 0 grep / audit 阶段发现偏差 + (3) 偏差**显著**（默认值差 10³+ 倍）。**预估**：~10 min。
- **P1 #5（来自 TASK-20260505-01 reflection §5 #5）systemPatterns.md 新沉淀「视觉链路三件齐识别协议」段** — 当 dogfood UI 行为依赖 ≥3 个独立缺陷修复才能完整工作时，必须**单任务集中闭环**而非分多任务拆分；plan 阶段 §UI 行为验收表是识别工具（本任务 plan §0.11「视觉恢复链路」表是范式）。**预估**：~30-40 行 systemPatterns 段 / ~10 min。
- **P1 #6（来自 TASK-20260505-01 reflection §5 #6 → TASK-20260505-02 首次成功实施 ✅）`/plan` 命令固化「plan/spec docs 落盘即 commit」步骤** — TASK-20260505-02 `/plan` 阶段 commit `4feda52` 即 plan + spec + MB 更新合并提交 / build 阶段 0 collateral commit / **首次成功实施 ✅**；建议升级到 P0 立即固化协议到 `.cursor/rules/skills/writing-plans.mdc` 或 `.cursor/commands/plan`。**预估**：~10 min。
- **P1 #7（来自 TASK-20260505-02 reflection §5 #4 → triple-evidence 升级 / 已达固化阈值）反复模式 #8 spec 数据回归 audit 协议固化到 `.cursor/rules/skills/writing-plans.mdc`** — Phase 0 audit 段「spec vs code 一致性 audit」+「能力假设 audit」双子条；TASK-20260504-01 + TASK-20260505-01 + TASK-20260505-02 三次实证累计已达固化阈值。**预估**：~15 min。
- **P1 #8（来自 TASK-20260505-02 reflection §5 #5 → quad-evidence 升级 / 已成 Veloxa 默认范式）lazy-attach C ABI 容错模式固化** — sytemPatterns quad-evidence 已沉淀；建议下次工作流元任务批量落地时 (a) `.cursor/rules/skills/writing-plans.mdc` C ABI 设计模式段「lazy-attach 默认契约」子条 + (b) `veloxa/api/veloxa_api.h` 顶部 doc 段添加「lazy-attach contract」一节统一引用前序 ABI。**预估**：~15 min。
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
