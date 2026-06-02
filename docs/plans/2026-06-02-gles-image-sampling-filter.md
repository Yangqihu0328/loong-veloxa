# GLES 图像采样过滤选项 NEAREST/LINEAR 实现计划

**任务：** TASK-20260602-01（G1.9 技术债 #2 清理）
**复杂度：** Level 2 / **设计规格：** [`docs/specs/2026-06-02-gles-image-sampling-filter-design.md`](../specs/2026-06-02-gles-image-sampling-filter-design.md)
**目标：** `GLESCanvas` 补齐图像采样过滤选项（NEAREST/LINEAR），默认 LINEAR，`ImageTexturePool` 零改动，0 新依赖。

---

## 0. Phase 0 audit

### §0.1 ctest baseline（实测）

| Matrix | VX_RENDERER | baseline | 本任务后期望 |
|:-:|:-:|:-:|:-:|
| A | software | **1303** | 1303（0 退化）|
| B | no-devtool | **1141** | 1141（0 退化）|
| C | gles | **1432** | **~1437**（+5：gles_canvas_image_test 扩展）|

### §0.2 依赖 / 复用 audit

- `Canvas::DrawImage` 3 参纯虚（`canvas.h:39`）/ 子类仅 software + gles ✅（不动抽象 API → D1=①）
- `types.h` 枚举放置（`Color/Point/Rect/Matrix3x2` 同文件风格）✅
- `ImageTexturePool::GetOrUpload` 上传时 LINEAR（`image_texture_pool.cc`）→ **不改**，DrawImage 每 draw 权威覆盖 ✅
- `gles_canvas_image_test.cc` 现有 fixture（offscreen + EGL + `GlY` + ReadPixel + Rgba helper）→ 直接扩展 ✅
- software DrawImage NEAREST 整数截断（`software_canvas.cc:312`）→ D3 仅文档化 ✅
- 0 新 FetchContent ✅

### §0.3 add_test guard

无新增测试可执行文件（扩展现有 `gles_canvas_image_test`，已在 `if(VX_RENDERER STREQUAL "gles")` guard 内）→ CMake **零改动**。

---

## 1. 文件结构

| # | 文件 | 操作 | 估行 | 共享 | 职责 |
|:-:|---|:-:|:-:|:-:|---|
| 1 | `veloxa/graphics/types.h` | 🟡 | +5 | — | `enum class SamplingFilter{kLinear,kNearest}` |
| 2 | `veloxa/graphics/gles/gles_canvas.h` | 🟡 | +6 | — | `SetImageSamplingFilter`/getter + `image_filter_` 成员 |
| 3 | `veloxa/graphics/gles/gles_canvas.cc` | 🟡 | +5 | — | DrawImage 绑定后按 filter 设 `glTexParameteri` |
| 4 | `tests/graphics/gles/gles_canvas_image_test.cc` | 🟡 | +~90 | — | +5 测（F1-F5）|
| **合计** | — | — | **~106** | — | `ImageTexturePool` + CMake **零改动** |

---

## 2. 实现步骤（TDD 严格顺序 / 单轮次）

### Phase A RED

**A.1** 扩展 `tests/graphics/gles/gles_canvas_image_test.cc`：

- helper：`MakeSeamImage()` 构造 2×1 图（texel0=红 `Rgba(255,0,0,255)` / texel1=蓝 `Rgba(0,0,255,255)`）。
- helper：`CountPurple(x0,y0,x1,y1)`（`R∈[60,200] && B∈[60,200]`）。复用现有 `CountRed`/`CountBlue`/`GlY`/`ReadPixel`/`Rgba`。

测试（F1-F5，见 spec §4）：

```cpp
TEST_F(GlesImageTest, SetGetSamplingFilter) {
  GLESCanvas canvas(surface_.get());
  EXPECT_EQ(canvas.image_sampling_filter(), SamplingFilter::kLinear);  // D4 默认
  canvas.SetImageSamplingFilter(SamplingFilter::kNearest);
  EXPECT_EQ(canvas.image_sampling_filter(), SamplingFilter::kNearest);
}

TEST_F(GlesImageTest, DrawImage_DefaultLinear_BlendsAtSeam) {
  Image img = MakeSeamImage();
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 2, 1}, Rect{0, 0, 64, 64});
  canvas.End();
  EXPECT_GT(CountPurple(0, 32, 64, 33), 0);  // 接缝混合证据
}

TEST_F(GlesImageTest, DrawImage_Nearest_HardSeam) {
  Image img = MakeSeamImage();
  GLESCanvas canvas(surface_.get());
  canvas.SetImageSamplingFilter(SamplingFilter::kNearest);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 2, 1}, Rect{0, 0, 64, 64});
  canvas.End();
  EXPECT_EQ(CountPurple(0, 32, 64, 33), 0);     // 硬边无混合
  EXPECT_GT(CountRed(0, 32, 32, 33), 0);
  EXPECT_GT(CountBlue(32, 32, 64, 33), 0);
}

TEST_F(GlesImageTest, DrawImage_Linear_SoftSeam) {
  Image img = MakeSeamImage();
  GLESCanvas canvas(surface_.get());
  canvas.SetImageSamplingFilter(SamplingFilter::kLinear);
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  canvas.DrawImage(img, Rect{0, 0, 2, 1}, Rect{0, 0, 64, 64});
  canvas.End();
  EXPECT_GT(CountPurple(0, 32, 64, 33), 0);
}

TEST_F(GlesImageTest, DrawImage_FilterSwitch_NoGLError) {
  Image img = MakeSeamImage();
  GLESCanvas canvas(surface_.get());
  canvas.Begin();
  canvas.Clear(Color{255, 255, 255, 255});
  while (glGetError() != GL_NO_ERROR) {}
  for (int i = 0; i < 4; ++i) {
    canvas.SetImageSamplingFilter(i % 2 == 0 ? SamplingFilter::kNearest
                                             : SamplingFilter::kLinear);
    canvas.DrawImage(img, Rect{0, 0, 2, 1}, Rect{0, 0, 64, 64});
  }
  canvas.End();
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
}
```

**A.2** 构建 → RED：`SetImageSamplingFilter`/`image_sampling_filter` 未声明 → 编译失败（F1-F5 全 FAIL）。

> RED 形态为编译失败（新 API 未定义），符合 TDD（无生产代码先行）。

### Phase B GREEN

**B.1** `veloxa/graphics/types.h` — 在 `Matrix3x2` 后、命名空间闭合前加：

```cpp
// 图像采样过滤模式。kLinear = 双线性平滑（缩放默认）；kNearest = 最近邻
// （像素艺术 / 精确像素映射）。当前仅 GLESCanvas::DrawImage 消费；software
// 后端固定 NEAREST（整数截断），不读取本枚举（D3 跨后端差异，记技术债）。
enum class SamplingFilter { kLinear, kNearest };
```

**B.2** `gles_canvas.h` — public 区（DrawImage 声明附近）加 setter/getter，private 区加成员：

```cpp
// 设置后续 DrawImage 的纹理采样过滤（GLES 专属 / 非抽象 Canvas API）。
// 默认 kLinear（向后兼容 G1.9）。不受 PushState/PopState 影响（D5）。
void SetImageSamplingFilter(SamplingFilter filter) { image_filter_ = filter; }
SamplingFilter image_sampling_filter() const { return image_filter_; }
```

```cpp
SamplingFilter image_filter_ = SamplingFilter::kLinear;  // G1.9 技术债 #2
```

**B.3** `gles_canvas.cc` `DrawImage` — `glBindTexture(GL_TEXTURE_2D, tex)` 后、`glDrawArrays` 前插入：

```cpp
glBindTexture(GL_TEXTURE_2D, tex);  // bind AFTER GetOrUpload (P1#A)
// 每 draw 权威设置采样过滤（覆盖 pool 上传时的 LINEAR 初值），使同一缓存纹理
// 在 NEAREST/LINEAR 间切换正确（D2：filter 是采样器状态，不入缓存键）。
const GLint gl_filter =
    (image_filter_ == SamplingFilter::kNearest) ? GL_NEAREST : GL_LINEAR;
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter);
glDrawArrays(GL_TRIANGLES, 0, 6);
```

**B.4** 构建 → `ctest -R GlesImageTest` → F1-F5 + 既有 8 测全 PASS（13/13）。

### Phase C REFACTOR + 三矩阵 + finalize

- ReadLints（types.h / gles_canvas.{h,cc} / 测试）。
- 三矩阵：gles 1432→~1437 / software 1303 / no-devtool 1141（0 退化）。
- 确认 `SamplingFilter` enum 不破坏既有 `#include "types.h"` 消费者（纯新增）。
- commit。

---

## 3. Commit 时间线

| Phase | subject |
|---|---|
| Plan | `chore(plan): land GLES image sampling filter plan + memory bank` |
| A RED | `test(gles): TASK-20260602-01 RED — image sampling filter tests` |
| B/C | `feat(gles): TASK-20260602-01 — DrawImage NEAREST/LINEAR sampling filter` |

---

## 4. 创意阶段需求

⊘ **跳过独立 `/creative`** — 设计决策 D1-D5 已在 `/plan` 头脑风暴锁定，无新增 UI/算法决策。

---

## 5. 估时（plan ×0.6）

| 阶段 | 估时 |
|---|---|
| Plan | ~20 min |
| Build（单轮 A→B→C）| ~30-40 min |
| **总计 plan ×0.6** | **~30-36 min** |
| **预期实测** | **~20-25 min**（4 文件小改 + 现有测试 fixture 直接复用）|

---

**下一步：** `/build` — Phase A RED → B GREEN → C 三矩阵 finalize

**Source:** spec §2-4 + G1.9 archive 技术债 #2 + `image_texture_pool.cc` LINEAR 硬编码 + `gles_canvas_image_test.cc` 现有 fixture
