# 活跃上下文

## 当前阶段

**初始化** — TASK-20260505-06 G1.2 `GLESDisplay` 抽象 + `Sdl2EGLDisplay` 实施（Level 3 / GLES 蓝图实施第二步 / **MVP-C 战略主线第二个实施任务** / G1.1 D1=A 推迟点正式落地：引入 EGL/GLES dep）VAN ✅ → 待 `/plan`。

**当前任务：** TASK-20260505-06 / Level 3 / 分支 `feature/TASK-20260505-06-gles-display-sdl2-egl`（基于 main `ee2569d` ✅ 创建 / G1.1 已合并）

**VAN 阶段产出（2026-05-05 ~20:15 / 实测 ~10-15 min）：**

- **任务范围（来自 [GLES 蓝图 plan §3.2](docs/plans/2026-05-05-gles-renderer-blueprint.md#32-子任务-g12--glesdisplay-抽象--sdl2egldisplay-实施)）：**
  - **目标：** 落地 `GLESDisplay` 纯虚抽象（G2 共享接口预留 / B8 决策落地）+ `Sdl2EGLDisplay` SDL2 子类实施 + ~6-8 单测 + 反向探针
  - **文件影响（plan 估）：** 3 创建 + 1 创建（test）+ 1 修改（CMakeLists）= 5 文件
    - 创建：`veloxa/platform/gles_display.h`（~80 行 / 纯虚抽象 / G2 桥接接口预留）
    - 创建：`veloxa/platform/sdl2/sdl2_egl_display.h`（~50 行）
    - 创建：`veloxa/platform/sdl2/sdl2_egl_display.cc`（~150 行 / SDL_GL_* 实施）
    - 创建：`tests/platform/sdl2_egl_display_test.cc`（~200 行 / ~6-8 单测）⚠️ plan 偏差：plan 说 `tests/platform/sdl2/` 子目录，实际 tests/platform 扁平结构
    - 修改：`veloxa/platform/sdl2/CMakeLists.txt`（+~5 行 / 加 sdl2_egl_display.cc + EGL/GLES dep）⚠️ plan 偏差：plan 说改顶层 platform/CMakeLists.txt
  - **依赖引入（D1=A 推迟点）：** `pkg_check_modules(EGL REQUIRED egl)` + `pkg_check_modules(GLESv2 REQUIRED glesv2)`（与 HARFBUZZ pattern 一致）

- **Phase 0 audit 11/11 实证 ✅：**

  | # | 项 | 结果 |
  |:-:|---|:-:|
  | 1 | SDL2 dev 可用性（pkg-config sdl2 2.32.10 / 远超 2.0.20+）| ✅ |
  | 2 | SDL_GL_* 5 个 API（CreateContext / MakeCurrent / SwapWindow / DeleteContext / SetAttribute）| ✅ 全在 |
  | 3 | EGL dev（libegl-dev 1.7.0-3 / pkg-config egl 1.5）| ✅ |
  | 4 | EGL API（eglGetCurrentDisplay / eglGetCurrentContext / EGL_NO_CONTEXT）| ✅ 全在 |
  | 5 | GLES3 dev（libgles-dev 1.7.0-3 / pkg-config glesv2 3.2）| ✅ |
  | 6 | GLES3 API（glGetString / glGetStringi / GL_VERSION）+ GL_CONTEXT_LOST_KHR（KHR_robustness）| ✅ 全在 |
  | 7 | Mesa headless drivers（swrast_dri + kms_swrast_dri + libEGL_mesa）| ✅ |
  | 8 | 既有 SDL2 双轨 find_package pattern（sdl2/CMakeLists.txt L7-15）| ✅ 可复用 |
  | 9 | 既有 platform::Surface 抽象（surface.h / 24 行 header-only）| ✅ GLESDisplay 应平级 |
  | 10 | tests/platform/ 扁平结构（5 既有 _test.cc / 无 sdl2/ 子目录）| ⚠️ plan §3.2 偏差点 |
  | 11 | G1.2 目标 artifact 不存在（gles_display.h / sdl2_egl_display.{h,cc} / test）| ✅ 0 冲突 |

- **plan §3.2 偏差点（来自 brainstorming P1.3 主动 push-back / 已实战 dual-evidence 模式）：**
  - **偏差 #1：** plan 说「修改顶层 `veloxa/platform/CMakeLists.txt` +~10 行」→ 实际：GLESDisplay.h 是**纯虚 header-only**（spec §3.3.2 / 0 .cc）+ 顶层用 `target_include_directories(... ${CMAKE_SOURCE_DIR})` 隐式头扫描 → **顶层 0 修改**；Sdl2EGLDisplay.cc 加到 **`veloxa/platform/sdl2/CMakeLists.txt`**（与 sdl2_window_surface.cc 平级 / +~5 行）
  - **偏差 #2：** plan §3.2 测试路径 `tests/platform/sdl2/sdl2_egl_display_test.cc` → 实际 tests/platform 是**扁平结构**（5 既有 _test.cc 直接在 tests/platform/）→ 沿用扁平 → `tests/platform/sdl2_egl_display_test.cc`
  - **偏差 #3：** plan §3.2 步骤 1 测试设计未明示 headless CI 适配（SDL_VIDEODRIVER=offscreen 或 dummy 或 EGL_PLATFORM=surfaceless）→ /plan 阶段决策 testing fixture 策略
  - **偏差度评估：** 中等限定范围（仅影响实施代码片段 + 测试 setup / 不动整体架构 / 0 倒退既有 build）

- **反复模式预审 0/8 命中**（VAN 阶段 / 累计 19 模式连续抑制 / 历史新高继续刷新）

- **估时（plan ×0.6）：** ~4-6 h（GLES 蓝图 plan §3.2）/ 预期实测 ~1.5-3 h（实施类 Level 3 子档 / 标准极速区 0.4-0.6× / 含 ~3 偏差校正 + headless GL 测试 setup）

**前置验证通过 ✅（4 维度）：**

| 维度 | 结果 |
|---|---|
| 依赖可获取性 | ✅ EGL 1.5 + GLES 3.2 + SDL2 2.32.10 + Mesa swrast headless 全在 |
| 环境就绪 | ✅ CMake 4.2.3 + GCC 14+ + ctest 1303/1110 baseline + G1.1 VX_RENDERER=gles 1303 PASS |
| 已有 artifact | ✅ G1.2 目标 4 文件全不存在 / 0 冲突 |
| 待处理事项关联 | ✅ G1.1 D1=A 推迟点（引入 EGL/GLES dep）正式落地节点 |

**安全相关：** ❌ 否（仅平台抽象 + GL context 创建 / 0 输入处理 / 0 网络 / 0 新威胁面）

**最近闭环：** TASK-20260505-05 G1.1 CMake `VX_RENDERER` flag（Level 2）✅ 已归档闭环 / 9 systemPatterns 沉淀 / 7 范式里程碑 / quint-evidence + 14 次连续命中 + ennea-evidence。

**下一步：** `/plan` — 进入规划阶段，brainstorm 决策矩阵（候选议题：testing fixture 策略 / context lost 测试覆盖度 / GLES extension 查询 cache 策略 / 反向探针实施方式 / 偏差点处理 / commit 粒度 / P0 协议复用）。

**TASK-05 总产出：**

- **9 个 systemPatterns 沉淀** ✅（reflect 阶段全部直接落地 / reflection 史上单任务沉淀次高纪录）
  - 跨决策协同度 100% 第 14 次连续命中（128/128 历史最高 streak）
  - plan ×0.6 ennea-evidence（第 9 数据点 + 实施类 Level 2 子档新增）
  - brainstorming P1.3 主动 push-back dual-evidence（首次实战 / 3 偏差校正）
  - writing-plans P1.6 spec vs code audit dual-evidence
  - **CMake 依赖引入时机 YAGNI 原则 first-evidence**（D1=A 实证）
  - **ctest cmake -P stub probe 范式 first-evidence**（600-1200× 加速）
  - **REFACTOR 涌现单一真相源模式 first-evidence**（cmake/VxRenderer.cmake）
  - P0 协议 quint-evidence（quad → quint / 实施类首次实证 / 适用性矩阵 5 类全覆盖）
  - LOC ×1.3-1.5 buffer dual-evidence（×1.4 命中 buffer 上限）
- **7 范式里程碑：** quint-evidence + 14 次连续命中 + 128/128 streak + ennea-evidence + 实施忠实度新维度 + REFACTOR 涌现单一真相源 first-evidence + cmake -P stub probe 600-1200× 加速 first-evidence
- **writing-plans.mdc P1.5 段升级 ✅** — quad → quint-evidence 实证表 + 适用性矩阵
- **跨决策协同度：** 7/7 D 决策 1 次 AskQuestion all_recommended 锁定 / 第 14 次连续命中 / 累计 128/128 历史最高 streak / **实施忠实度新维度入库**（plan→build 0 偏差实施）
- **plan ×0.6 实测系数：** 全任务 ~0.6-1.0× 标准极速区（实施类 Level 2 子档新增）
- **反复模式抑制：** 0/8 全程 4 阶段保持（VAN + Plan + Build + Reflect）+ 累计 19 模式连续抑制 / 历史新高继续刷新
- **改进建议落实：** 12 项（P0×0 + P1×9 + P2×3）/ P1×9 reflect 阶段全直接落地 ✅ / P2×3 累积下次工作流元任务

**ctest baseline 生效（main 分支）：**
- DEVTOOL=ON / VX_RENDERER=software（default）：**1303/1303**
- DEVTOOL=OFF / VX_RENDERER=software（default）：**1110/1110**
- DEVTOOL=ON / VX_RENDERER=gles：**1303/1303**（D1=A 0 GLES dep / 0 link 失败）

---

## 下一推荐任务（基于 spec §11.2 + GLES 蓝图 plan §3 18 子任务清单）

> 🚀 **MVP-C 战略主线进行中** — G1.1 CMake VX_RENDERER flag 已实施完成 ✅ → **可立即进入 G1.2 GLESDisplay + Sdl2EGLDisplay**（D1=A 推迟点 / 引入 EGL/GLES dep）。

| 优先 | 候选任务 | MVP 档 | Level | plan ×0.6 |
|:-:|---|:-:|:-:|:-:|
| **1** | **G1.2 GLESDisplay + Sdl2EGLDisplay**（GLES context 创建 / 引入 EGL/GLES dep）| MVP-C 核心 | **L3** | ~2-3 h |
| 2 | G1.3 Sdl2GLWindowSurface（Surface 子类 + GL context 持有）| MVP-C 核心 | L3 | ~1.5-2.5 h |
| 3 | G1.4 GLESCanvas 骨架（shader 静态嵌入 + state stack）| MVP-C 核心 | L3 | ~2-3 h |
| 4 | R9 EventManager HitTest 改造（HUD pointer-events 真支持）| MVP-C | L2-3 | ~1.5-2 h |
| 5 | 资源加载策略蓝图（HTTP / file:// / data: URI 完整支持）| MVP-C 过渡 | L3 蓝图 + 实施 | ~5-10 h |
| 6 | G2 DRM/KMS 嵌入式后端蓝图 | MVP-C 核心 | L3-4 V2=a | ~10-20 h |
| 7 | DomBindings 节点动态创建删除 | MVP-C | L3 | ~3-5 h |
| 8 | CSS 高级特性 5 项 | MVP-C | 5 × L2-3 | ~10-20 h |
| 9 | 图像扩展 3 项（GIF / WebP / 异步加载）| MVP-C | 3 × L2 | ~6-12 h |
| 10 | 性能优化收口（含 #35 阶段 2 / R3+ 13 项）| MVP-C | 多 L2-3 | ~10-30 h |
| **元** | **下次工作流元任务批量落地**（累计 6 项 P1/P2 待处理事项 ≥ 4 阈值 ✅ / triple-evidence 候选 / 沿用 dual-evidence 范式）| 工作流 | L2 | ~30-60 min |

**当前焦点：** 继续 G1 OpenGL ES 实施阶段（建议从 G1.2 GLESDisplay 开始 / 详见 [docs/plans/2026-05-05-gles-renderer-blueprint.md](../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3.2 子任务规格化）

---

## 待处理事项 — 跨任务沉淀（按优先级）

### 留下次工作流元任务批量落地（P1 / 累计 1 项 / dual → triple-evidence 候选）

> TASK-05-04 已批量清零 14.5 项 P1+P2 累计沉淀 ✅。本段只列 TASK-05-04 + TASK-05-05 reflect 阶段新发现的 P1 项 — 等待下次工作流元任务批量清零（**累计 ≥ 4 项已达阈值 ✅** / 沿用工作流元任务 dual-evidence 范式）。

- **P1 #1（来自 TASK-20260505-04 reflection §5 #1 / 新发现）`writing-plans.mdc` 「附录：LOC 估算 — 隐性附加工作类型清单」段补「表格密度系数」子条** — plan 阶段对「commit body 范本表」+「触发条件矩阵」+「实证表」+「交叉引用清单」类结构化内容的行数 underestimate（单段 4 表格 ~30-40 行 / plan 仅按段长 base 估算未计表格行数 / TASK-05-04 P2.1 段实际 75 行 vs 估 30 行 = ×2.5 偏差）；建议加表格密度系数子条：散文段 ~30-40 行 / 单表格 ~5-15 行/表 / **多表段（≥ 4 表格）×2-2.5 base 行数**。**预估**：~10 min。

### 长期沉淀（P2 — 不强制 archive / 累计 5 项）

- **P2 #1（来自 TASK-20260505-04 reflection §5 #2 / 新发现）`git-workflow.mdc` 「commit body Source 溯源 + 实测数据格式」段补「实测数据采集协议」子条** — TASK-05-04 Phase B.6 commit body 写「+44 行」/ 实际 git 显示 +39 行 / -5 行偏差，根因 commit body 写在 add 之前 / 凭目测估算 / 未做 `git diff --cached --stat` 二次确认；建议加「实测数据采集协议」子条：commit 前必须运行 `git diff --cached --stat` 实测后再写 commit body 数据。**预估**：~10 min。
- **P2 #2（来自 TASK-20260505-04 reflection §8.2 / 新发现）systemPatterns 「lazy-attach C ABI 容错模式 quad-evidence」段加 TASK-05-04 头部 doc 落地标注** — TASK-05-04 P1.7-half 在 `veloxa/api/veloxa_api.h` 顶部 doc 段追加「lazy-attach contract」节统一引用 4 个 quad-evidence ABI（vx_view_set_pipeline_hooks / vx_view_attach_devtool / vx_devtool_get_console_output / vx_view_invalidate）；建议在 systemPatterns quad-evidence 段加「头部 doc 已落地」标注 + 引用 commit `4765224`。**预估**：~5 min。
- **P2 #3（来自 TASK-20260505-05 reflection §6 #10 / 新发现）`writing-plans.mdc`「ctest 守门脚本设计」段加「最小验证表面」原则 + stub probe 范式** — TASK-05 G1.1 cmake -P stub probe（`cmake_minimum_required + project(NONE) + include(<module>)` 3 行）实测 ~50ms/scenario / 全项目 configure ~30-60s/scenario / 加速比 ~600-1200×；建议 writing-plans 段加：测试 cmake module 应优先 stub probe 而非全项目 configure；stub probe 适用条件 = 待测 module 自治（无外部 module 依赖 / 仅校验 STATUS 文本）。**预估**：~15 min。
- **P2 #4（来自 TASK-20260505-05 reflection §6 #11 / 新发现）`writing-plans.mdc`「文件结构」段加 checklist「是否需要新 cmake/ 子目录抽 module？」** — TASK-05 G1.1 plan 阶段未规划 cmake/VxRenderer.cmake / build 阶段 REFACTOR 涌现单一真相源 / 顶层 +8 行（vs plan 估 +20）；建议 writing-plans 「文件结构」段加 checklist 项：「是否有可能在 build 阶段抽出 cmake/ 子目录共享 module？（信号：≥ 20 行 cmake 逻辑 + 多处 include 候选 + 测试需独立 include）」。**预估**：~10 min。
- **P2 #5（来自 TASK-20260505-05 reflection §6 #12 / 新发现）`writing-plans.mdc`「FetchContent 缓存命中策略」子条** — TASK-05 G1.1 build-gles 配置首次 FetchContent harfbuzz/freetype/sdl/libpng/zlib ~3.5 min 时间损耗 / plan §3 步骤 4 未提前规避；建议 writing-plans 加 plan 阶段 audit 项：「新 build 配置是否首次 FetchContent？是否可复用既有 build/_deps/ 缓存？」+ 推荐 `cmake -B build-gles --reuse-deps=build-default` （或类似 incremental reconfigure 技巧）。**预估**：~10 min。

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

- ✅ `feature/TASK-20260505-05-cmake-vx-renderer-flag` 分支已合并 + 删除（archive 阶段完成）
- ✅ `feature/TASK-20260505-04-workflow-meta-batch` 分支已合并 + 删除
- ✅ `feature/TASK-20260505-03-gles-renderer-blueprint` 分支已合并 + 删除
- ✅ `feature/TASK-20260505-01-dombindings-r2-closure` 分支已合并 + 删除
- ✅ `feature/TASK-20260504-01-mvp-scope-doc` 分支已合并 + 删除
- 早期 feature 分支（TASK-20260430-* / TASK-20260502-* / TASK-20260503-*）如未删除可批量清理

---

## 最近归档（速查，详细见 archive 文档）

- `archive-TASK-20260505-05.md`（**G1.1 CMake `VX_RENDERER` flag — GLES 蓝图实施首步 / MVP-C 战略主线第一个实施任务 Level 2，2026-05-05**）— **本批最新 ✅ / 9 个 systemPatterns 沉淀（reflection 史上单任务沉淀次高纪录）/ 跨决策协同度 100% 第 14 次连续命中（128/128 历史最高 streak）/ 实施忠实度新维度入库 / brainstorming P1.3 + writing-plans P1.6 双 dual-evidence 首次实战 / REFACTOR 涌现单一真相源 first-evidence（cmake/VxRenderer.cmake）/ ctest cmake -P stub probe 600-1200× 加速 first-evidence / P0 协议 quint-evidence + 适用性矩阵 5 类全覆盖 / plan ×0.6 ennea-evidence（实施类 Level 2 子档）/ LOC ×1.3-1.5 buffer dual-evidence / TDD 三阶完整 + 双 build 矩阵全 PASS（A 1303 + B 1110 + C gles 1303 + D invalid FATAL_ERROR）/ 反复模式 0/8 4 阶段全程抑制（累计 19 模式连续抑制 / 历史新高继续刷新）/ 12 改进建议（P0×0 + P1×9 全落实 + P2×3 累积）**
- `archive-TASK-20260505-04.md`（**工作流元任务批量落地 — 14.5 项 P1+P2 跨任务沉淀清零 Level 2-3，2026-05-05**）— 工作流元任务 dual-evidence 第 2 实证 ✅ / 跨决策协同度 100% 第 13 次连续命中（累计 121/121）/ 极致 dogfooding 三层闭环 first-evidence ✅ / plan ×0.6 oct-evidence + 双子档分化 / P0 协议 quad-evidence 已固化 / 5 个范式里程碑 + 5 个 systemPatterns 沉淀 + 6/8 改进建议已落实 / 反复模式 0/8 4 阶段全程抑制（累计 17 模式连续抑制）
- `archive-TASK-20260505-03.md`（**G1 OpenGL ES 硬件渲染后端蓝图 Level 4 V2=a，2026-05-05**）— **🚀 MVP-C 战略主线启动里程碑** — G1 OpenGL ES 硬件渲染后端蓝图 / 13 决策矩阵 + 18 实施子任务规格化 / 单 commit P0 协议首次完整实施 / 3 个范式升级同时落地（doudec-evidence + 极致极速区 0.02-0.05× + V2=a triple-evidence）/ 总投入 ~30-40 min vs plan ×0.6 ~17-25 h = **0.02-0.04× 极致极速区**
- `archive-TASK-20260505-02.md`（Performance Overlay 持续 invalidate 机制 Level 2，2026-05-05）— 🎉 **MVP-B 100% 闭环里程碑达成** / B-G4 / 5 个范式同时升级（plan ×0.6 sext + 跨决策协同度 dec + 反向探针 triple + 反复模式 #8 triple + lazy-attach quad）
- `archive-TASK-20260505-01.md`（**DomBindings R2 收口 — B-G1 children + B-G3 innerHTML setter + B-G2 audit Level 3，2026-05-05**）— MVP-B 完成度 90% → 95% / dogfood 视觉自动恢复链路三件齐 ✅ / 协议三件套里程碑（Phase 0 极速区 quint-evidence + 跨决策协同度 nona-evidence + 反向探针强度梯度三档 dual-evidence）/ 反复模式 #8 入库定型 / P0×4 reflect 全落实
- `archive-TASK-20260504-01.md`（MVP-scope 文档蓝图 Level 4 V2=a 完整变体，2026-05-04）— DevTool 4 件套主线收官标识 🎉 / 三档分级 MVP-A/B/C 体系建立 / 路线图按 MVP 档分层重写 / 核心目标 #1+#2 路径量化
- `archive-TASK-20260503-04.md`（DevTool Phase D · Console JS REPL Level 3 [安全相关]，2026-05-04）— DevTool 4 件套全部完整闭环 ✅ / spec §11.1 完整闭环 ✅ / T1 5 维度首次完整暴露 ✅ / plan ×0.6 0.07-0.10× 创历史新低
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
