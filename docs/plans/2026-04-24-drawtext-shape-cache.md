# TASK-20260424-04 实现计划：DrawText hb_shape 结果缓存

**任务：** TASK-20260424-04 — SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）
**设计文档：** `docs/specs/2026-04-24-drawtext-shape-cache-design.md`
**分支：** `feature/TASK-20260424-04-drawtext-residual-opt`
**基础 commit：** `78cabf4`（main）+ `03421a5`（VAN MB 初始化）
**预估时长：** ~3.5-4.5 小时（× 0.6 calibration → **~2.1-2.7 hours**；当前 plan 体量属"中窄"档）

---

## 文件结构

### 新建文件

| 路径 | 类型 | 职责 |
|---|---|---|
| `veloxa/foundation/base/hash.h` | header-only | `vx::HashBytesU64` FNV-1a |
| `veloxa/text/shape_cache.h` | header | `ShapedGlyph / ShapedRun / ShapeCacheKey / ShapeCache` 类型与 API |
| `veloxa/text/shape_cache.cc` | source | `ShapeCache` 实现（LookupOrNull / Insert / Clear） |
| `tests/text/shape_cache_test.cc` | test | 9 单元测试（T1-T9）|
| `tests/graphics/drawtext_shape_cache_test.cc` | test | 3 集成测试（I1-I3）+ 2 反向探针（R1, R2）|

### 修改文件

| 路径 | 改动摘要 | 标签 |
|---|---|:-:|
| `veloxa/text/font_manager.h` | + `ShapeOrLookup` / `ClearShapeCache` / `ShapeCache shape_cache_` | |
| `veloxa/text/font_manager.cc` | 实现 `ShapeOrLookup`（含 hb_shape fallback）| |
| `veloxa/graphics/software/software_canvas.cc` | L216-231 段替换为 `ShapeOrLookup` 调用 + blit loop 取 `run->glyphs[i]` | **[影响 TASK-24-03 warm 路径，需验证不回归]** |
| `veloxa/text/CMakeLists.txt` | + `shape_cache.h/.cc` sources | **[共享文件]** |
| `tests/CMakeLists.txt` | 追加 2 行 `vx_add_test(...)` + `target_link_libraries(...)` | **[共享文件]** |
| `benchmarks/bench_drawtext.cc` | + 2 BM + 256 text pool + warmup | |
| `benchmarks/baseline/bench_drawtext.json` | Phase 6 刷新 | |
| `benchmarks/baseline/README.md` | Phase 6 追加 TASK-20260424-04 历史行 | |

### 影响评估

- **[影响前序测试]：** `pixel_blend_test.cc` / `drawtext_coverage_test.cc`（若有）继续 PASS — DrawText API 签名不变，仅内部 shape 路径改造；I1 会验证 byte-identical
- **[子代理相关]：** 无；本任务纯 C++ + TDD 实施，不触发 subagent
- **[FetchContent]：** 无新依赖

---

## Phase 划分

**6 phase / 共 12 任务**，遵循每 phase 末尾 `commit`（TDD：Red → Green → Refactor → Commit）。

### Phase 1 — HashBytesU64 + 单元测试（~15 min）

#### Task 1.1：**RED** — 编写 HashBytesU64 测试（T8, T9）

**改动：** 新建 `tests/text/shape_cache_test.cc`（空骨架；先放 T8, T9，暂不含 cache 测试）

```cpp
// test/text/shape_cache_test.cc
#include <gtest/gtest.h>
#include "veloxa/foundation/base/hash.h"

namespace {

TEST(HashBytesU64, EmptyInput_ReturnsFnvBasis) {
  EXPECT_EQ(vx::HashBytesU64(nullptr, 0), 0xCBF29CE484222325ULL);
  const vx::u8 empty[1] = {0};
  EXPECT_EQ(vx::HashBytesU64(empty, 0), 0xCBF29CE484222325ULL);
}

TEST(HashBytesU64, SingleByteDifference_ChangesHash) {
  const vx::u8 a[] = {'a'};
  const vx::u8 b[] = {'b'};
  EXPECT_NE(vx::HashBytesU64(a, 1), vx::HashBytesU64(b, 1));
}

TEST(HashBytesU64, LengthMatters) {
  const vx::u8 data[] = {'x', 'y'};
  EXPECT_NE(vx::HashBytesU64(data, 1), vx::HashBytesU64(data, 2));
}

}  // namespace
```

**CMake：** `tests/CMakeLists.txt` 在 foundation 段末追加：

```cmake
vx_add_test(shape_cache_test text/shape_cache_test.cc)
target_link_libraries(shape_cache_test PRIVATE vx_text)
```

（`vx_foundation` 已由 `vx_add_test` 宏自动链接；显式链 `vx_text` 因测试文件 include `shape_cache.h`，而 `shape_cache.cc` 归属 vx_text。）

**验证：** `cmake --build build --target shape_cache_test` → **编译失败**（`hash.h` 不存在）→ **RED**

#### Task 1.2：**GREEN** — 实现 HashBytesU64

**新建：** `veloxa/foundation/base/hash.h`

```cpp
// veloxa/foundation/base/hash.h
#ifndef VELOXA_FOUNDATION_BASE_HASH_H_
#define VELOXA_FOUNDATION_BASE_HASH_H_

#include "veloxa/foundation/base/types.h"

namespace vx {

// FNV-1a 64-bit byte hash. Suitable for short-text fingerprinting used in
// short-lived per-process caches (e.g. hb_shape cache). NOT suitable for
// cryptographic uses or adversarial-input resistance.
// Contract:
//   - HashBytesU64(nullptr, 0) == HashBytesU64(ptr, 0) == FNV-1a 64-bit basis
//   - Deterministic across runs within a single ABI / platform.
inline u64 HashBytesU64(const u8* data, usize len) {
  u64 h = 0xCBF29CE484222325ULL;
  for (usize i = 0; i < len; ++i) {
    h ^= static_cast<u64>(data[i]);
    h *= 0x100000001B3ULL;
  }
  return h;
}

}  // namespace vx

#endif  // VELOXA_FOUNDATION_BASE_HASH_H_
```

**验证：** `ctest -R HashBytesU64` → **3 PASS** → **GREEN**

#### Task 1.3：**Commit** Phase 1

```bash
git add veloxa/foundation/base/hash.h tests/text/shape_cache_test.cc tests/CMakeLists.txt
git commit -m "feat(foundation): add HashBytesU64 FNV-1a byte hash [TASK-20260424-04 P1]"
```

---

### Phase 2 — ShapeCache 核心 + 单元测试（~40 min）

#### Task 2.1：**RED** — 编写 ShapeCache 单元测试（T1-T7）

**改动：** 扩展 `tests/text/shape_cache_test.cc`（追加 cache 测试）

关键测试代码（示意，完整写入文件时含 include / setup）：

```cpp
#include "veloxa/text/shape_cache.h"

namespace vx::text {
namespace {

ShapeCacheKey MakeKey(FontHandle f, u32 px, vx::StringView text) {
  return ShapeCacheKey{
      f, px,
      vx::HashBytesU64(reinterpret_cast<const vx::u8*>(text.data()),
                        text.size()),
      static_cast<vx::u32>(text.size())};
}

ShapedRun MakeRun(std::initializer_list<vx::u32> ids) {
  ShapedRun r;
  r.glyphs.reserve(ids.size());
  for (auto id : ids) r.glyphs.push_back({id, 0.f, 0.f, 10.f});
  return r;
}

TEST(ShapeCache, LookupEmpty_ReturnsNull) {
  ShapeCache c;
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 12, "hi")), nullptr);
}

TEST(ShapeCache, InsertThenLookup_ReturnsSame) {
  ShapeCache c;
  auto key = MakeKey(1, 12, "hi");
  ShapedRun* inserted = c.Insert(key, MakeRun({42, 43}));
  const ShapedRun* found = c.LookupOrNull(key);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found, inserted);
  ASSERT_EQ(found->glyphs.size(), 2u);
  EXPECT_EQ(found->glyphs[0].glyph_id, 42u);
}

TEST(ShapeCache, InsertBeyondCapacity_FIFOEvict) {
  ShapeCache c;
  char buf[8];
  // fill 128
  for (vx::usize i = 0; i < ShapeCache::kCapacity; ++i) {
    std::snprintf(buf, 8, "t%03zu", i);
    c.Insert(MakeKey(1, 12, buf), MakeRun({static_cast<vx::u32>(i)}));
  }
  EXPECT_EQ(c.size(), ShapeCache::kCapacity);
  // insert the 129th: should evict entry 0
  std::snprintf(buf, 8, "t%03d", 128);
  c.Insert(MakeKey(1, 12, buf), MakeRun({999}));
  // entry 0 ("t000") should miss now
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 12, "t000")), nullptr);
  // but entry 1 ("t001") still hits
  EXPECT_NE(c.LookupOrNull(MakeKey(1, 12, "t001")), nullptr);
  // and "t128" hits
  EXPECT_NE(c.LookupOrNull(MakeKey(1, 12, "t128")), nullptr);
}

TEST(ShapeCache, KeyMismatch_Font_Miss) {
  ShapeCache c;
  c.Insert(MakeKey(1, 12, "hi"), MakeRun({1}));
  EXPECT_EQ(c.LookupOrNull(MakeKey(2, 12, "hi")), nullptr);
}

TEST(ShapeCache, KeyMismatch_PixelSize_Miss) {
  ShapeCache c;
  c.Insert(MakeKey(1, 12, "hi"), MakeRun({1}));
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 14, "hi")), nullptr);
}

TEST(ShapeCache, FingerprintEqLenMismatch_Miss) {
  // Manually craft a key that shares fingerprint with a different-length entry;
  // even forcing identical fingerprints, text_len guard should reject.
  ShapeCache c;
  ShapeCacheKey k1{1, 12, 0xDEADBEEFDEADBEEFULL, 2};
  ShapeCacheKey k2{1, 12, 0xDEADBEEFDEADBEEFULL, 3};  // same fp, different len
  c.Insert(k1, MakeRun({1}));
  EXPECT_EQ(c.LookupOrNull(k2), nullptr);  // len differs -> miss
  EXPECT_NE(c.LookupOrNull(k1), nullptr);
}

TEST(ShapeCache, Clear_EmptiesButPreservesCapacity) {
  ShapeCache c;
  c.Insert(MakeKey(1, 12, "hi"), MakeRun({1}));
  EXPECT_EQ(c.size(), 1u);
  c.Clear();
  EXPECT_EQ(c.size(), 0u);
  EXPECT_EQ(c.LookupOrNull(MakeKey(1, 12, "hi")), nullptr);
  // insert still works post-Clear
  c.Insert(MakeKey(1, 12, "hi"), MakeRun({2}));
  EXPECT_EQ(c.size(), 1u);
}

}  // namespace
}  // namespace vx::text
```

**验证：** `cmake --build build` → **编译失败**（`shape_cache.h` 未存在）→ **RED**

#### Task 2.2：**GREEN** — 实现 ShapeCache

**新建：** `veloxa/text/shape_cache.h`

```cpp
#ifndef VELOXA_TEXT_SHAPE_CACHE_H_
#define VELOXA_TEXT_SHAPE_CACHE_H_

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/text/font_manager.h"  // for FontHandle

namespace vx::text {

struct ShapedGlyph {
  u32 glyph_id;
  f32 x_offset;
  f32 y_offset;
  f32 x_advance;
};

struct ShapedRun {
  Vector<ShapedGlyph> glyphs;
};

struct ShapeCacheKey {
  FontHandle font;
  u32 pixel_size;
  u64 text_fingerprint;
  u32 text_len;
};

inline bool operator==(const ShapeCacheKey& a, const ShapeCacheKey& b) {
  return a.font == b.font && a.pixel_size == b.pixel_size &&
         a.text_fingerprint == b.text_fingerprint && a.text_len == b.text_len;
}

// Fixed-capacity FIFO cache for hb_shape results. Non-thread-safe; owned
// by FontManager (single-threaded by techContext convention). See design
// doc §3.3 / §6.2 for stability + safety contract.
class ShapeCache {
 public:
  static constexpr usize kCapacity = 128;

  ShapeCache();
  ShapeCache(const ShapeCache&) = delete;
  ShapeCache& operator=(const ShapeCache&) = delete;

  const ShapedRun* LookupOrNull(const ShapeCacheKey& key) const;
  ShapedRun* Insert(ShapeCacheKey key, ShapedRun run);
  void Clear();
  usize size() const { return count_; }

 private:
  struct Entry {
    ShapeCacheKey key{};
    ShapedRun run{};
    bool occupied = false;
  };
  Vector<Entry> entries_;  // always size == kCapacity (pre-allocated)
  usize head_ = 0;
  usize count_ = 0;
};

}  // namespace vx::text

#endif  // VELOXA_TEXT_SHAPE_CACHE_H_
```

**新建：** `veloxa/text/shape_cache.cc`

```cpp
#include "veloxa/text/shape_cache.h"

#include <utility>

namespace vx::text {

ShapeCache::ShapeCache() {
  entries_.reserve(kCapacity);
  for (usize i = 0; i < kCapacity; ++i) entries_.push_back(Entry{});
}

const ShapedRun* ShapeCache::LookupOrNull(const ShapeCacheKey& key) const {
  // O(N=128) linear scan; cache-line prefetcher friendly.
  // Compares 4 fields per entry; early-reject via fingerprint first (hot field).
  for (usize i = 0; i < kCapacity; ++i) {
    const Entry& e = entries_[i];
    if (!e.occupied) continue;
    if (e.key.text_fingerprint == key.text_fingerprint &&
        e.key.text_len == key.text_len &&
        e.key.font == key.font &&
        e.key.pixel_size == key.pixel_size) {
      return &e.run;
    }
  }
  return nullptr;
}

ShapedRun* ShapeCache::Insert(ShapeCacheKey key, ShapedRun run) {
  Entry& slot = entries_[head_];
  if (slot.occupied) {
    // Overwrite: FIFO eviction. count_ stays at kCapacity.
    slot.run = std::move(run);
    slot.key = key;
  } else {
    slot.run = std::move(run);
    slot.key = key;
    slot.occupied = true;
    ++count_;
  }
  head_ = (head_ + 1) % kCapacity;
  return &slot.run;
}

void ShapeCache::Clear() {
  for (usize i = 0; i < kCapacity; ++i) {
    entries_[i].occupied = false;
    entries_[i].run.glyphs.clear();
  }
  head_ = 0;
  count_ = 0;
}

}  // namespace vx::text
```

**CMake 改动：** `veloxa/text/CMakeLists.txt` 添加 `shape_cache.h` + `shape_cache.cc` 到 text target sources。

**验证：** `ctest -R ShapeCache` → **7 tests PASS** + `ctest -R HashBytesU64` 3 PASS → **GREEN**

#### Task 2.3：**Commit** Phase 2

```bash
git add veloxa/text/shape_cache.h veloxa/text/shape_cache.cc \
        veloxa/text/CMakeLists.txt tests/text/shape_cache_test.cc
git commit -m "feat(text): ShapeCache FIFO cache for hb_shape results [TASK-20260424-04 P2]"
```

---

### Phase 3 — FontManager API + SoftwareCanvas 接入（~60 min）

#### Task 3.1：**RED** — 编写集成测试骨架（I1 + R1）

**新建：** `tests/graphics/drawtext_shape_cache_test.cc`（完整 test framework setup）

```cpp
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace {

constexpr vx::u32 kW = 128;
constexpr vx::u32 kH = 64;
constexpr const char* kFontPath =
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

class DrawTextShapeCacheTest : public ::testing::Test {
 protected:
  vx::text::FontManager fm_;
  vx::text::GlyphCache gc_;
  std::vector<vx::u32> pixels_a_, pixels_b_;

  void SetUp() override {
    ASSERT_TRUE(fm_.Init().ok());
    auto r = fm_.LoadFont(vx::StringView(kFontPath), vx::StringView("DejaVu"));
    ASSERT_TRUE(r.ok());
    pixels_a_.assign(kW * kH, 0);
    pixels_b_.assign(kW * kH, 0);
  }

  void Draw(std::vector<vx::u32>& target, vx::StringView text) {
    std::fill(target.begin(), target.end(), 0);
    vx::gfx::SoftwareCanvas canvas(target.data(), kW, kH, kW * 4, &fm_, &gc_);
    vx::gfx::Brush b;
    b.solid = vx::Color{255, 255, 255, 255};
    canvas.DrawText(text, {0, 0, 128, 64}, 12.0f, b);
  }
};

TEST_F(DrawTextShapeCacheTest, SameTextTwice_PixelIdentical) {
  Draw(pixels_a_, vx::StringView("Hello World"));
  Draw(pixels_b_, vx::StringView("Hello World"));  // second call → hit path
  EXPECT_EQ(std::memcmp(pixels_a_.data(), pixels_b_.data(),
                        pixels_a_.size() * sizeof(vx::u32)),
            0);
}

TEST_F(DrawTextShapeCacheTest, AfterClearShapeCache_Identical) {
  Draw(pixels_a_, vx::StringView("Hello World"));
  fm_.ClearShapeCache();  // invalidates cache
  Draw(pixels_b_, vx::StringView("Hello World"));  // forced miss path
  EXPECT_EQ(std::memcmp(pixels_a_.data(), pixels_b_.data(),
                        pixels_a_.size() * sizeof(vx::u32)),
            0);
}

TEST_F(DrawTextShapeCacheTest, MultipleTextsInterleaved) {
  std::vector<vx::u32> buf_a1(kW * kH, 0), buf_a2(kW * kH, 0);
  std::vector<vx::u32> buf_b1(kW * kH, 0);
  Draw(buf_a1, vx::StringView("Alpha"));
  Draw(buf_b1, vx::StringView("Beta"));
  Draw(buf_a2, vx::StringView("Alpha"));  // hit path
  EXPECT_EQ(std::memcmp(buf_a1.data(), buf_a2.data(),
                        buf_a1.size() * sizeof(vx::u32)),
            0);
}

// RED PROBE R2: forced fingerprint collision (synthetic); verifies text_len
// guard. Since we cannot easily force FNV collisions without a brute-force
// search, we instead exercise the direct ShapeCache API with hand-crafted
// keys — that coverage lives in shape_cache_test.cc T6. This file's R2
// focuses on the end-to-end path: draw two texts with different content
// must not silently reuse the other's shape.
TEST_F(DrawTextShapeCacheTest, DifferentTexts_DifferentOutput) {
  std::vector<vx::u32> a(kW * kH, 0), b(kW * kH, 0);
  Draw(a, vx::StringView("ABC"));
  Draw(b, vx::StringView("XYZ"));
  EXPECT_NE(std::memcmp(a.data(), b.data(), a.size() * sizeof(vx::u32)), 0);
}

}  // namespace
```

**CMake 改动：** `tests/CMakeLists.txt` graphics 段末追加：

```cmake
vx_add_test(drawtext_shape_cache_test graphics/drawtext_shape_cache_test.cc)
target_link_libraries(drawtext_shape_cache_test PRIVATE vx_graphics vx_text)
```

**验证：** `cmake --build build` → **编译失败**（`fm_.ClearShapeCache()` 未存在）→ **RED**

#### Task 3.2：**GREEN** — FontManager::ShapeOrLookup + ClearShapeCache

**修改：** `veloxa/text/font_manager.h`

- 在 public section 的 `GetHbFont(...)` 后追加：

```cpp
  // TASK-20260424-04: cache hb_shape results across DrawText calls.
  // Returns nullptr on invalid font / empty text / hb_font unavailable /
  // zero-glyph shape result. Pointer validity: stable until next Clear,
  // FontManager destruction, or overwriting FIFO insert (see shape_cache.h).
  const ShapedRun* ShapeOrLookup(FontHandle font, u32 pixel_size,
                                  StringView text);

  // Test-only / font reload helper.
  void ClearShapeCache();
```

- 在 includes 区追加 `#include "veloxa/text/shape_cache.h"`
- 在 private 区字段末尾追加：`ShapeCache shape_cache_;`

**注意循环依赖：** shape_cache.h 已 include font_manager.h（for FontHandle）→ 此处反向 include 会循环。
**解决方案：** 将 `FontHandle` + `kInvalidFont` 提取到独立头 `veloxa/text/font_handle.h`，shape_cache.h 与 font_manager.h 都 include 它。

额外新建：`veloxa/text/font_handle.h`

```cpp
#ifndef VELOXA_TEXT_FONT_HANDLE_H_
#define VELOXA_TEXT_FONT_HANDLE_H_

#include "veloxa/foundation/base/types.h"

namespace vx::text {

using FontHandle = u32;
static constexpr FontHandle kInvalidFont = 0;

}  // namespace vx::text

#endif  // VELOXA_TEXT_FONT_HANDLE_H_
```

修改 `font_manager.h` / `shape_cache.h`：两处改 `#include "veloxa/text/font_handle.h"`，移除原始 FontHandle 重复定义（font_manager.h L14-15 删除 FontHandle 定义，改 include font_handle.h）。

**修改：** `veloxa/text/font_manager.cc`

追加实现（在文件末，`GetHbFont` 之后）：

```cpp
const ShapedRun* FontManager::ShapeOrLookup(FontHandle font, u32 pixel_size,
                                              StringView text) {
  if (font == kInvalidFont) return nullptr;
  if (text.empty()) return nullptr;
  if (text.size() > 0xFFFF) return nullptr;  // u32 text_len guard (design §6.2)

  ShapeCacheKey key{
      font, pixel_size,
      vx::HashBytesU64(reinterpret_cast<const u8*>(text.data()), text.size()),
      static_cast<u32>(text.size())};

  if (const ShapedRun* hit = shape_cache_.LookupOrNull(key)) {
    return hit;
  }

  // Miss path: perform hb_shape using already-set face + hb_font.
  // Contract: caller has called SetFacePixelSize(font, pixel_size) before.
  hb_font_t* hb_font = GetHbFont(font, pixel_size);
  if (!hb_font) return nullptr;

  // Reuse thread-local hb_buffer (TASK-24-03 HbBufferHolder).
  // NOTE: AcquireThreadLocalHbBuffer is defined in software_canvas.cc's
  // anonymous namespace. We need to either expose it or inline the
  // thread_local pattern here. Choose: move HbBufferHolder to a shared
  // header `veloxa/text/hb_buffer_holder.h` (declared in Task 3.2.5).
  hb_buffer_t* buf = AcquireThreadLocalHbBuffer();
  if (!buf) return nullptr;
  hb_buffer_reset(buf);
  hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0, -1);
  hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
  hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
  hb_buffer_guess_segment_properties(buf);
  hb_shape(hb_font, buf, nullptr, 0);

  unsigned int glyph_count = 0;
  hb_glyph_info_t* gi = hb_buffer_get_glyph_infos(buf, &glyph_count);
  hb_glyph_position_t* gp = hb_buffer_get_glyph_positions(buf, &glyph_count);
  if (glyph_count == 0) return nullptr;

  ShapedRun run;
  run.glyphs.reserve(glyph_count);
  for (u32 i = 0; i < glyph_count; ++i) {
    run.glyphs.push_back(ShapedGlyph{
        static_cast<u32>(gi[i].codepoint),
        static_cast<f32>(gp[i].x_offset) / 64.0f,
        static_cast<f32>(gp[i].y_offset) / 64.0f,
        static_cast<f32>(gp[i].x_advance) / 64.0f});
  }

  return shape_cache_.Insert(std::move(key), std::move(run));
}

void FontManager::ClearShapeCache() { shape_cache_.Clear(); }
```

#### Task 3.2.5：提取 HbBufferHolder 到共享 header

**理由：** `AcquireThreadLocalHbBuffer()` 当前在 `software_canvas.cc` 匿名命名空间内；FontManager 需访问 → 提取到 `veloxa/text/hb_buffer_holder.h`（新建）。

```cpp
// veloxa/text/hb_buffer_holder.h
#ifndef VELOXA_TEXT_HB_BUFFER_HOLDER_H_
#define VELOXA_TEXT_HB_BUFFER_HOLDER_H_

#include <hb.h>

namespace vx::text {

// Thread-local hb_buffer acquired once per thread and reset on each use.
// Ownership: process-lifetime; destroyed at thread exit via the holder's
// destructor. Not exported outside the text subsystem.
hb_buffer_t* AcquireThreadLocalHbBuffer();

}  // namespace vx::text

#endif  // VELOXA_TEXT_HB_BUFFER_HOLDER_H_
```

**新建：** `veloxa/text/hb_buffer_holder.cc` — 移动 `software_canvas.cc` 中的 HbBufferHolder 实现至此。

**修改：** `software_canvas.cc` — 移除本地 HbBufferHolder 定义 + `AcquireThreadLocalHbBuffer` 前向声明；改 include 新头。

#### Task 3.3：**GREEN** — SoftwareCanvas::DrawText 接入

**修改：** `veloxa/graphics/software/software_canvas.cc` L216-231

**原代码：**

```cpp
hb_buffer_t* buf = AcquireThreadLocalHbBuffer();
if (!buf) { DrawTextFallback(...); return; }
hb_buffer_reset(buf);
hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0, -1);
hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
hb_buffer_guess_segment_properties(buf);
hb_shape(hb_font, buf, nullptr, 0);

u32 glyph_count = 0;
hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
```

**新代码：**

```cpp
// TASK-20260424-04: cache hb_shape results across DrawText calls.
const vx::text::ShapedRun* run = font_manager_->ShapeOrLookup(font, pixel_size, text);
if (!run) { DrawTextFallback(text, bounds, font_size, brush); return; }
const u32 glyph_count = static_cast<u32>(run->glyphs.size());
```

**之后**，blit loop 原 `glyph_info[i].codepoint` / `glyph_pos[i].x_offset` 等引用替换：

```cpp
for (u32 i = 0; i < glyph_count; ++i) {
  u32 glyph_id = run->glyphs[i].glyph_id;
  f32 x_offset = run->glyphs[i].x_offset;
  f32 y_offset = run->glyphs[i].y_offset;
  f32 x_advance = run->glyphs[i].x_advance;
  // ... rest unchanged (GlyphCache / blit SIMD paths preserved)
}
```

**add include：** `#include "veloxa/text/shape_cache.h"` 顶部

**移除 include（可选）：** 因 hb_shape/hb_buffer_* 已移到 FontManager，可减少 `<hb.h>` 相关 symbol 依赖（保留视实现细节）。

**验证：**
1. `cmake --build build` 成功编译
2. `ctest -R DrawTextShapeCacheTest` → **4 PASS**（I1, I2, I3, DifferentTexts）
3. 已有 tests 继续 PASS（drawtext*, pixel_blend*）
4. Release 构建 `cmake --build build-release -- -j` 0 warn

#### Task 3.4：**反向探针 R1** — 编译宏 disable path

**改动：** 在 `software_canvas.cc` DrawText 中用 `#ifdef VX_SHAPE_CACHE_DISABLE` 保留老路径；`drawtext_shape_cache_test.cc` 中的 `DifferentTexts_DifferentOutput` 作为 R1 的 end-to-end 行为守卫。正式 R1 是**若定义该宏编译，pixel output 与默认一致** — 但真正要求编译两份难以做 unit test，**留作 code review checklist（archive 时手动验证一次即可）**，不作为 CI gate。

> **调整：** R1 改为文档化 checklist（archive `/reflect` 阶段执行一次手动 disable build + diff pixel 验证），非 CI 强制。这符合 D 纯收尾模式轻量化要求。

#### Task 3.5：**Commit** Phase 3

```bash
git add veloxa/text/font_handle.h veloxa/text/font_manager.h \
        veloxa/text/font_manager.cc veloxa/text/hb_buffer_holder.h \
        veloxa/text/hb_buffer_holder.cc veloxa/text/CMakeLists.txt \
        veloxa/graphics/software/software_canvas.cc \
        tests/graphics/drawtext_shape_cache_test.cc tests/CMakeLists.txt
git commit -m "feat(text): wire ShapeCache into FontManager::ShapeOrLookup + DrawText [TASK-20260424-04 P3]"
```

---

### Phase 4 — 新增 2 BM + pre-baseline 采集（~30 min）

#### Task 4.1：新增 2 BM 骨架

**修改：** `benchmarks/bench_drawtext.cc`

在文件中（合适位置，其他 BM 之后）追加：

```cpp
namespace {
constexpr vx::usize kVaryingPoolSize = 256;

struct VaryingTextPool {
  char buffer[kVaryingPoolSize][24];
  VaryingTextPool() {
    for (vx::usize i = 0; i < kVaryingPoolSize; ++i) {
      std::snprintf(buffer[i], 24, "The lazy dog var %03zu", i);
    }
  }
};
const VaryingTextPool& GetPool() {
  static VaryingTextPool p;
  return p;
}
}  // namespace

static void BM_DrawTextReal_Warm_TextVarying_RoundRobin(benchmark::State& state) {
  auto& fm = SharedFM();
  vx::text::GlyphCache gc;
  vx::Vector<vx::u32> pixels(kW * kH, 0);
  vx::gfx::SoftwareCanvas canvas(pixels.data(), kW, kH, kW * 4, &fm, &gc);
  vx::gfx::Brush br;
  br.solid = vx::Color{255, 255, 255, 255};
  const auto& pool = GetPool();
  // Warm up glyph cache for all 256 texts (prevents cold bitmap cost from
  // contaminating shape-cache measurement).
  for (vx::usize i = 0; i < kVaryingPoolSize; ++i) {
    canvas.DrawText(vx::StringView(pool.buffer[i]),
                    {0, 0, static_cast<vx::f32>(kW), static_cast<vx::f32>(kH)},
                    12.0f, br);
  }
  fm.ClearShapeCache();  // reset shape cache but keep glyph cache warm
  vx::usize idx = 0;
  for (auto _ : state) {
    canvas.DrawText(
        vx::StringView(pool.buffer[idx % kVaryingPoolSize]),
        {0, 0, static_cast<vx::f32>(kW), static_cast<vx::f32>(kH)}, 12.0f, br);
    idx = (idx + 1) % kVaryingPoolSize;
  }
}
BENCHMARK(BM_DrawTextReal_Warm_TextVarying_RoundRobin);

static void BM_DrawTextReal_Warm_TextVarying_AllMiss(benchmark::State& state) {
  auto& fm = SharedFM();
  vx::text::GlyphCache gc;
  vx::Vector<vx::u32> pixels(kW * kH, 0);
  vx::gfx::SoftwareCanvas canvas(pixels.data(), kW, kH, kW * 4, &fm, &gc);
  vx::gfx::Brush br;
  br.solid = vx::Color{255, 255, 255, 255};
  // Pre-warm glyph cache with ASCII digit set to amortize glyph-cache cost
  canvas.DrawText(vx::StringView("0123456789 unique_missABCDEF"),
                  {0, 0, static_cast<vx::f32>(kW), static_cast<vx::f32>(kH)},
                  12.0f, br);
  vx::usize idx = 0;
  char buf[32];
  for (auto _ : state) {
    std::snprintf(buf, 32, "unique_miss %015zu", idx++);
    canvas.DrawText(vx::StringView(buf),
                    {0, 0, static_cast<vx::f32>(kW), static_cast<vx::f32>(kH)},
                    12.0f, br);
  }
}
BENCHMARK(BM_DrawTextReal_Warm_TextVarying_AllMiss);
```

#### Task 4.2：Pre-baseline 采集（**关键步骤**）

**操作（WSL2 稳态协议）：**

```bash
# Step A: 在 shape-cache 接入后的代码上先测 2 新 BM（这是 pre-baseline？不对 — 应该
# 在 cache 接入前测 disabled 版本作为 pre-baseline）
# 正确做法：暂时 checkout Phase 3 前的版本 or 用 VX_SHAPE_CACHE_DISABLE 宏跑一次
```

**修正方案（避免 checkout 脏乱）：** 在 Phase 4.2 走**双路径测量**：

```bash
# 1) 无 cache 基线（Phase 3 前代码；用 git stash 或临时 revert + 环境宏）
cmake --build build-bench -j
cd build-bench/benchmarks
# 稳态预热
sleep 10
./bench_drawtext --benchmark_filter="BM_DrawTextReal_Warm_TextVarying_*" --benchmark_repetitions=3 --benchmark_min_warmup_time=1.0
# 正式测
./bench_drawtext --benchmark_filter="BM_DrawTextReal_Warm_TextVarying_*" --benchmark_repetitions=10 --benchmark_report_aggregates_only=true --benchmark_out=./varying-precache.json --benchmark_out_format=json
```

**替代方案（更简洁）：** 临时加编译宏 `VX_SHAPE_CACHE_BENCH_DISABLE` 到 `ShapeOrLookup` 入口，跳过 lookup 直接走 miss 路径（仍生成 ShapedRun 但不查 / 不 insert）→ 测 pre-baseline → 去掉宏测 post-baseline。

**选定方案：** 环境变量 `VX_SHAPE_CACHE_OFF=1` → `ShapeOrLookup` 入口读 env 跳过 cache（std::getenv 一次 static init 缓存）。轻量、无 checkout 污染。

```cpp
// font_manager.cc ShapeOrLookup 顶部追加（可保留到 archive 后再移除 or 用 NDEBUG 包一层）
static const bool kShapeCacheDisabledEnv = []() {
  const char* v = std::getenv("VX_SHAPE_CACHE_OFF");
  return v && v[0] == '1';
}();
```

then 在 Lookup 处 `if (!kShapeCacheDisabledEnv) { ... LookupOrNull ... }`；miss 分支始终走 → 测 pre-baseline。

**操作命令：**

```bash
# pre-baseline (cache disabled)
VX_SHAPE_CACHE_OFF=1 ./bench_drawtext \
  --benchmark_filter="BM_DrawTextReal_Warm_TextVarying_*" \
  --benchmark_repetitions=10 --benchmark_report_aggregates_only=true \
  --benchmark_out=./varying-precache.json --benchmark_out_format=json

# post-baseline (cache enabled, default)
./bench_drawtext \
  --benchmark_filter="BM_DrawTextReal_Warm_TextVarying_*" \
  --benchmark_repetitions=10 --benchmark_report_aggregates_only=true \
  --benchmark_out=./varying-postcache.json --benchmark_out_format=json
```

**记录到计划文档：** 将两份 JSON 的 mean/cv 记录到 Phase 5 验证节末尾 + baseline README 历史行。

#### Task 4.3：**Commit** Phase 4

```bash
git add benchmarks/bench_drawtext.cc veloxa/text/font_manager.cc
git commit -m "bench: add TextVarying RoundRobin + AllMiss BMs with env-toggle pre-baseline [TASK-20260424-04 P4]"
```

---

### Phase 5 — 完整 bench 验证 + 门槛判决（~40 min）

#### Task 5.1：Full bench run（WSL2 稳态协议）

```bash
# 关闭 CPU freq 扰动（如已配）
# （WSL2 下通常无需额外命令 — 依赖 techContext 约束）

cd build-bench/benchmarks
sleep 10

# 10 reps + aggregate only + full filter
./bench_drawtext \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true \
  --benchmark_min_warmup_time=1.0 \
  --benchmark_out=./full-run.json --benchmark_out_format=json
```

#### Task 5.2：门槛判决

解析 `full-run.json`，填入下表：

| BM | baseline | observed mean | CV | 门槛 | 判决 |
|---|---:|---:|---:|---|:-:|
| `BM_DrawTextReal_Warm_Medium` | 3499 | **?** | ? | < 3200 ns | ? |
| `BM_DrawTextReal_Warm_Short` | 677 | ? | ? | ≤ 744 | ? |
| `BM_DrawTextReal_Warm_Long` | 10573 | ? | ? | ≤ 11630 | ? |
| `BM_DrawTextReal_Cold_Medium` | 28338 | ? | ? | ≤ 31172 | ? |
| `BM_DrawTextReal_Warm_TextVarying_RoundRobin` | pre-baseline | ? | ? | ≤ pre+max(5%, 50ns) | ? |

**CV ≤ 2%** 要求（TASK-24-03 稳态协议），超标则追加 warm-up + 重测。

**D 纯收尾模式分支判决：**

- Medium < 3200 ns：**主目标达成** → 进入 Phase 6
- 3200 ≤ Medium < 3499 ns：**部分达成**（有进步但未越阈值）→ 询问用户是否"部分达成归档"或"追加兜底尝试"；**D 模式下默认接受部分达成归档**
- Medium ≥ 3499 ns：**回归** → rollback ShapeCache + 分析原因 + 归档为"实测证明无空间"
- 任一兜底 BM > baseline × 1.1：**回归** → 调试（常见：short text cache overhead → 方案 = text.size() ≤ 3 的 short-circuit 入口，直接走 miss 路径不查 cache）
- RoundRobin 超门槛：**miss 开销过大** → 尝试 early-exit 短 text；若仍超 → 降级（部分达成归档 / rollback）

#### Task 5.3：**Commit** Phase 5（记录测量结果）

```bash
git add docs/plans/2026-04-24-drawtext-shape-cache.md  # 如更新了判决表
git commit -m "bench: TASK-20260424-04 threshold verdict recorded in plan [P5]"
```

---

### Phase 6 — baseline 刷新 + 文档收尾（~20 min）

#### Task 6.1：baseline 刷新

```bash
cp build-bench/benchmarks/full-run.json benchmarks/baseline/bench_drawtext.json
```

**修改：** `benchmarks/baseline/README.md` — 追加 TASK-20260424-04 历史行：

```markdown
| TASK-20260424-04 | 2026-04-24 | Medium: 3499 → **{XXXX}** (-{Y}%)  | hb_shape 结果缓存 (FIFO 128) + HashBytesU64 FNV-1a;
                                                                          新增 BM_DrawTextReal_Warm_TextVarying_{RoundRobin,AllMiss} |
```

#### Task 6.2：**Commit** Phase 6

```bash
git add benchmarks/baseline/bench_drawtext.json benchmarks/baseline/README.md
git commit -m "bench: refresh drawtext baseline post TASK-20260424-04 [P6]"
```

---

## 测试矩阵总览

| 类别 | 数量 | 文件 |
|---|:-:|---|
| HashBytesU64 单元测试 | 3 | `tests/text/shape_cache_test.cc` |
| ShapeCache 单元测试 | 7 | 同上 |
| DrawText 集成测试 | 3 | `tests/graphics/drawtext_shape_cache_test.cc` |
| 反向探针 R2（内嵌） | 1 | 同上（DifferentTexts）|
| 反向探针 R1（手动 checklist） | 1 | Archive 阶段执行 |
| **合计新增** | **14** | |

**bench 新增：** 2 BM（RoundRobin + AllMiss）

---

## 时间估算

| Phase | 预估 | × 0.6 校准 |
|---|---:|---:|
| P1 HashBytesU64 | 15 min | 9 min |
| P2 ShapeCache + 单测 | 40 min | 24 min |
| P3 FontManager 集成 + DrawText 改造 + 集测 | 60 min | 36 min |
| P4 2 BM + pre-baseline | 30 min | 18 min |
| P5 完整 bench + 判决 | 40 min | 24 min |
| P6 baseline + README | 20 min | 12 min |
| **合计** | **~3h 25m** | **~2h 3m** |

---

## 风险与降级

| 风险 | 概率 | 应对 |
|---|:-:|---|
| R1. hb_shape 迁移出 `software_canvas.cc` 后某个依赖未暴露 | 中 | Task 3.2.5 HbBufferHolder 提取完成后一次 full rebuild 验证 |
| R2. text.size() > 0xFFFF 截断 → 未预期 miss | 低 | 入口早退；测试覆盖 |
| R3. RoundRobin 超 +5% 门槛（miss 开销过大）| 中 | 对 text.size() ≤ 2 加 short-circuit 绕过 cache |
| R4. Short / Cold 回归 | 低 | 兜底 BM 10 reps 守护；超即 rollback |
| R5. 整体 Medium 未达 3200 但有进步 | 中 | **D 模式接受部分达成**（设计 §9 契约 4） |
| R6. pre-baseline 采集漂移（env toggle 跨两次测量）| 低 | 同一 shell 连续运行；记录 load_avg 同步 |

---

## 创意阶段判定

❌ **不需要 `/creative`**（Level 2 任务；3 候选已 VAN 锁定；所有设计决策已在 /plan 头脑风暴 5 问 锁定）。

---

## 下一步

**规划完成** → 进入 `/build` 依次执行 Phase 1 → 6。
