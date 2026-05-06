# 回顾：G1.3 Sdl2GLWindowSurface 实施

**日期：** 2026-05-07
**任务 ID：** TASK-20260506-01
**复杂度级别：** Level 3
**分支：** `feature/TASK-20260506-01-sdl2-gl-window-surface`
**关联蓝图：** `docs/plans/2026-05-05-gles-renderer-blueprint.md` §3.3
**关联 plan：** `docs/plans/2026-05-06-sdl2-gl-window-surface.md`

---

## 1. 计划 vs 实际

| 维度 | 计划 | 实际 | 偏差原因 |
|------|------|------|---------|
| 复杂度 | Level 3 | Level 3 | ✅ 无偏差 |
| 文件数（新建） | 3 | 3 | ✅ 完全一致 |
| 文件数（修改） | 2 | 2 | ✅ 完全一致 |
| LOC（总插入行数） | ~490（buffer [415, 735]） | **424** | ✅ ×0.865（贴下界 / GLES 实现代码精简 — 无注释冗余） |
| 总时间 | plan ×0.6 ~110-180 min | **~44 min** | ×0.24 — 极端极速区；Plan 充分锁定实施路径 |
| Build 阶段（代码编写） | 90-135 min | **~17 min** | ×0.13 — 代码完全按 plan 机械转化，无设计临场决策 |
| 测试通过率 | 7/7（T4 可能 GTEST_SKIP） | **7/7 全绿（T4 非 SKIP！）** | 正向意外：Mesa swrast 默认帧缓冲真实写入确认 |
| 三 build 矩阵 | A+B+C 全 PASS | **A(1337/1337) + B(1141/1141) + C(1352/1352)** | ✅ gles 增量 +7 精确命中预期 |
| 决策遵循 | 13 决策全 lock → build 忠实实施 | **0 偏差** | 实施忠实度 **triple-evidence** 确立（G1.1 first + G1.2 dual + G1.3 triple）|
| commit 策略 | D12=B 三段 commit | 3 commits（init + plan + feat + MB） | ✅ 严格遵循 / chore 分离清洁 |

### 唯一偏差（plan vs 实现）

测试文件缺少 `#include <GLES3/gl3.h>`：plan 的「测试文件 includes 清单」只列了 `<SDL2/SDL.h>` + `<gtest/gtest.h>`，未明确列出 GLES3 头文件。原因：plan 编写时关注的是 `sdl2_gl_window_surface.h`（forward declare）路径，遗漏了测试直接调用 `glClearColor` / `glClear` 需要独立包含 GLES header 这一点。编译报错时立即识别并修复（1 行 include，无返工）。

---

## 2. 回顾检查清单

**代码变更类任务：**
- ✅ 计划精确度 — 文件清单与实际变更一致；预估 490 行 vs 实际 424 行（×0.865，紧贴 [0.85, 1.5] 双向 buffer 下界）
- ✅ TDD 执行情况 — 严格 RED（编译 fatal error 确认）→ GREEN（7/7 PASS）→ REFACTOR（lint clean）三阶完整
- N/A 子代理质量 — 本任务未使用子代理
- ✅ 测试隔离 — `SDL_GL_ResetAttributes()` ctor 内调用，无跨测试 GL attribute 泄露；每测独立 `Sdl2GLWindowSurface` 实例
- ✅ 提交粒度 — 4 commits 严格分层（init / plan / feat / MB finalize）
- ✅ 非默认路径 — T6 nullptr title / T7 零维度软失败 / T5 双次 Present 均验证

---

## 3. 做得好的

1. **Mesa swrast default framebuffer 行为实证（§0.4 不确定性已消除）** — T4 `SavePPM_WritesValidFile` 非 GTEST_SKIP，Mesa swrast 在 `SDL_VIDEODRIVER=offscreen` 下确实渲染到默认帧缓冲并可通过 `glReadPixels` 读取。此前 G1.2 plan 将此标注为「驱动严格性分层 first-evidence 沿用」中的不确定项，G1.3 实际运行给出了首个阳性实证。

2. **实施忠实度 triple-evidence 确立** — G1.3 build 阶段 0 计划偏差（仅缺失 1 行 test include，属 plan 遗漏而非 build 偏差）。G1.1 first + G1.2 dual + G1.3 triple，可作为 systemPatterns 固化的成熟范式（连续 3 任务 0 计划偏差）。

3. **析构顺序正确性** — `display_.reset()` 先于 `SDL_DestroyWindow` 在设计时就锁定，build 阶段无需临场判断，也未出现任何 use-after-free 或 GL context 泄漏。

4. **LOC 精准性** — 424 行 vs 估 490（×0.865）。在 [0.85, 1.5] buffer 范围内但贴近下界，说明：GLES 实现路径代码密度高（逻辑清晰、无注释冗余）；plan 的代码片段预先完整设计使得实现几乎是机械转化。

5. **三 build 矩阵 +7 精确命中** — gles baseline 1345 → 1352 = +7 精确匹配 plan 预期（7 TEST_F 对应 7 个 gtest_discover_tests 注册条目）。software 矩阵 baseline 无增量（gles guard 保护），符合设计。

6. **跨决策协同度 100% 第 17 次连续命中** — 13 决策全 lock，build 阶段 0 偏差，累计 streak: 145 + 13 = **158/158**。

---

## 4. 遇到的挑战

1. **测试文件缺 GLES 头文件（plan 遗漏 / 编译时发现）** — `tests/platform/sdl2_gl_window_surface_test.cc` 中 T4 直接调用 `glClearColor` / `glClear`，但 plan 的测试 includes 清单只列了 `<SDL2/SDL.h>` + `<gtest/gtest.h>`；`sdl2_gl_window_surface.h` 本身未 expose GLES3 头（forward declare 路径）。编译阶段立即报 `'glClearColor' was not declared`，1 行修复。  
   **根因分析**：Plan 编写时测试 include 检查点仅考虑了「从 header 传递」路径，未单独 audit「测试文件自身直接调用 GL 函数的情况」。G1.2 test 未直接调 GL 函数（只用 `IsValid()`/`MakeCurrent()` 等 display API），所以没有先例。

2. **矩阵 A 编译时间（~7 min）** — `build-sw-devtool` 首次 cmake configure（DEVTOOL=ON / FetchContent QuickJS 等）耗时 ~5 min configure + ~2 min build。这不是 bug，是预期的 cold configure 成本。

---

## 5. 经验教训

### 5.1 新 pattern：GLES test 直接调用 GL 函数 → 需独立 include GLES3 header

**模式**：测试文件如果直接调用 `gl*()` 函数（glClear / glClearColor / glReadPixels 等），必须在测试文件顶部显式包含 `<GLES3/gl3.h>`，不能依赖被测类 header 的间接传递（GLES-backed 类通常使用 forward declare 或在 `.cc` 中包含 GLES header）。

**规则**：Plan 中「测试文件 includes 审查」应显式 checklist 一项：「测试文件是否直接调用 `gl*()` 函数？是 → includes 清单必须含 `<GLES3/gl3.h>`」。

**首发来源**：G1.3 T4 编译报错（1 次命中 / first-evidence）。

### 5.2 确认 pattern：Mesa swrast default framebuffer 真实写入

**实证**：Mesa swrast + `SDL_VIDEODRIVER=offscreen` + `SDL_GL_CreateContext(SDL_WINDOW_OPENGL)` 路径下，`glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ...)` 可以读取 `glClearColor` + `glClear` 之后的真实颜色值（T4 first pixel R≈255 / G≈0 / B≈0 验证红色清屏）。

**重要性**：此行为此前在 G1.2 plan 中标记为「不确定 / 留 G1.3 T4 RED 探针」。G1.3 给出了首个阳性实证（**first-evidence**），可以下调后续 G1.4+ 测试对此路径的不确定性估计。G1.4 GLESCanvas 测试可以更安全地使用 `glReadPixels` 做 pixel-accurate 测试。

**条件限制**：仅在 Mesa swrast（软件光栅化）路径下验证；真实 GPU driver（i915、amdgpu、freedreno）行为需实机验证；DRM/KMS 路径（G2）另议。

### 5.3 实施忠实度 triple-evidence 成熟

G1.1 first + G1.2 dual + G1.3 triple — 连续 3 个 GLES 实施子任务均做到 build 阶段 0 计划偏差（不含 plan 遗漏的 1 行 include fix，属 plan 质量问题而非 build 忠实度问题）。可作为 systemPatterns 成熟范式固化：**当 plan 包含完整 C++ 代码片段时，实施忠实度接近 100%**。

---

## 6. 改进建议

| # | 建议 | 优先级 | 落实方式 | 目标 |
|---|------|--------|---------|------|
| 1 | Plan 测试 includes checklist 加「GLES 函数直调 → 需 `<GLES3/gl3.h>`」子条 | **P1** | 更新 `writing-plans.mdc` 「Phase A 测试设计」段；或 plan template 「测试 includes 审查」checklist | `writing-plans.mdc` |
| 2 | systemPatterns 新段：「Mesa swrast default framebuffer 真实写入 first-evidence」 | **P1** | 直接在 reflect 阶段落地 `systemPatterns.md` | `systemPatterns.md` |
| 3 | systemPatterns 更新：实施忠实度 dual-evidence → triple-evidence | **P1** | 直接在 reflect 阶段落地 `systemPatterns.md` | `systemPatterns.md` |
| 4 | systemPatterns 更新：跨决策协同度 第 17 次连续命中 / 158/158 | **P1** | 直接在 reflect 阶段落地 `systemPatterns.md` | `systemPatterns.md` |
| 5 | Plan ×0.6 系数 dec-evidence 第 12 数据点（G1.3 build = 0.13×） | **P1** | 直接在 reflect 阶段落地 `systemPatterns.md` | `systemPatterns.md` |
| 6 | techContext 加 G1.3 落地节点 + Mesa swrast 帧缓冲行为实证 | **P1** | 直接在 reflect 阶段落地 `techContext.md` | `techContext.md` |
| 7 | `writing-plans.mdc` P1 #1（来自 activeContext 待处理事项）：表格密度系数子条 | P2（待批量元任务） | 累积到 activeContext 待处理事项 P1 #1 | `writing-plans.mdc` |

优先级说明：
- **P1 → reflect 阶段直接落地**（6 项 / 本阶段完成）
- **P2 → 累积下次工作流元任务**（P2 #7 已在 activeContext 待处理事项 P1 #1 位置，保持）

---

## 7. 安全评估

本任务不涉及安全变更：
- 0 新公开 ABI / 0 用户输入路径
- `SavePPM` 路径参数来自测试 / 内部调用（非外部输入）
- 无网络、无认证、无敏感数据处理

| 维度 | 状态 | 备注 |
|------|------|------|
| 输入验证 | N/A | |
| 认证/授权 | N/A | |
| 数据保护 | N/A | |
| 依赖审计 | N/A | 0 新依赖（EGL/GLES3 已在 G1.2 引入） |
| 错误信息脱敏 | N/A | |

---

## 8. 反复模式识别

| 已知模式 | 本次是否重复？ | 备注 |
|---------|-------------|------|
| 计划文件清单与实际变更不一致 | ❌ 未重复 | 文件清单 100% 一致 |
| 子代理产出需大量返工 | N/A | 未使用子代理 |
| 前置依赖/环境/API 能力未验证 | ❌ 未重复 | Phase 0 §0.1-§0.5 全 ✅ |
| 非默认路径遗漏验证 | ❌ 未重复 | T6 + T7 反向探针完整 |
| 测试隔离问题 | ❌ 未重复 | `SDL_GL_ResetAttributes()` 隔离 |
| 提交粒度偏离计划 | ❌ 未重复 | D12=B 三段 commit 严格遵循 |
| TDD 严格度与场景不匹配 | ❌ 未重复 | RED→GREEN→REFACTOR 完整 |
| **新增：GLES test header 遗漏** | ✅ **首次出现**（first-evidence） | 建议作为 P1 加入 plan checklist |

反复模式总计：**0/7 已知模式命中**（累计 20+ 模式连续抑制 / 历史新高续刷）。1 个新模式（GLES test header 遗漏）发现并记录。

---

## 9. 范式里程碑

| 里程碑 | 状态 | 说明 |
|------|------|------|
| 实施忠实度 triple-evidence | ✅ **首次确立** | G1.1 first + G1.2 dual + G1.3 triple / 3 连续 GLES 子任务 0 plan 偏差 |
| 跨决策协同度 第 17 次连续命中 | ✅ | 158/158（145 → 158）/ 历史最高 streak 续刷 |
| Mesa swrast 帧缓冲行为 first-evidence | ✅ **首次实证** | T4 非 SKIP / glReadPixels 读取真实颜色值 |
| Plan ×0.6 dec-evidence 第 12 数据点 | ✅ | G1.3 build 0.13× / 极端极速区 |
| LOC ×0.865（贴下界 [0.85, 1.5]） | ✅ | GLES 实现代码精简特征首次验证 |

---

## 10. 总结

TASK-20260506-01 G1.3 `Sdl2GLWindowSurface` 以 **~44 min** 总时间（VAN+Plan+Build）完成，为本 GLES 蓝图实施序列（G1.1 / G1.2 / **G1.3**）中执行最快的一次（G1.3 build 0.13× vs G1.2 build ~0.30×）。

关键成果：
- **Surface + EGL 完整闭环首次实现**：`Sdl2GLWindowSurface` 成为首个同时持有 SDL_Window（owns）+ Sdl2EGLDisplay（owns）+ GL context lifecycle 的 Surface 子类，为 G1.4 `GLESCanvas`（首个真实可绘制画布）直接奠基。
- **Mesa swrast 帧缓冲实证**：T4 SavePPM 非 SKIP，消除 GLES headless 测试路径的最大不确定性。
- **GLES test header 新 pattern**：plan checklist 应显式包含「测试直调 `gl*()` 函数 → 需 `<GLES3/gl3.h>`」条目。
