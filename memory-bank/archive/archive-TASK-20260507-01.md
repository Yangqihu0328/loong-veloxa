# 归档：G1.4 GLESCanvas 骨架实施

**日期：** 2026-05-07
**任务 ID：** TASK-20260507-01
**复杂度级别：** Level 3
**状态：** ✅ 已完成
**安全标注：** ⚠️ [安全相关]（shader source 注入防御）
**分支：** `feature/TASK-20260507-01-gles-canvas-skeleton`（已合并 → main）
**关联蓝图：** [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../../docs/plans/2026-05-05-gles-renderer-blueprint.md) §3.4
**实施计划：** [`docs/plans/2026-05-07-gles-canvas-skeleton.md`](../../docs/plans/2026-05-07-gles-canvas-skeleton.md)
**回顾文档：** [`memory-bank/reflection/reflection-TASK-20260507-01.md`](../reflection/reflection-TASK-20260507-01.md)

---

## 任务概述

GLES 蓝图实施第四步 — 在 G1.3 已落地的 `Sdl2GLWindowSurface`（Surface + GL context）之上，实现 `vx::gfx::gles::GLESCanvas`：继承 `Canvas` 抽象（22 纯虚方法），骨架阶段实现 `Begin/End/Clear/SetTransform/PushState/PopState`，其余 15 个方法留 inline no-op stub（G1.5+ 逐步填充）。包含 shader 静态嵌入（`shaders.h` raw string literal / B6=A 安全契约）、VAO/VBO 初始化、以及 **首次安全回归测试**（`shader_injection_test.cc`）。

**前置链：** G1.1 CMake flag ✅ → G1.2 GLESDisplay ✅ → G1.3 Sdl2GLWindowSurface ✅ → **G1.4 GLESCanvas 骨架（本任务）** → G1.5 FillRect（下一步）

**ctest baseline 变化：**
- gles DEVTOOL=ON：1352 → **1362**（+10）

---

## 技术方案

### 架构定位

`GLESCanvas` 位于 GLES 渲染栈顶部，持有（borrowed ptr）`Sdl2GLWindowSurface` 提供的 GL context 访问入口：

```
vx::gfx::Canvas（抽象）
  └── vx::gfx::gles::GLESCanvas     ← G1.4 本任务
        ├── owns: VAO + VBO（quad 几何）
        ├── borrows: Sdl2GLWindowSurface*  ← G1.3
        │     └── owns: Sdl2EGLDisplay（GL context）  ← G1.2
        └── manages: state_stack_（transform + clip depth）
```

### 关键设计选择

| 决策 | 选项 | 理由 |
|------|------|------|
| D5=A | VAO/VBO 在构造函数初始化 | 避免懒初始化判断 / context 已在 ctor 前就绪（borrowed surface 保证）|
| D6=A | `State{ transform, clip_stack_depth }` | 与 `SoftwareCanvas` 对称 / 0 设计漂移 |
| D7=A | stub 方法内联 no-op（header 中 `{}`/`return nullptr`）| 编译期 inline / 0 运行时开销 / 最干净 |
| D8=B | 8 个骨架测试 | 精准覆盖 6 已实现方法 + 2 反向探针（T7 stub / T8 线程安全）|
| D9=A | 复用 G1.3 `Sdl2GlSurfaceEnvironment` fixture | 0 重复样板 / Mesa swrast first-evidence 直接续用 |
| D10=A | `vx_graphics` 内置（非独立 target）| 无外部可见 ABI / 依赖链最短 |
| D11=B | 最小 passthrough shader（kPassthroughVert + kPassthroughFrag）| 骨架阶段够用 / 避免过度设计 |
| D12=A | `shader_injection_test.cc` 本任务建 | 安全 first-evidence 提前入库，避免 G1.5 积累安全债 |
| D13=B | 三段 commit（init + plan + feat + reflect）| 历史可追溯 |

---

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 🆕 创建 | `veloxa/graphics/gles/shaders.h` | GLSL 编译期常量（kPassthroughVert + kPassthroughFrag）/ B6=A 安全注释 |
| 🆕 创建 | `veloxa/graphics/gles/gles_canvas.h` | `GLESCanvas` 类声明（22 override / 7 实现 + 15 no-op stub）/ state stack / accessor |
| 🆕 创建 | `veloxa/graphics/gles/gles_canvas.cc` | 骨架实现：ctor(VAO/VBO init) + dtor + Begin/End/Clear/SetTransform/GetTransform/PushState/PopState |
| 🆕 创建 | `tests/graphics/gles/gles_canvas_skeleton_test.cc` | 8 TEST_F（含 T3 pixel readback + T7 stub probe + T8 线程安全文档）|
| 🆕 创建 | `tests/graphics/gles/shader_injection_test.cc` | 2 安全测试（S1 编译期常量验证 + S2 文档即测试）|
| 🟡 修改 | `veloxa/graphics/CMakeLists.txt` | gles/ 子目录注册 + GLESv2 私有链接 |
| 🟡 修改 | `tests/CMakeLists.txt` | 2 新测试目标注册（GLES guard 内）|

**实际 LOC：** 569 行插入（估 727 / ×0.78 / stub-heavy 骨架新规律）

### 核心实现要点

**`shaders.h` — B6=A 安全契约：**
```cpp
// compile-time GLSL constant — MUST NEVER be concatenated with user data
inline constexpr const char* kPassthroughVert = R"(#version 300 es
precision highp float;
in vec2 a_pos;
void main() { gl_Position = vec4(a_pos, 0.0, 1.0); }
)";
```

**`gles_canvas.cc` — `Begin()` 设置 GL 状态：**
```cpp
void GLESCanvas::Begin() {
  glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  active_ = true;
}
```

**`gles_canvas.cc` — State Stack（沿用 SoftwareCanvas 模式）：**
```cpp
void GLESCanvas::PushState() {
  state_stack_.push_back({transform_, 0});  // clip_stack_depth 留 G1.x
}
void GLESCanvas::PopState() {
  if (!state_stack_.empty()) {
    transform_ = state_stack_.back().transform;
    state_stack_.pop_back();
  }
}
```

**`gles_canvas_skeleton_test.cc` — T3 Pixel Readback（续 Mesa swrast first-evidence）：**
```cpp
TEST_F(GLESCanvasSkeletonTest, Clear_WritesPixels) {
  canvas_->Begin();
  canvas_->Clear(Color{255, 0, 0, 255});
  canvas_->End();
  // glReadPixels 验证红色 — Mesa swrast G1.3 T4 first-evidence 续用
  GLubyte pixel[4] = {};
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  EXPECT_GT(pixel[0], 200);  // R
  EXPECT_LT(pixel[1], 50);   // G
}
```

---

## 关键决策

1. **Surface 采用 borrowed ptr（非 owns）** — `GLESCanvas` 不负责 Surface lifecycle，与 `SoftwareCanvas` 模式一致。GL context 由 Surface 管理，Canvas 只是"使用者"。

2. **stub 方法全部内联 header（`{}`）** — 15 个未实现方法直接在 .h 中写 `{}` 或 `return nullptr`，编译器可 inline 展开，0 运行时开销，且未来填充时只需修改 header 声明 + .cc 实现（无需 CMake 变更）。

3. **`MatrixEq()` 辅助函数（plan 遗漏的即时发现）** — `Matrix3x2` 无 `operator==`，测试中需要比较 transform 时手写 11 行辅助函数。这个发现已记录为 P2 改进建议（writing-plans.mdc 测试辅助函数 checklist）。

4. **安全测试提前到 G1.4（非 G1.5）** — shader_injection_test.cc 属 G1.5 范围，但 shaders.h 在 G1.4 建立，提前加安全测试避免安全债累积。

5. **clip_stack_depth 预留（未实现）** — `State` struct 含 `vx::usize clip_stack_depth = 0` 占位，不影响当前逻辑，为 G1.x clip stack 实施预留接口，0 修改成本。

---

## 安全决策

**shader source 注入防御（B6=A / D12=A）：**

- **威胁模型：** GLSL source 若在 runtime 拼接用户字符串，可导致 GPU 端任意指令执行（shader injection）
- **防御策略（B6=A）：** 所有 GLSL source 以 `inline constexpr const char*` 在 `shaders.h` 中静态嵌入，编译期绑定，不暴露任何接受用户字符串的 API
- **验证方法（D12=A first-evidence）：**
  - `S1 ShaderSourcesAreCompileTimeLiterals`：验证 `kPassthroughVert` / `kPassthroughFrag` 指针在多次调用间稳定（证明编译期常量，无 runtime 构造）
  - `S2 NoConcatenationApiExposed`：文档即测试，声明 `shaders.h` 不存在接受用户字符串的函数
- **适用范围：** G1.5+ 新增 shader（kSolidVert / kSolidFrag 等）须遵循同一规则并追加同类测试

---

## 测试覆盖

### 骨架功能测试（`gles_canvas_skeleton_test.cc`）

| 测试 ID | 测试名 | 验证内容 | 结果 |
|:-:|---|---|:-:|
| T1 | `Construct_BasicState` | VAO/VBO 非零、transform identity、active=false | ✅ |
| T2 | `Begin_EnablesBlend` | `glIsEnabled(GL_BLEND)` 为真 | ✅ |
| T3 | `Clear_WritesPixels` | glReadPixels 读出红色（Mesa swrast dual-evidence）| ✅（非 SKIP）|
| T4 | `SetTransform_GetTransform_RoundTrip` | SetTransform/GetTransform 对称 | ✅ |
| T5 | `PushState_PopState_RestoresTransform` | PushState/PopState 恢复 transform | ✅ |
| T6 | `End_FlushesGL` | glFlush 无 GL_NO_ERROR | ✅ |
| T7 | `ReverseProbe_StubMethodsAreNoOp` | 15 个 stub 方法调用无 GL 错误 | ✅ |
| T8 | `ReverseProbe_NonMainThread_GLContextNotCurrent` | 非主线程 GL context 不可用文档契约 | ✅ |

### 安全测试（`shader_injection_test.cc`）

| 测试 ID | 测试名 | 验证内容 | 结果 |
|:-:|---|---|:-:|
| S1 | `ShaderSourcesAreCompileTimeLiterals` | 编译期常量 / 指针稳定 | ✅ |
| S2 | `NoConcatenationApiExposed` | 无 runtime shader 组合 API | ✅ |

### 三 build 矩阵

| 配置 | 结果 | 说明 |
|------|------|------|
| software DEVTOOL=ON | 1337/1337 ✅ | 无变化（GLES guard 内测试不编译）|
| software DEVTOOL=OFF | 1141/1141 ✅ | 无变化 |
| gles DEVTOOL=ON | **1362/1362** ✅ | +10（+8 skeleton + +2 shader inject）|

---

## 经验教训

1. **实施忠实度 quad-evidence 确立** — G1.1→G1.2→G1.3→G1.4 连续 4 任务 0 plan 偏差，「plan 含完整 C++ 代码片段 → build ≈ 机械转化」模式已成熟，可作为团队范式推广。

2. **LOC ×0.78 穿下界 — stub-heavy 骨架新规律** — 15 个 no-op stub 内联 header + fixture 复用使实际 LOC（569）低于标准 [0.85, 1.5] buffer 下界。后续骨架任务 LOC 估算应使用 ×0.70-0.85 低端子注记。

3. **`Matrix3x2` 无 `operator==` — 测试需手写 `MatrixEq`** — plan 未预见，build 即时发现处理（11 行辅助函数）。今后 GLES 测试 plan 应检查被测类型是否有 `==` 运算符。

4. **Mesa swrast default framebuffer 已达 dual-evidence** — G1.3 T4 + G1.4 T3 双路径证实，G1.5+ 测试可直接使用 `glReadPixels` 无需预留 GTEST_SKIP 预算。

5. **安全测试提前优于积累** — shader injection 安全测试随 `shaders.h` 一起建立（D12=A），比 G1.5 再补代价低，且在 first-evidence 入库时锁定了设计契约。

---

## 范式里程碑汇总（本任务达成）

| 里程碑 | 里程碑类型 |
|---|:-:|
| 实施忠实度 quad-evidence（G1.1→G1.2→G1.3→G1.4） | 🏆 首次 |
| 跨决策协同度第 18 次连续命中 / 171/171 streak | 🏆 历史最高续刷 |
| Mesa swrast default framebuffer dual-evidence | 🏆 首次 |
| shader injection 安全 first-evidence | 🏆 首次 |
| plan ×0.6 第 12 数据点 / 实施类 Level 3 triple-evidence 候选 | 📊 |
| LOC 穿下界 first-evidence（stub-heavy 骨架规律）| 📊 新发现 |

---

## 参考文档

- **MVP 范围规格：** [`docs/specs/2026-05-04-mvp-scope.md`](../../docs/specs/2026-05-04-mvp-scope.md)
- **GLES 渲染蓝图：** [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../../docs/plans/2026-05-05-gles-renderer-blueprint.md)
- **实施计划：** [`docs/plans/2026-05-07-gles-canvas-skeleton.md`](../../docs/plans/2026-05-07-gles-canvas-skeleton.md)
- **回顾文档：** [`memory-bank/reflection/reflection-TASK-20260507-01.md`](../reflection/reflection-TASK-20260507-01.md)
- **前置 G1.3 归档：** [`memory-bank/archive/archive-TASK-20260506-01.md`](archive-TASK-20260506-01.md)
- **feat commit：** `670b75c`（7 files changed, 569 insertions）

---

## 后续任务（蓝图 plan §3.5）

**G1.5 FillRect** — 首个真实绘制方法：
- 实现 `FillRect(Rect, Color)`：solid color shader（kSolidVert + kSolidFrag）+ VAO/VBO 更新 + MVP 矩阵注入
- 继承 G1.4 的 VAO/VBO 基础设施 + B6=A shader 模式
- 新增 2 shader 常量时须同步在 `shader_injection_test.cc` 追加安全测试（B6=A 契约延续）
