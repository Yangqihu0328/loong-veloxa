# GLES `GLESCanvas` Clip（PushClipRect/PushClipPath/PopClip via glScissor）设计规格

**任务 ID：** TASK-20260606-01
**日期：** 2026-06-06
**复杂度：** Level 3
**状态：** 设计锁定（D1-D6 = a）
**蓝图定位：** GLES 硬件渲染后端蓝图 G1.10（Clip 部分 / [`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../plans/2026-05-05-gles-renderer-blueprint.md) §3.10）

## 1. 目的

将 `GLESCanvas` 三个 clip stub（`PushClipRect`/`PushClipPath`/`PopClip`，`gles_canvas.h:99-101` 当前内联 `{}`）实现为基于 `glScissor` 的矩形裁剪栈，使后续绘制（FillRect/FillPath/DrawText/DrawImage 等）受当前 clip 区域约束。镜像 software 后端 `clip_stack_` + `CurrentClip()`（`software_canvas.cc:322-407`）的交集语义，保证跨后端一致。

## 2. 不做（Non-goals）

- **PushLayer/PopLayer（FBO）** — 蓝图 §3.10 原与 Clip 合并，本任务拆出后续独立任务（仍保留 stub）。
- **真路径裁剪** — `PushClipPath` 仅用 `path.Bounds()` AABB 近似（镜像 software / stencil 真路径裁剪记 G2 技术债）。
- **旋转 clip** — `glScissor` 仅支持 axis-aligned 设备空间矩形；D2 镜像 software 语义（clip 不应用 `transform_`），旋转下行为与 software 一致。
- **dirty rect + glScissor 集成** — 属 G1.11（独立任务）。

## 3. 接口与实现设计

### 3.1 决策矩阵（D1-D6 全锁 = a）

| # | 决策 | 锁定值 |
|:-:|---|---|
| D1 | Clip 存储与交集语义 | **a** 镜像 software：`Vector<Rect> clip_stack_`，push 存 `CurrentClip().Intersect(rect)`，栈顶=当前有效 clip（设备空间像素），栈空=全 viewport |
| D2 | clip rect 坐标空间 | **a** 镜像 software：clip rect 直接视为**设备空间**，**不**应用 `transform_`（与 software `clip_stack_` 完全一致 / glScissor 直接消费 / 跨后端一致） |
| D3 | glScissor 应用时机 + Y 翻转 + Begin 复位 | **a** Push/Pop 立即 `glScissor(x, height-(y+h), w, h)` + `glEnable/glDisable(GL_SCISSOR_TEST)`（栈空→disable）；`Begin()` 入口 `glDisable(GL_SCISSOR_TEST)` + 清空 `clip_stack_`（帧隔离 / 防跨测试泄露） |
| D4 | PushClipPath 近似 | **a** 镜像 software：`PushClipPath(path) → PushClipRect(path.Bounds())`（AABB 近似 / 真路径裁剪记技术债 G2） |
| D5 | PushState/PopState 联动 | **a** 镜像 software：`PushState` 存 `clip_stack_.size()`，`PopState` 弹栈到该深度并重新 `ApplyScissor()`（栈顶 or disable） |
| D6 | 测试矩阵 + 流程 | **a** 单轮 TDD（A RED→B GREEN→C 三矩阵 finalize）/ ~7-8 像素测 / 承接 P1#1 解析采样 + P1#2 双通道 / 跳过独立 creative |

### 3.2 头文件变更（`gles_canvas.h`）

```cpp
// 3 clip stub（L99-101）改为 out-of-line 声明：
void PushClipRect(const Rect&) override;
void PushClipPath(const Path&) override;
void PopClip() override;

// private 新增成员：
vx::Vector<Rect> clip_stack_;   // 设备空间像素 rect 交集栈（D1）

// private 新增 helper：
void ApplyScissor();            // 据 clip_stack_ 栈顶设 glScissor + enable/disable（D3）
Rect CurrentClipDevice() const; // 栈顶 or 全 viewport（镜像 software CurrentClip）
```

> `State.clip_stack_depth`（L115）字段已预留，本任务激活使用。

### 3.3 `ApplyScissor()` 核心（D3 Y 翻转）

```cpp
void GLESCanvas::ApplyScissor() {
  if (clip_stack_.empty()) {
    glDisable(GL_SCISSOR_TEST);
    return;
  }
  const Rect& c = clip_stack_.back();
  glEnable(GL_SCISSOR_TEST);
  // GL scissor 窗口坐标原点 bottom-left；clip rect 是 top-left 设备空间 → Y 翻转。
  GLint gl_y = static_cast<GLint>(height_) -
               static_cast<GLint>(c.y + c.h);
  glScissor(static_cast<GLint>(c.x), gl_y,
            static_cast<GLsizei>(c.w), static_cast<GLsizei>(c.h));
}
```

### 3.4 方法实现（镜像 software）

```cpp
Rect GLESCanvas::CurrentClipDevice() const {
  if (clip_stack_.empty()) {
    return {0, 0, static_cast<vx::f32>(width_),
            static_cast<vx::f32>(height_)};
  }
  return clip_stack_.back();
}

void GLESCanvas::PushClipRect(const Rect& rect) {
  clip_stack_.push_back(CurrentClipDevice().Intersect(rect));
  ApplyScissor();
}

void GLESCanvas::PushClipPath(const Path& path) {
  PushClipRect(path.Bounds());   // D4 AABB 近似
}

void GLESCanvas::PopClip() {
  if (!clip_stack_.empty()) clip_stack_.pop_back();
  ApplyScissor();
}
```

### 3.5 Begin / PushState / PopState 联动（D3 + D5）

```cpp
// Begin() 末尾新增（帧隔离）：
clip_stack_.clear();
glDisable(GL_SCISSOR_TEST);

// PushState()（L254-257）：clip_stack_depth 改存真实深度
state_stack_.push_back({transform_, clip_stack_.size()});

// PopState()（L259-265）：弹栈到记录深度 + reapply
transform_ = s.transform;
while (clip_stack_.size() > s.clip_stack_depth) clip_stack_.pop_back();
ApplyScissor();
```

## 4. 测试策略（D6 / ~7-8 像素测 / 32×32 surface）

承接 G1.7 P1#1（采样坐标解析推导）+ P1#2（白底正向像素测双通道硬规则 `R>200 && green<50` 杜绝白底假绿）。

| # | 测试 | 验证 |
|:-:|---|---|
| C1 | `PushClipRect_ClipsFill` | clip=(8,8,16,16)，FillRect 全屏红 → clip 内(16,16)红 / clip 外(4,4)白（双通道） |
| C2 | `NestedClip_Intersection` | clip(0,0,20,20) ∩ clip(10,10,20,20) → 仅(15,15)红 / (5,5)+(25,25)白 |
| C3 | `PopClip_RestoresFull` | push→pop→FillRect → 全屏(4,4)+(28,28)红（clip 解除） |
| C4 | `PushClipPath_BoundsApprox` | path bounds 近似裁剪（D4）/ bounds 内红 bounds 外白 |
| C5 | `PushState_PopState_RestoresClip` | PushState→PushClipRect→PopState→FillRect → clip 还原全屏红 |
| C6 | `ClipYFlip_Position` | clip 仅上半(0,0,32,16) → 上半(8,8)红 / 下半(8,24)白（防 Y 翻转 bug 反探针，双向） |
| C7 | `EmptyIntersection_NoFill`（反向探针） | 两不相交 clip → CurrentClip 空 → FillRect 无任何 fill（全白） |
| C8 | `BeginResetsClip`（反向探针） | push clip → Begin() → clip_stack_ 清空 + scissor disable → FillRect 全屏红 |

**采样坐标解析推导（P1#1）：** clip(8,8,16,16) 内点取 (16,16)（clip 中心 / 像素中心 +0.5 不落边界）；clip 外点取 (4,4)（< clip.x=8）+ (28,28)（> clip.right=24）。嵌套交集中心点解析为 (15,15)。

## 5. 影响面

- **0 新依赖**（`glScissor`/`GL_SCISSOR_TEST` 均 GLES 3.0 core）。
- **0 新 shader / 0 CMake 链接改动**（仅 tests/CMakeLists.txt 注册新测）。
- **0 抽象 Canvas ABI 变更**（3 方法已是 Canvas 虚函数 override）。
- **ctest 预期：** gles 1437 → ~1444-1445（+7-8）/ software 1303 不变 / no-devtool 1141 不变。

## 6. 安全

**本任务不涉及安全变更。** clip rect 为内部几何 / 0 用户输入 / 0 GLSL 拼接 / 0 新 ABI 表面。

## 7. 技术债（MVP 取舍 / G2）

- **PushClipPath 仅 bounds 近似**（非真路径裁剪）— stencil/SDF 真路径裁剪记 G2（与 software `PushClipPath` 同款近似，跨后端一致）。
- **clip 不随 transform 旋转**（D2 axis-aligned 设备空间）— 与 software 一致；真正变换空间 clip 需 stencil，记 G2。
- **clip 未纳入 dirty rect glScissor 集成** — G1.11 独立任务。

## 8. 参考

- 蓝图 §3.10：[`docs/plans/2026-05-05-gles-renderer-blueprint.md`](../plans/2026-05-05-gles-renderer-blueprint.md)
- 镜像源：`veloxa/graphics/software/software_canvas.cc:322-407`（clip_stack_ / CurrentClip / PushState / PopState）
- 前序：[`archive-TASK-20260602-01.md`](../../memory-bank/archive/archive-TASK-20260602-01.md)（G1.9 采样过滤）
