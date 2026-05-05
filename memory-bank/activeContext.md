# 活跃上下文

## 当前阶段

**回顾完成** — TASK-20260505-06 G1.2 `GLESDisplay` 抽象 + `Sdl2EGLDisplay` 实施（Level 3 / GLES 蓝图实施第二步 / **MVP-C 战略主线第二个实施任务** / G1.1 D1=A 推迟点正式落地）VAN ✅ + Plan ✅ + Build ✅ + Reflect ✅ → 待 `/archive`。

**Reflect 阶段产出（2026-05-05 ~21:10 / 实测 ~15-20 min / 标准区 ~1.0× 子档 / 预估命中 ✅）：**

- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260505-06.md`（10 段全覆盖 / 7 P1 沉淀直接落地 + 5 P2 改进建议 / 度量数据汇总详尽 / 自评 4.7/5）
- **systemPatterns 7 段更新（P1 直接落地）：**
  1. 跨决策协同度 100% 第 15 次连续命中 + 实施忠实度 dual-evidence（streak 128 → 136）
  2. plan ×0.6 实测系数 dec-evidence（ennea → dec / 实施类 Level 3 子档新增）
  3. brainstorming P1.3 主动 push-back 模式 triple-evidence（dual → triple）
  4. writing-plans P1.6 spec vs code audit triple-evidence（dual → triple）
  5. P0 协议 sext-evidence（quint → sext / 适用性矩阵 6 类全覆盖 ✅）
  6. **D3=B eager extension cache 范式 first-evidence**（新段 / 多 ext 查询通用）
  7. **D4=C inline test 反向探针 + 驱动严格性分层 first-evidence**（新段 / Mesa headless 经验）
  8. LOC 双向 ±25% buffer 子档（单向 ×1.3-1.5 → 双向 [0.85, 1.5] / 模式参数细化）
- **writing-plans P1.5 段升级：** quint → sext-evidence 实证表 + 适用性矩阵 6 类表
- **5 P2 改进建议沉淀到 activeContext 待处理事项**（writing-plans 双向 ±25% buffer / ctest baseline 比对 noise / Mesa headless 严格性 / techContext OpenGL ES dep / 双 100% 流程闭环）
- **反复模式 0/8 reflect 阶段保持**（累计 19 模式连续抑制 / 历史新高继续刷新 ✅）

**Reflect 关键发现：**
1. **brainstorming P1.3 + writing-plans P1.6 双 triple-evidence 续延** — TASK-04 first + TASK-05 dual + TASK-06 triple / 3 plan §3.2 偏差 100% 校正 / 节省 ~60-90 min build 返工
2. **跨决策协同度 + 实施忠实度双 100% first → dual-evidence** — 8/8 D 决策 0 偏差实施 / 累计 128 → 136 streak 续刷
3. **P0 协议适用性矩阵 6 类全覆盖** — V2=a 蓝图 + 工作流元 + 实施类 Level 2 + Level 1 + Level 4 多 Phase + **实施类 Level 3** ✅
4. **D4=C 反向探针「驱动严格性分层」** — Mesa headless silent fallback → 驱动无关层（nullptr / 非法 enum）必选 / 驱动严格层 P3 优化
5. **LOC buffer 双向 ±25% 反向校准** — TASK-05 ×1.4 偏高 + TASK-06 ×0.95 偏低 dual-evidence / 单向 → 双向 ±25%

**下一步：** `/archive` — 进入归档阶段，整合 7 P1 P0 协议 sext-evidence 数据 + 5 P2 累积。

---

**Build 阶段产出（2026-05-05 ~20:55 / 实测 ~25-35 min / 极速区 ~0.4-0.6× 子档）：**

**Build 阶段产出（2026-05-05 ~20:55 / 实测 ~25-35 min / 极速区 ~0.4-0.6× 子档）：**

- **3 commits 总计 +495 行 / 6 文件改动 / 1 ctest baseline +8（gles only）：**
  - `4b095c4` feat(platform): add GLESDisplay abstract + Sdl2EGLDisplay impl — 主交付 +495 行 / 6 文件
  - `39d2981` chore(plan): land plan + memory bank（P0 sext-evidence 候选）
  - `545fa1f` chore(workflow): initialize VAN

- **TDD 三阶完整 ✅：**
  - **RED**：`sdl2_egl_display.h: No such file or directory` 编译失败（gles_display.h / sdl2_egl_display.h 不存在）✅
  - **GREEN**：8/8 TEST_F PASS（~150ms 总时长 / Mesa swrast / SDL_VIDEODRIVER=offscreen）✅
  - **REFACTOR**：T8 反向探针从 `SDL_GL_CONTEXT_MAJOR_VERSION=99`（Mesa silent fallback / 不可靠）改为 `nullptr` window（驱动无关 / 100% 可重现）✅

- **ctest 三 build 矩阵全 PASS ✅：**
  - Matrix A (DEVTOOL=ON / software default): **1303 → 1303** ✅（不退化 / sdl2_egl_display_test 仅 gles 编译）
  - Matrix B (DEVTOOL=OFF / software): **1110 → 1110** ✅（不退化）
  - Matrix C (DEVTOOL=ON / gles): **1345 PASS** ✅（含 +8 sdl2_egl_display_test Test #1191-1198）

- **LOC 实测 ×0.95**（plan 520 → 实际 495 / **反向偏低 / 命中 P2.2「LOC ×1.3-1.5 buffer」反例 → reflect 候选：单向 → 双向 ±25% buffer 子档**）：
  - gles_display.h: 70 行 (plan 80 / ×0.875)
  - sdl2_egl_display.h: 65 行 (plan 50 / ×1.30)
  - sdl2_egl_display.cc: 146 行 (plan 150 / ×0.97)
  - sdl2_egl_display_test.cc: 189 行 (plan 220 / ×0.86)
  - sdl2/CMakeLists.txt: +13 (plan +10 / ×1.30)
  - tests/CMakeLists.txt: +12 (plan +10 / ×1.20)

- **0 lint errors**（6 改动文件 ReadLints 全 ✅）

- **plan §3.2 偏差校正 3/3 实施成功 ✅：**
  - 偏差 #1：sdl2/CMakeLists.txt 修改（非顶层 platform/）✅
  - 偏差 #2：tests/platform/sdl2_egl_display_test.cc 扁平路径 ✅
  - 偏差 #3：::testing::Environment + SDL_VIDEODRIVER=offscreen ✅

- **8/8 D 决策 0 偏差实施 ✅**（实施忠实度新维度续延 / G1.1 first-evidence + G1.2 dual-evidence ✅）

- **build 中发现意外 1 处 / 反向探针调整 ✅：**
  - 发现：Mesa swrast 不严格 enforce SDL_GL_CONTEXT_MAJOR_VERSION（silent fallback）/ 原 T8 设计不可靠
  - 调整：T8 改为 `Sdl2EGLDisplay(nullptr).Initialize()` → expect kInvalidArgument（驱动无关）
  - 这是 D4=C「inline test 反向探针范式」的健壮性细化 — reflect 阶段 P2 候选「Mesa headless 驱动严格性 vs 真实 GPU 行为差异」

- **反复模式 0/8 抑制延续**（VAN + Plan + Build 三阶段全程保持 / 累计 19 模式连续抑制 / 历史新高继续刷新）

**新 ctest baseline 生效（main 分支待合并 / build 阶段 commit `4b095c4`）：**
- DEVTOOL=ON / VX_RENDERER=software（default）：**1303/1303**（不退化）
- DEVTOOL=OFF / VX_RENDERER=software（default）：**1110/1110**（不退化）
- DEVTOOL=ON / VX_RENDERER=gles：**1345/1345**（含 +8 sdl2_egl_display_test）

**当前任务：** TASK-20260505-06 — `G1.2 GLESDisplay + Sdl2EGLDisplay` / Level 3 / 分支 `feature/TASK-20260505-06-gles-display-sdl2-egl`

**Plan 阶段产出（2026-05-05 ~20:25 / 实测 ~25-35 min / 标准区 ~1.0× 子档）：**

- **8/8 D 决策 1 次 AskQuestion all_recommended 锁定 ✅** — **跨决策协同度 100% 第 15 次连续命中** / dec → endec → doudec → 13 → 14 → **15** / 累计 128 → **136/136 历史最高 streak 续刷** ✅
  - **D1=A** SDL_VIDEODRIVER=offscreen testing fixture
  - **D2=B** 含 RestoreContext 路径覆盖
  - **D3=B** Initialize 后 eager std::unordered_set extension cache
  - **D4=C** inline test SDL_GL_SetAttribute(MAJOR_VERSION=99) 反向探针
  - **D5=A** plan §0.4 3 偏差详细校正
  - **D6=A** sdl2/CMakeLists.txt 局部 EGL/GLES dep
  - **D7=A** 单 feat commit
  - **D8=A** plan + MB 单 commit 自吃狗粮（quint → sext-evidence 第 6 数据点候选 / 实施类 Level 3 首次实证）

- **plan §3.2 偏差校正（brainstorming P1.3 主动 push-back triple-evidence 候选 ✅）：**
  - 偏差 #1：plan 改顶层 platform/CMakeLists.txt → 校正为 sdl2/CMakeLists.txt（GLESDisplay.h header-only）
  - 偏差 #2：plan tests/platform/sdl2/ 子目录 → 校正为扁平 tests/platform/sdl2_egl_display_test.cc
  - 偏差 #3：plan 未明示 headless CI fixture → 校正为 ::testing::Environment with SDL_VIDEODRIVER=offscreen

- **主交付物（plan + Memory Bank ×3 / D8=A 自吃狗粮单 commit）：**
  - `docs/plans/2026-05-05-gles-display-sdl2-egl.md`（~600 行 / 11 段全覆盖 / 含 D1-D8 决策矩阵 + plan §0.4 详细校正 + 8 TEST_F 步骤 1-5 完整代码片段 + 三 build 矩阵 ctest + 7 反思候选）
  - `memory-bank/activeContext.md`（更新 Plan 阶段产出）
  - `memory-bank/tasks.md`（加 Plan 阶段决策矩阵 + 估时）
  - `memory-bank/progress.md`（加 Plan 阶段时间线）

- **estimaate（plan ×0.6）：** ~110-180 min 总线（plan ~25-40 + build ~50-90 + reflect ~15-20 + archive ~10-15）/ vs GLES 蓝图 plan §3.2 估时 ~240-360 min = **预期总线极速区 0.46-0.50×**

- **反复模式预审 0/8 命中**（VAN + Plan 两阶段全程保持 / 累计 19 模式连续抑制 / 历史新高继续刷新）

- **不进入 `/creative`：** Level 3 实施类 / 8 决策已 lock / 设计 spec §3.3.2 已规格化（11 virtual methods + ctor/dtor + private members）/ 0 创意阶段需求

- **沉淀候选（reflect 阶段处理 / 7 项 P1）：**
  - systemPatterns 升级「跨决策协同度 100% 第 15 次连续命中」（136/136 历史最高 streak）
  - systemPatterns 升级「plan ×0.6 dec-evidence 第 10 数据点」（实施类 Level 3 子档新增）
  - systemPatterns 升级「brainstorming P1.3 主动 push-back triple-evidence」
  - systemPatterns 升级「writing-plans P1.6 spec vs code audit triple-evidence」
  - systemPatterns 升级「P0 协议 sext-evidence 第 6 数据点」（quint → sext / 实施类 Level 3 首次实证）
  - systemPatterns 新段「D3=B eager extension cache 范式 first-evidence」
  - systemPatterns 新段「D4=C inline test 反向探针范式 first-evidence」

**当前任务：** TASK-20260505-06 / Level 3 / 分支 `feature/TASK-20260505-06-gles-display-sdl2-egl`（基于 main `ee2569d` ✅ 创建 / G1.1 已合并）

**下一步：** `/build` — 进入构建阶段，按 plan §3 步骤 1-6 实施（TDD RED → 抽象 → impl GREEN → ctest 注册 → 三 build 矩阵 → D7=A 单 feat commit）。

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

> TASK-05-04 已批量清零 14.5 项 P1+P2 累计沉淀 ✅。本段只列 TASK-05-04 + TASK-05-05 + TASK-05-06 reflect 阶段新发现且未在 reflect 阶段直接 P1 落地的项 — 等待下次工作流元任务批量清零（**累计 P1×1 + P2×10 = 11 项 ≥ 4 阈值 ✅✅** / 沿用工作流元任务 dual-evidence 范式 / **triple-evidence 候选**）。
>
> **TASK-05-06 reflect 阶段 7/7 P1 已直接落地 ✅**（systemPatterns 7 段更新 + writing-plans P1.5 段升级）/ 不进入此累积清单。

- **P1 #1（来自 TASK-20260505-04 reflection §5 #1 / 新发现）`writing-plans.mdc` 「附录：LOC 估算 — 隐性附加工作类型清单」段补「表格密度系数」子条** — plan 阶段对「commit body 范本表」+「触发条件矩阵」+「实证表」+「交叉引用清单」类结构化内容的行数 underestimate（单段 4 表格 ~30-40 行 / plan 仅按段长 base 估算未计表格行数 / TASK-05-04 P2.1 段实际 75 行 vs 估 30 行 = ×2.5 偏差）；建议加表格密度系数子条：散文段 ~30-40 行 / 单表格 ~5-15 行/表 / **多表段（≥ 4 表格）×2-2.5 base 行数**。**预估**：~10 min。

### 长期沉淀（P2 — 不强制 archive / 累计 10 项）

- **P2 #1（来自 TASK-20260505-04 reflection §5 #2 / 新发现）`git-workflow.mdc` 「commit body Source 溯源 + 实测数据格式」段补「实测数据采集协议」子条** — TASK-05-04 Phase B.6 commit body 写「+44 行」/ 实际 git 显示 +39 行 / -5 行偏差，根因 commit body 写在 add 之前 / 凭目测估算 / 未做 `git diff --cached --stat` 二次确认；建议加「实测数据采集协议」子条：commit 前必须运行 `git diff --cached --stat` 实测后再写 commit body 数据。**预估**：~10 min。
- **P2 #2（来自 TASK-20260505-04 reflection §8.2 / 新发现）systemPatterns 「lazy-attach C ABI 容错模式 quad-evidence」段加 TASK-05-04 头部 doc 落地标注** — TASK-05-04 P1.7-half 在 `veloxa/api/veloxa_api.h` 顶部 doc 段追加「lazy-attach contract」节统一引用 4 个 quad-evidence ABI（vx_view_set_pipeline_hooks / vx_view_attach_devtool / vx_devtool_get_console_output / vx_view_invalidate）；建议在 systemPatterns quad-evidence 段加「头部 doc 已落地」标注 + 引用 commit `4765224`。**预估**：~5 min。
- **P2 #3（来自 TASK-20260505-05 reflection §6 #10 / 新发现）`writing-plans.mdc`「ctest 守门脚本设计」段加「最小验证表面」原则 + stub probe 范式** — TASK-05 G1.1 cmake -P stub probe（`cmake_minimum_required + project(NONE) + include(<module>)` 3 行）实测 ~50ms/scenario / 全项目 configure ~30-60s/scenario / 加速比 ~600-1200×；建议 writing-plans 段加：测试 cmake module 应优先 stub probe 而非全项目 configure；stub probe 适用条件 = 待测 module 自治（无外部 module 依赖 / 仅校验 STATUS 文本）。**预估**：~15 min。
- **P2 #4（来自 TASK-20260505-05 reflection §6 #11 / 新发现）`writing-plans.mdc`「文件结构」段加 checklist「是否需要新 cmake/ 子目录抽 module？」** — TASK-05 G1.1 plan 阶段未规划 cmake/VxRenderer.cmake / build 阶段 REFACTOR 涌现单一真相源 / 顶层 +8 行（vs plan 估 +20）；建议 writing-plans 「文件结构」段加 checklist 项：「是否有可能在 build 阶段抽出 cmake/ 子目录共享 module？（信号：≥ 20 行 cmake 逻辑 + 多处 include 候选 + 测试需独立 include）」。**预估**：~10 min。
- **P2 #5（来自 TASK-20260505-05 reflection §6 #12 / 新发现）`writing-plans.mdc`「FetchContent 缓存命中策略」子条** — TASK-05 G1.1 build-gles 配置首次 FetchContent harfbuzz/freetype/sdl/libpng/zlib ~3.5 min 时间损耗 / plan §3 步骤 4 未提前规避；建议 writing-plans 加 plan 阶段 audit 项：「新 build 配置是否首次 FetchContent？是否可复用既有 build/_deps/ 缓存？」+ 推荐 `cmake -B build-gles --reuse-deps=build-default` （或类似 incremental reconfigure 技巧）。**预估**：~10 min。
- **P2 #6（来自 TASK-20260505-06 reflection §6.3 #1 / 新发现）`writing-plans.mdc` P2.2「LOC ×1.3-1.5 buffer」段更新为「双向 ±25% buffer 子档」** — TASK-05 G1.1 ×1.40（偏高 / 命中上限）+ TASK-06 G1.2 ×0.95（偏低 / 接近下限）/ 双向偏差 dual-evidence；建议 P2.2 段加：单向 ×1.3-1.5 buffer → 双向 [0.85, 1.5] buffer / 偏低根因（注释精简 / 测试 GTEST_SKIP）/ 偏高根因（drift guard / REFACTOR 涌现 / 多表格）/ 偏差范围 [-15%, +50%] / 平均 ×1.20。**预估**：~10 min。
- **P2 #7（来自 TASK-20260505-06 reflection §6.3 #2 / 新发现）`writing-plans.mdc`「ctest baseline 比对」段加澄清** — TASK-06 build 阶段 ctest 总数从 1303 → 1345（+42）vs plan 估 +8 / +34 是 build-gles 首次配置 gtest_discover_tests 的 incremental 注册 noise；建议加澄清：ctest baseline 比对仅看「本任务测试增量」（明确指定 Test 编号区间）/ 不依赖数据库总量 / gtest_discover_tests 增量注册可能带来 noise / 通过指定测试名前缀（如 sdl2_egl_display_test）+ Test 编号确认精确增量。**预估**：~10 min。
- **P2 #8（来自 TASK-20260505-06 reflection §6.3 #3 / 新发现）systemPatterns 新段「Mesa headless 驱动严格性差异 / 反向探针分层」** — Mesa swrast silent fallback for SDL_GL_CONTEXT_MAJOR_VERSION=99 → 反向探针应分层（驱动无关层 / 驱动严格层）/ 驱动无关层（nullptr / 非法 enum / 不变量违反）必选 / 驱动严格层（请求非法版本 / 非法 attribute）作为 P3 优化。已在「D4=C inline test 反向探针 + 驱动严格性分层 first-evidence」段 P1 落地 ✅；P2 候选 = 沉淀到 techContext 「Mesa headless 测试环境」段（含 SDL_VIDEODRIVER=offscreen / EGL_PLATFORM=surfaceless / driver behavior 差异 checklist）。**预估**：~15 min。
- **P2 #9（来自 TASK-20260505-06 reflection §6.3 #4 / 新发现）techContext 加段「OpenGL ES + EGL 平台 dep」** — pkg-config egl 1.5（Mesa 24.x）+ glesv2 3.2（Mesa 24.x）+ Mesa headless drivers（swrast / kms_swrast）+ SDL_VIDEODRIVER=offscreen 兼容性 / 嵌入式 GL test setup checklist；建议 techContext 加 GLES + EGL 段（dep 版本 + headless 测试 setup + 平台兼容性）。**预估**：~20 min。
- **P2 #10（来自 TASK-20260505-06 reflection §6.3 #5 / 新发现）systemPatterns 新段「实施忠实度 + 跨决策协同度 双 100% 流程闭环」** — TASK-05 G1.1 first + TASK-06 G1.2 dual / 双 100% first → dual-evidence 已固化 / 待 G1.3 G1.4 续延 triple-evidence；当前已在「跨决策协同度 100% 第 15 次连续命中 + 实施忠实度 dual-evidence」段 P1 落地 ✅；P2 候选 = 进一步沉淀「双 100% 流程闭环」独立段（前置条件 + 模式参数 + 适用范围）。**预估**：~10 min。

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
