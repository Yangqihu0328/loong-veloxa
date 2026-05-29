# G1.9 `GLESCanvas::DrawImage` 实现计划

**目标：** 新增 `ImageTexturePool`（RGBA8 纹理缓存 + Context Lost/Restored）+ `GLESCanvas::DrawImage`（src/dst rect→UV quad + image shader），复用 G1.8 范式，0 新依赖。

**复杂度：** Level 4 / **决策：** D1-D8 推荐（D2=B 指针键缓存）/ **creative：** 复用 `creative-gles-resources.md` §4（reconcile 折入，跳过独立 `/creative`）

**设计规格：** [`docs/specs/2026-05-29-gles-canvas-drawimage-design.md`](../specs/2026-05-29-gles-canvas-drawimage-design.md)

---

## 0. Phase 0 audit

### §0.1 ctest baseline（实测）

| Matrix | VX_RENDERER | baseline | 本任务后期望 |
|:-:|:-:|:-:|:-:|
| A | software | **1303** | 1303（0 退化）|
| B | no-devtool | **1141** | 1141（0 退化）|
| C | gles | **1416** | **~1432**（+16：image_texture_pool_test ~8 + gles_canvas_image_test ~8）|

### §0.2 reconcile（spec §2，R1-R5 前置消化）

- R1 Image 无 handle → 缓存键 = `(u64)image.pixels()` + (w,h) 校验（D2=B）
- R2 不生成 mipmap（Mesa swrast 风险）
- R3 OnContextRestored 无参（无 ImageCache 依赖）
- R4 image frag 纯采样（API 无 brush/opacity）
- R5 RGBA32 byte 序 = GL_RGBA 直传

### §0.3 依赖 / 复用 audit

- `Image`（`image.h`）RGBA8 / `u32* pixels`、`width()/height()/valid()` ✅
- `Rect`：`x/y/w/h` + `right()/bottom()/IsEmpty()`（software_canvas.cc:284,293 实证）✅
- `HashMap` API = `Find/Insert/clear/size`（G1.8 实证 / P1#B）✅
- `LinkProgram` 已绑 `a_uv`→loc1（G1.8 落地）✅ → image program 复用，无需改 LinkProgram
- 0 新 FetchContent / proxy 空但无拉取 → 守卫 ⊘

### §0.4 add_test guard

`image_texture_pool_test` + `gles_canvas_image_test` 均 `if(VX_RENDERER STREQUAL "gles")`（OFF/software 不编译）。

---

## 1. 文件结构

| # | 文件 | 操作 | 估行 | 共享 | 职责 |
|:-:|---|:-:|:-:|:-:|---|
| 1 | `veloxa/graphics/gles/image_texture_pool.h` | 🆕 | ~55 | — | ImageTexturePool 接口 |
| 2 | `veloxa/graphics/gles/image_texture_pool.cc` | 🆕 | ~90 | — | RGBA8 上传 / 指针键缓存 / Context Lost |
| 3 | `veloxa/graphics/gles/shaders.h` | 🟡 | +~30 | — | kImageVert + kImageFrag + kAllShaderSources 注册 |
| 4 | `veloxa/graphics/gles/gles_canvas.h` | 🟡 | +~22 | — | image program/uniforms/vao/vbo + image_pool_ + DrawImage 声明 |
| 5 | `veloxa/graphics/gles/gles_canvas.cc` | 🟡 | +~90 | — | InitImageResources + DrawImage impl |
| 6 | `veloxa/graphics/CMakeLists.txt` | 🟡 | +1 | **[共享]** | 注册 image_texture_pool.cc（gles guard）|
| 7 | `tests/graphics/gles/image_texture_pool_test.cc` | 🆕 | ~150 | — | ~8 单测 |
| 8 | `tests/graphics/gles/gles_canvas_image_test.cc` | 🆕 | ~190 | — | ~8 像素测 |
| 9 | `tests/CMakeLists.txt` | 🟡 | +~16 | **[共享]** | 注册 2 测（gles guard）|
| **合计** | — | — | **~643** | — | LOC ×[0.85,1.5] = ~547–965 |

---

## 2. 实现步骤（TDD 严格顺序 / 2 轮次）

> 轮次 1 = ImageTexturePool（独立可测）；轮次 2 = DrawImage 集成。每轮 RED→GREEN→REFACTOR→commit。

### 轮次 1 — ImageTexturePool

#### Phase 1A RED

**1A.1** 创建 `tests/graphics/gles/image_texture_pool_test.cc`（fixture：SDL offscreen + EGL MakeCurrent；helper `MakeImage(w,h,rgba)` 构造内存 Image）。

测试矩阵（~8）：

| ID | 名称 | 断言 |
|:-:|---|---|
| A1 | `Ctor_Empty` | `size()==0` |
| A2 | `GetOrUpload_ValidTexture` | 有效 Image → texture≠0 |
| A3 | `GetOrUpload_CacheHit` | 同 Image 二次 → `cache_hits()==1 && size()==1` |
| A4 | `GetOrUpload_CacheMiss` | 两个不同 pixels 的 Image → `cache_misses()==2` |
| A5 | `GetOrUpload_InvalidZero` | 默认构造（空）Image → 返回 0 |
| A6 | `GetOrUpload_NoGLError` | 上传多图 `glGetError()==GL_NO_ERROR` |
| A7 | `OnContextLost_ClearsCache` | OnContextLost 后 `size()==0` |
| A8 | `OnContextRestored_LazyReupload` | Restored 后 size 清零，再 GetOrUpload → miss+1 |

**1A.2** 注册 `image_texture_pool_test`（gles guard）+ `graphics/CMakeLists.txt` 加 image_texture_pool.cc。
**1A.3** `ctest -R ImageTexturePoolTest` → 空壳 FAIL。

#### Phase 1B GREEN

**1B.1** `image_texture_pool.h`（spec §3.1）。
**1B.2** `image_texture_pool.cc`：

```cpp
#include "veloxa/graphics/gles/image_texture_pool.h"
#include "veloxa/graphics/image.h"

namespace vx::gfx::gles {

ImageTexturePool::~ImageTexturePool() {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->value.texture != 0) glDeleteTextures(1, &it->value.texture);
  }
}

GLuint ImageTexturePool::GetOrUpload(const Image& image) {
  if (!image.valid()) return 0;
  const vx::u64 key = reinterpret_cast<vx::u64>(image.pixels());
  if (Entry* e = entries_.Find(key)) {
    if (e->width == image.width() && e->height == image.height()) {
      ++cache_hits_;
      return e->texture;
    }
    // 指针复用到不同尺寸图 → 删旧重传
    if (e->texture != 0) glDeleteTextures(1, &e->texture);
    entries_.Erase(key);
  }
  ++cache_misses_;
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
               static_cast<GLsizei>(image.width()),
               static_cast<GLsizei>(image.height()), 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image.pixels());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  entries_.Insert(key, Entry{tex, image.width(), image.height()});
  return tex;
}

void ImageTexturePool::OnContextLost() { entries_.clear(); }  // 不 glDelete
void ImageTexturePool::OnContextRestored() { entries_.clear(); }

}  // namespace vx::gfx::gles
```

> **注意**：`HashMap` 迭代器字段访问需按实际 API（`it->value` 或 `it->second`）—— 1B 实现前先确认 hash_map.h 迭代器形态，避免 G1.8 同类 API 误判（P1#B）。`Erase` 是否存在亦先确认（hash_map.h:204 有 `Erase`）。

**1B.3** `ctest -R ImageTexturePoolTest` → 8/8 PASS。

#### Phase 1C REFACTOR + commit

- ReadLints；commit `feat(gles): G1.9 round1 — ImageTexturePool (RGBA8 cache)`。

---

### 轮次 2 — GLESCanvas::DrawImage

#### Phase 2A RED

**2A.1** 创建 `tests/graphics/gles/gles_canvas_image_test.cc`（fixture：offscreen + EGL + GLESCanvas(surface)；helper `MakeImage` + 区域扫描双通道）。

测试矩阵（~8）：

| ID | 名称 | 断言（P1#2 双通道）|
|:-:|---|---|
| T1 | `DrawImage_SolidRed_Covered` | 16×16 纯红图绘到 dst，dst 中心 `R>200 && green<60 && blue<60` |
| T2 | `DrawImage_SrcSubRegion` | 左红右蓝图，src 取左半 → dst 全红（`R>200 && blue<60`）|
| T3 | `DrawImage_DstScaleUp` | 8×8 图 → 64×64 dst，dst 远点仍红（纹理放大覆盖）|
| T4 | `DrawImage_AfterSetTransform` | Translate(dx,0) 后红块在平移区；原点区背景 |
| T5 | `DrawImage_MultipleDraws_NoGLError` | 3× draw `glGetError()==GL_NO_ERROR` |
| T6 | `DrawImage_InvalidImage_NoOp` | 默认空 Image → 背景不变 |
| T7 | `DrawImage_EmptyRect_NoOp` | src 或 dst 空 → 背景不变 |
| T8 | `DrawImage_CacheReuse` | 同 Image 二次 draw → 无 GL error（pool 复用路径，间接）|

**2A.2** 注册 `gles_canvas_image_test`。**2A.3** RED：DrawImage stub → 覆盖测 FAIL（no-op 测 PASS）。

#### Phase 2B GREEN

**2B.1** `shaders.h`：加 `kImageVert/kImageFrag`（spec §3.2）+ `kAllShaderSources[]` 注册（kImageVert, kImageFrag）。
**2B.2** `gles_canvas.h`：加 `image_program_/image_vao_/image_vbo_` + `enum ImageUniform{kImageUXformPx,kImageUViewportPx,kImageUTex,kImageUniformCount}` + `image_uniforms_[]` + `std::unique_ptr<ImageTexturePool> image_pool_` + DrawImage override 声明（移除 stub）+ `InitImageResources/DestroyImageResources` 私有方法 + 前置 `#include "veloxa/graphics/gles/image_texture_pool.h"`（或前向声明 + dtor 在 cc）。
**2B.3** `gles_canvas.cc`：
- include `image_texture_pool.h` + `image.h`。
- `InitImageResources`：CompileShader(kImageVert/kImageFrag) + LinkProgram（已绑 a_uv loc1）+ 缓存 3 uniform；glyph 风格交错 VBO（pos loc0 off0 / uv loc1 off2f / stride 4f）。
- ctor：`InitImageResources()` + `image_pool_ = std::make_unique<ImageTexturePool>()`（无条件）。
- dtor：`image_pool_.reset()` + `DestroyImageResources()`（删 program/vao/vbo）。
- `DrawImage` 实现（spec §3.3，含 P1#A：GetOrUpload 后、draw 前重绑纹理）。

**2B.4** `ctest -R GlesCanvasImageTest` → 8/8 PASS。

#### Phase 2C REFACTOR + 2D 三矩阵

- ReadLints；确认 `shader_injection_test` S1 覆盖 2 新 shader。
- 三矩阵：gles 1416→~1432 / software 1303 / no-devtool 1141。
- commit `feat(gles): G1.9 round2 — GLESCanvas::DrawImage via ImageTexturePool` + finalize。

---

## 3. Commit 时间线

| Phase | subject |
|---|---|
| Plan | `chore(plan): land G1.9 DrawImage plan + memory bank` |
| 1A RED | `test(gles): G1.9 round1 RED — ImageTexturePool unit tests` |
| 1B/C | `feat(gles): G1.9 round1 — ImageTexturePool (RGBA8 cache)` |
| 2A RED | `test(gles): G1.9 round2 RED — DrawImage pixel tests` |
| 2B/C/D | `feat(gles): G1.9 round2 — GLESCanvas::DrawImage via ImageTexturePool` |

---

## 4. 风险登记

| ID | 风险 | 级别 | 缓解 |
|---|---|:-:|---|
| R1 | Mesa swrast GL_RGBA8 / glTexImage2D 不支持 | 🟡 | A2 早测；blank → SKIP_IF_SWRAST_BLANK |
| R2 | 像素指针键 ABA（释放后同址同尺寸新图）| 🟢 | 文档化约束 / MVP 接受（spec §6）|
| R3 | HashMap 迭代器/Erase API 形态误判 | 🟡 | 1B 前读 hash_map.h 确认（承接 P1#B）|
| R4 | GetOrUpload mutate 纹理绑定 → draw 前未重绑（G1.8 同 bug）| 🟡 | **P1#A 显式**：spec §3.3 draw 前重绑 + T1 早暴露 |
| R5 | UV 采样 src 子区坐标误判 | 🟡 | T2 左红右蓝解析采样（src 左半 → dst 全红）|

---

## 5. 反复模式预防

| # | 模式 | 抑制 |
|:-:|---|---|
| #1 | 前置依赖未验证 | §0.1-0.4 + Image/Rect/HashMap 实证 ✅ |
| #2 | spec 数据回归 | R1-R5 reconcile 写入 spec/plan ✅ |
| #3 | TDD 倒置 | 2 轮均 RED 先于 GREEN ✅ |
| #4 | 反向探针弱 | T6/T7 + A5 ✅ |
| #6 | Source 溯源 | commit body 必填（software 镜像 + creative §4 标注）✅ |
| #7 | 双 config 盲区 | 2D 三矩阵 ✅ |
| #8 | ctest baseline | §0.1 实测 1416 ✅ |
| **P1#A** | GL 状态副作用契约 | spec §3.3 GetOrUpload 后重绑 + R4 + T1 ✅ |
| **P1#B** | 容器 API 名称 | §0.3 HashMap Find/Insert + R3（迭代器/Erase 1B 前确认）✅ |
| **P1#2** | 白底假绿 | T1-T4 双通道 `R>200 && green<60 && blue<60`（红）/ 对应通道 ✅ |

---

## 6. 创意阶段需求

⊘ **跳过独立 `/creative`** — `creative-gles-resources.md` §4 ImageTexturePool 设计已就位，R1-R3 reconcile 折入 spec，无新增 UI/算法决策。

---

## 7. 估时（plan ×0.6）

| 阶段 | 估时 |
|---|---|
| Plan | ~30-40 min |
| 轮次 1（ImageTexturePool）| ~35-50 min |
| 轮次 2（DrawImage）| ~45-65 min |
| C/D finalize | ~15-20 min |
| **总计 plan ×0.6** | **~125-175 min** |
| **预期实测** | **~70-110 min**（GLES 范式 hex-evidence 极速区 + G1.8 纹理范式直接复用）|

---

**下一步：** `/build` — 轮次 1 RED → GREEN → 轮次 2 RED → GREEN → C/D

**Source:** 蓝图 §3.9 + creative-gles-resources §4（ImageTexturePool / reconcile R1-R3）+ software_canvas.cc:282-320（src/dst 采样镜像）+ G1.8 范式（纹理生命周期 + glyph shader 模板 + P1#A/#B）
