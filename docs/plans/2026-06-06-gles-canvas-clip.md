# GLES `GLESCanvas` Clip 实现计划（G1.10 / glScissor）

**任务 ID：** TASK-20260606-01
**日期：** 2026-06-06
**复杂度：** Level 3
**设计规格：** [`docs/specs/2026-06-06-gles-canvas-clip-design.md`](../specs/2026-06-06-gles-canvas-clip-design.md)
**前置：** G1.5 FillRect ✅（clip 测试用 FillRect 验证）/ ctest 基线 gles 1437 · sw 1303 · no-devtool 1141

## Phase 0 — audit（已在 VAN + Plan 完成）

- ✅ `glScissor` / `glEnable(GL_SCISSOR_TEST)` GLES 3.0 core / 0 新依赖
- ✅ software clip 镜像源确认：`software_canvas.cc:322-407`（clip_stack_ 设备空间原始 rect 交集 / PushClipRect 不应用 transform / PushState 存 size / PopState 弹栈）
- ✅ `Rect::Intersect`（types.h:79-90 / 空交集返回 `{0,0,0,0}`）+ `IsEmpty`（w<=0||h<=0）
- ✅ gles_canvas.h 现状：3 clip stub（L99-101 内联）+ `State.clip_stack_depth` 预留（L115）+ PushState/PopState 已存 0（L256/264 注释 G1.10 占位）
- ✅ Begin() L219-230（glViewport + glEnable(GL_BLEND)）/ FillRect L538-555（draw 验证用）

## 文件结构

| 操作 | 文件 | 职责 | 标注 |
|------|------|------|------|
| 修改 | `veloxa/graphics/gles/gles_canvas.h` | 3 clip stub → out-of-line 声明 + `clip_stack_` 成员 + `ApplyScissor`/`CurrentClipDevice` helper | — |
| 修改 | `veloxa/graphics/gles/gles_canvas.cc` | 实现 3 clip 方法 + ApplyScissor + Begin 复位 + PushState/PopState 联动 | — |
| 创建 | `tests/graphics/gles/gles_canvas_clip_test.cc` | ~8 像素测（C1-C8） | — |
| 修改 | `tests/CMakeLists.txt` | 注册 `gles_canvas_clip_test`（gles guard 内） | **[共享文件]** |

**0 CMake 链接改动 / 0 新 shader / 0 抽象 Canvas ABI 变更。**

## Phase A — RED（测试先行）

### A.1 — 新建 `tests/graphics/gles/gles_canvas_clip_test.cc`

复用既有 GLES 测试 fixture 范式（`SDL_VIDEODRIVER=offscreen` + Sdl2EGLDisplay + Sdl2GLWindowSurface + GLESCanvas / 参考 `gles_canvas_fill_test.cc` 头部）。32×32 surface。

像素读取 helper（承接 G1.7 P1#2 双通道硬规则）：

```cpp
// 红色填充判定：R 高 + G 低（杜绝白底假绿 / P1#2）
static bool IsRed(vx::u32 px) {
  vx::u8 r = px & 0xFF, g = (px >> 8) & 0xFF;
  return r > 200 && g < 50;
}
// 白底判定（未被 fill）：R+G+B 全高
static bool IsWhite(vx::u32 px) {
  vx::u8 r = px & 0xFF, g = (px >> 8) & 0xFF, b = (px >> 16) & 0xFF;
  return r > 200 && g > 200 && b > 200;
}
```

测试矩阵（C1-C8 / 采样坐标解析推导 P1#1）：

```cpp
// C1 PushClipRect_ClipsFill
//   Clear(white) → PushClipRect(8,8,16,16) → FillRect(全屏, red) → PopClip → End
//   读 (16,16)=clip 中心 → IsRed ✅ / 读 (4,4) < clip.x → IsWhite ✅
// C2 NestedClip_Intersection
//   clip(0,0,20,20) → clip(10,10,20,20) → 交集(10,10,10,10)
//   读 (15,15)=交集中心 → IsRed / (5,5) → IsWhite / (25,25) → IsWhite
// C3 PopClip_RestoresFull
//   PushClipRect(8,8,4,4) → PopClip → FillRect(全屏 red)
//   读 (4,4) → IsRed / (28,28) → IsRed（clip 解除）
// C4 PushClipPath_BoundsApprox
//   path（moveTo/lineTo 组成 bounds≈(8,8,12,12)）→ PushClipPath → FillRect 全屏
//   读 bounds 内 (12,12) → IsRed / bounds 外 (28,28) → IsWhite
// C5 PushState_PopState_RestoresClip
//   PushState → PushClipRect(8,8,4,4) → PopState → FillRect(全屏 red)
//   读 (28,28) → IsRed（clip 随 PopState 还原）
// C6 ClipYFlip_Position（Y 翻转双向反探针）
//   PushClipRect(0,0,32,16)=上半 → FillRect 全屏
//   读 (8,8) 上半 → IsRed / (8,24) 下半 → IsWhite（防 gl_y 翻转错位）
// C7 EmptyIntersection_NoFill（反向探针）
//   clip(0,0,10,10) → clip(20,20,10,10) → 空交集
//   FillRect 全屏 → 读 (5,5)+(25,25)+(16,16) 全 IsWhite（无 fill）
// C8 BeginResetsClip（反向探针）
//   PushClipRect(8,8,4,4) → Begin() → FillRect(全屏 red)
//   读 (28,28) → IsRed（Begin 清 clip_stack_ + disable scissor）
```

### A.2 — 注册 `tests/CMakeLists.txt`

在 gles guard 内（紧邻 `gles_canvas_image_test` 注册处）追加：

```cmake
vx_add_test(gles_canvas_clip_test graphics/gles/gles_canvas_clip_test.cc)
```

### A.3 — RED 验证

```bash
cmake --build build-gles --target gles_canvas_clip_test 2>&1 | tail -20
cd build-gles && ctest -R GlesCanvasClipTest --output-on-failure
```

期望：编译通过（3 clip 方法仍是 stub `{}`）但 C1/C2/C4/C6/C7 等**断言 FAIL**（stub 不裁剪 → clip 外也被 fill → IsWhite 失败）。C3/C5/C8 可能 PASS（无 clip 效果时全屏 fill 恰好符合）。RED 信号清晰。

> 注：clip stub 当前内联 `{}`，header 改 out-of-line 声明后须先有 .cc 定义才能链接 —— 故 A 阶段先把 .cc 加上**空实现占位**（仅声明迁移，body 暂留 `{}`）保证链接，断言 FAIL 即 RED。或直接 A→B 连续：header+cc 改声明骨架（body 空）→ 编译 → RED → 填 body → GREEN。

## Phase B — GREEN（实现）

### B.1 — `gles_canvas.h` 改 3 stub 为声明 + 加成员/helper

```cpp
// L99-101 替换：
void PushClipRect(const Rect&) override;
void PushClipPath(const Path&) override;
void PopClip() override;

// private 成员区（clip_stack_depth 附近）新增：
vx::Vector<Rect> clip_stack_;

// private helper 声明（StrokeSegmentQuad 附近）：
void ApplyScissor();
Rect CurrentClipDevice() const;
```

### B.2 — `gles_canvas.cc` 实现

`ApplyScissor` / `CurrentClipDevice` / `PushClipRect` / `PushClipPath` / `PopClip`（见 spec §3.3-3.4 完整代码）。

`Begin()` 末尾（L229 `active_ = true;` 前）新增：

```cpp
  clip_stack_.clear();
  glDisable(GL_SCISSOR_TEST);
```

`PushState()`（L256）：

```cpp
  state_stack_.push_back({transform_, clip_stack_.size()});
```

`PopState()`（L263-264）替换占位注释：

```cpp
  transform_ = s.transform;
  while (clip_stack_.size() > s.clip_stack_depth) clip_stack_.pop_back();
  ApplyScissor();
```

### B.3 — GREEN 验证

```bash
cmake --build build-gles --target gles_canvas_clip_test 2>&1 | tail -20
cd build-gles && ctest -R GlesCanvasClipTest --output-on-failure
```

期望：C1-C8 全 PASS（8/8）。

## Phase C — REFACTOR + 三矩阵 finalize

### C.1 — ReadLints 3 文件（gles_canvas.{h,cc} + clip_test.cc）

### C.2 — 三 build 矩阵 ctest（零退化验证）

```bash
# Matrix gles（含新测）
cmake --build build-gles -j && (cd build-gles && ctest 2>&1 | tail -5)   # 期望 1437 → ~1444-1445
# Matrix software（DEVTOOL=ON）
cmake --build build -j && (cd build && ctest 2>&1 | tail -5)             # 期望 1303 不变
# Matrix no-devtool
cmake --build build-no-devtool -j && (cd build-no-devtool && ctest 2>&1 | tail -5)  # 期望 1141 不变
```

> 若 `build`/`build-no-devtool`/`build-gles` 目录不存在，参考 techContext「GLES 蓝图实施落地节点」+ 既有 build 目录复用（FETCHCONTENT_BASE_DIR）。

### C.3 — commit 链拆分（沿用 G1.7/G1.9 范式）

| Phase | commit | 内容 |
|---|---|---|
| Plan | `chore(plan): land GLES clip plan + memory bank` | spec + plan + MB |
| A RED | `test(gles): TASK-20260606-01 RED — clip scissor tests` | clip_test.cc + CMake（stub 仍空 → 断言 FAIL）|
| B GREEN | `feat(gles): TASK-20260606-01 — PushClipRect/Path/PopClip via glScissor` | gles_canvas.{h,cc} 实现 |
| finalize | `chore(build): finalize TASK-20260606-01 memory bank` | MB 同步（构建完成）|

## 反复模式预防

| # | 模式 | 抑制 |
|:-:|---|---|
| P1#1 | 像素测采样坐标几何误判 | 全部采样点解析推导（clip 中心 / clip 外 < x or > right / 交集中心）/ 像素中心 +0.5 不落边界 |
| P1#2 | 白底正向像素测假绿 | `IsRed` 双通道 `R>200 && green<50` 硬规则 |
| #3 变体 | 容器 API 名称 | `clip_stack_` 用 `vx::Vector` push_back/pop_back/back/size/clear/empty（既有 state_stack_ 同款 / 已确认）|
| GL 副作用 | scissor 全局状态泄露 | Begin() glDisable + clip_stack_.clear() 帧复位 / PopState reapply |

## 预估

- **plan ×0.6：** ~3-4.5 h（蓝图 G1.10 Clip 部分 / 扣除 Layer FBO）
- **预期实测：** ~25-40 min（沿用实施类 Level 3 极速区 0.15-0.45× / 镜像 software 高度复用）
- **ctest 预期：** gles 1437 → ~1444-1445（+7-8）/ software 1303 不变 / no-devtool 1141 不变
