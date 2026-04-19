# Plan: Replay hot path 深度基准 + 真 ImageCache 通路（A+B）

**任务 ID：** TASK-20260419-09
**复杂度：** Level 2-3
**设计文档：** `docs/specs/2026-04-19-replay-deepbench-imagecache-design.md`
**分支：** `feature/TASK-20260419-09-replay-deepbench`（基于 main `bfe44ae`）
**估时：** 3-3.5h（5 phase）
**TDD 模式：** **bench 类任务豁免严格 TDD**（与 TASK-05 同例）— bench 没有"测试失败再实现"循环，但仍要求每 phase 末跑实际数据验证（smoke 三件套：非零 + items_processed > 0 + JSON aggregate 合法）

## 0. 全局约束

- **共享文件**：`benchmarks/CMakeLists.txt`、`benchmarks/layout_corpus.h`、`benchmarks/README.md`、`benchmarks/baseline/README.md`、`memory-bank/{techContext,systemPatterns,tasks,activeContext,progress}.md` — 改动需保留现有内容
- **Build 目录**：复用 `build-bench/`（Release，TASK-05 已建）；如有 stale cache 用 `cmake --build build-bench` 自动 reconfigure
- **Smoke 验收三件套**（每 phase 末必跑，落实 `writing-plans.mdc §4`）：
  1. exit 0 + 数字非零
  2. `state.SetItemsProcessed > 0`（不可遗漏，否则 `items_per_second=0/s` 是 K1 假阳性教训）
  3. `--benchmark_format=json` 输出 `items_per_second > 0`
- **Linter**：每个 .cc 写完跑 `ReadLints`（已知 `-Werror -Wpedantic` 严格）

---

## Phase 1：corpus 扩展 + 2 smoke .cc + CMake 注册（~30 min）

### 任务 1.1：扩展 `benchmarks/layout_corpus.h`（5 min）

**[共享文件]** 在 `BuildImageStyled` 后追加 `BuildImageWithSrcStyled`，在 `CachedImageStyledDocument` 后追加 `CachedImageWithSrcStyledDocument`：

```cpp
// 加在 detail 命名空间内，BuildImageStyled (行 95-104) 之后
inline void BuildImageWithSrcStyled(vx::dom::Document& doc, int n,
                                    const std::vector<std::string>& paths) {
  VX_DCHECK(static_cast<int>(paths.size()) >= n);
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* img = doc.CreateElement(vx::dom::TagId::kImg);
    img->SetAttribute(vx::InternedString::Intern("src"),
                      vx::String(paths[i].c_str()));
    img->SetInlineDeclaration(vx::css::PropertyId::kBackgroundColor,
                              vx::css::CssValue::Color(0xff993366u));
    body->AppendChild(img);
  }
}
```

```cpp
// 加在 CachedImageStyledDocument (行 291-301) 之后
inline vx::dom::Document& CachedImageWithSrcStyledDocument(
    int n, const std::vector<std::string>& paths) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildImageWithSrcStyled(*doc, n, paths);
  cache.emplace(n, doc);
  return *doc;
}
```

头部 includes 追加：
```cpp
#include <string>
#include <vector>
#include "veloxa/foundation/base/assert.h"  // VX_DCHECK
#include "veloxa/foundation/strings/interned_string.h"
```

### 任务 1.2：写 `benchmarks/bench_drawtext.cc` smoke（5 min）

```cpp
// benchmarks/bench_drawtext.cc — Phase 1 smoke
#include <benchmark/benchmark.h>
#include "veloxa/graphics/software/software_canvas.h"

namespace {
constexpr vx::u32 kW = 256, kH = 256;
vx::gfx::sw::SoftwareCanvas& SmokeCanvas() {
  static vx::u32 pixels[kW * kH] = {};
  static vx::gfx::sw::SoftwareCanvas c(pixels, kW, kH, kW);
  return c;
}

static void BM_DrawTextSmoke(benchmark::State& state) {
  auto& c = SmokeCanvas();
  vx::gfx::Rect r{0, 0, 100, 16};
  auto brush = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  for (auto _ : state) {
    c.DrawText(vx::StringView("hi"), r, 16.0f, brush);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 2);  // 2 chars
}
BENCHMARK(BM_DrawTextSmoke);
}
BENCHMARK_MAIN();
```

### 任务 1.3：写 `benchmarks/bench_imagecache.cc` smoke（5 min）

```cpp
// benchmarks/bench_imagecache.cc — Phase 1 smoke
#include <benchmark/benchmark.h>
#include "veloxa/core/image/image_cache.h"

static void BM_ImageCacheSmoke(benchmark::State& state) {
  vx::image::ImageCache cache;
  for (auto _ : state) {
    auto r = cache.Load(vx::StringView("/nonexistent_smoke.png"));
    benchmark::DoNotOptimize(r);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ImageCacheSmoke);
BENCHMARK_MAIN();
```

> Smoke 故意用 nonexistent 路径让 Load 走完 path 比较 + DecodeFromFile fopen 失败 — 测的是 Load 的"miss + decode-fail"路径开销，**不为零**。Phase 2 才换真 fixture。

### 任务 1.4：注册 CMake（2 min）

**[共享文件]** `benchmarks/CMakeLists.txt` 末尾追加：

```cmake
# Replay deep-bench (TASK-20260419-09): need vx_text for FontManager/GlyphCache.
# bench_imagecache also needs PNG::PNG to write libpng fixtures.
vx_add_benchmark(bench_drawtext   vx_core vx_graphics vx_platform vx_text)
vx_add_benchmark(bench_imagecache vx_core vx_graphics vx_platform vx_text PNG::PNG)
```

### 任务 1.5：构建 + 跑 smoke（10 min）

```bash
cd /home/qihooz/code/loong-veloxa
cmake --build build-bench --target bench_drawtext bench_imagecache -j
./build-bench/benchmarks/bench_drawtext   --benchmark_min_time=0.05s
./build-bench/benchmarks/bench_imagecache --benchmark_min_time=0.05s
```

**验收（smoke 三件套）：**
- exit 0 + 数字非零（DrawText fallback 应 ~10 ns，ImageCache fail-decode 应 ~µs）
- 输出含 `items_per_second > 0`
- `--benchmark_format=json` 末尾 `"items_per_second"` 字段非 0

如失败立即修复（如 link error → vx_text 缺、include 缺）。

### 任务 1.6：commit（3 min）

```
wip(TASK-20260419-09): phase-1 register bench_drawtext + bench_imagecache smoke
```

---

## Phase 2：bench_imagecache.cc 完整 7-8 BMs（~50 min）

### 任务 2.1：写 PngFixturePool helper（10 min）

替换 Phase 1 smoke 内容，开头添加：

```cpp
#include <cstdio>
#include <cstdlib>
#include <unistd.h>  // getpid
#include <png.h>

#include <string>
#include <vector>

namespace {

void WriteOnePng(const char* path, vx::u8 r, vx::u8 g, vx::u8 b) {
  FILE* fp = std::fopen(path, "wb");
  if (!fp) std::abort();
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                            nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  png_init_io(png, fp);
  png_set_IHDR(png, info, 1, 1, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  vx::u8 row[4] = {r, g, b, 255};
  png_bytep rp = row;
  png_write_rows(png, &rp, 1);
  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  std::fclose(fp);
}

struct PngFixturePool {
  std::vector<std::string> paths;
  ~PngFixturePool() { for (auto& p : paths) std::remove(p.c_str()); }
};

PngFixturePool& EnsureFixturePool(int n) {
  static PngFixturePool pool;
  for (int i = static_cast<int>(pool.paths.size()); i < n; ++i) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "/tmp/vx_bench_%d_%d.png", getpid(), i);
    WriteOnePng(buf, 255, 0, 0);
    pool.paths.emplace_back(buf);
  }
  return pool;
}

constexpr int kMaxFixtures = 256;

}  // namespace
```

### 任务 2.2：B1 Load 微基准 4 BMs（15 min）

```cpp
#include "veloxa/core/image/image_cache.h"

static void BM_ImageCacheLoad_Miss(benchmark::State& state) {
  auto& pool = EnsureFixturePool(kMaxFixtures);
  vx::image::ImageCache cache;
  // Pre-load all but the last to keep cache size at kMaxFixtures-1, then
  // load the last fresh per iteration would mutate cache — instead use
  // a fresh cache + fresh path each iter for true "miss" measurement.
  // Strategy: per-iter Pause/Resume to swap cache; cheaper alternative —
  // measure Load on incrementally growing cache (each iter loads next path).
  int i = 0;
  for (auto _ : state) {
    if (i >= kMaxFixtures) {
      state.PauseTiming();
      cache.Clear();
      i = 0;
      state.ResumeTiming();
    }
    auto r = cache.Load(
        vx::StringView(pool.paths[i].data(), pool.paths[i].size()));
    benchmark::DoNotOptimize(r);
    ++i;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ImageCacheLoad_Miss);

template <int kSize>
static void BM_ImageCacheLoad_Hit(benchmark::State& state) {
  auto& pool = EnsureFixturePool(kSize);
  vx::image::ImageCache cache;
  for (int i = 0; i < kSize; ++i) {
    auto r = cache.Load(
        vx::StringView(pool.paths[i].data(), pool.paths[i].size()));
    if (!r.ok()) std::abort();
  }
  // Worst-case hit: last entry (linear scan from index 0).
  auto worst = vx::StringView(pool.paths[kSize - 1].data(),
                              pool.paths[kSize - 1].size());
  for (auto _ : state) {
    auto r = cache.Load(worst);
    benchmark::DoNotOptimize(r);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK_TEMPLATE(BM_ImageCacheLoad_Hit, 1);
BENCHMARK_TEMPLATE(BM_ImageCacheLoad_Hit, 16);
BENCHMARK_TEMPLATE(BM_ImageCacheLoad_Hit, 256);
```

> ⚠️ Miss BM 用 PauseTiming/ResumeTiming 重置 cache。Pause 本身有 ~µs 级开销，但 256 iter 仅 1 次（每 256 iter Pause 一次），相对 Load+Decode 的 ~µs 级开销摊还后噪声 < 5%。如实测噪声大，改用 fresh cache per state.iterations() 块的方案。

### 任务 2.3：B2 端到端 image 真路径 2 BMs（10 min）

```cpp
#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace {
struct ImageRecorded {
  std::unique_ptr<vx::ArenaAllocator> arena;
  std::unique_ptr<vx::image::ImageCache> cache;
  vx::layout::LayoutBox* root = nullptr;
  vx::render::DisplayList list;
};

const vx::Vector<vx::css::Stylesheet>& EmptySheets() {
  static const vx::Vector<vx::css::Stylesheet> sheets;
  return sheets;
}

ImageRecorded& CachedImageRec(int n) {
  static std::mutex mu;
  static std::map<int, std::unique_ptr<ImageRecorded>> mp;
  std::lock_guard<std::mutex> lk(mu);
  auto it = mp.find(n);
  if (it != mp.end()) return *it->second;
  auto rec = std::make_unique<ImageRecorded>();
  rec->arena = std::make_unique<vx::ArenaAllocator>();
  rec->cache = std::make_unique<vx::image::ImageCache>();
  auto& pool = EnsureFixturePool(n);
  for (int i = 0; i < n; ++i) {
    rec->cache->Load(vx::StringView(pool.paths[i].data(), pool.paths[i].size()));
  }
  auto& doc = vx::bench::CachedImageWithSrcStyledDocument(n, pool.paths);
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 256;
  ctx.viewport_height = 256;
  ctx.root_font_size = 16.0f;
  ctx.stylesheets = &EmptySheets();
  ctx.image_cache = rec->cache.get();
  rec->root = vx::layout::LayoutEngine::Layout(&doc, ctx, *rec->arena);
  rec->list = vx::render::Record(rec->root, rec->cache.get());
  auto* out = rec.get();
  mp.emplace(n, std::move(rec));
  return *out;
}

vx::gfx::sw::SoftwareCanvas& SharedCanvas() {
  static vx::u32 pixels[256 * 256] = {};
  static vx::gfx::sw::SoftwareCanvas c(pixels, 256, 256, 256);
  return c;
}
}  // namespace

template <int kN>
static void BM_ReplayImageReal(benchmark::State& state) {
  auto& rec = CachedImageRec(kN);
  auto& c = SharedCanvas();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &c, rec.cache.get());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK_TEMPLATE(BM_ReplayImageReal, 16);
BENCHMARK_TEMPLATE(BM_ReplayImageReal, 64);
```

### 任务 2.4：B3 Get 单次微基准（5 min）

```cpp
static void BM_ImageCacheGet(benchmark::State& state) {
  auto& pool = EnsureFixturePool(64);
  vx::image::ImageCache cache;
  for (int i = 0; i < 64; ++i) {
    cache.Load(vx::StringView(pool.paths[i].data(), pool.paths[i].size()));
  }
  vx::gfx::ImageHandle h = 32;
  for (auto _ : state) {
    auto* img = cache.Get(h);
    benchmark::DoNotOptimize(img);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ImageCacheGet);

BENCHMARK_MAIN();
```

### 任务 2.5：构建 + smoke 三件套（5 min）

```bash
cmake --build build-bench --target bench_imagecache -j
./build-bench/benchmarks/bench_imagecache --benchmark_min_time=0.05s
./build-bench/benchmarks/bench_imagecache --benchmark_min_time=0.05s --benchmark_format=json | tail -50
```

**预期：** 7 BMs（Miss / Hit×3 / ReplayImageReal×2 / Get），全 exit 0 + items_per_second > 0。

### 任务 2.6：commit（3 min）

```
feat(bench): add bench_imagecache full suite (TASK-20260419-09 phase-2)
```

---

## Phase 3：bench_drawtext.cc 完整 6-8 BMs（~60 min）

### 任务 3.1：FontManager + GlyphCache fixture helper（15 min）

替换 Phase 1 smoke 内容，开头：

```cpp
#include <cstdio>
#include <cstdlib>

#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include <benchmark/benchmark.h>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace {
constexpr vx::u32 kW = 256, kH = 256;
constexpr const char* kFontPath =
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

vx::text::FontManager& SharedFM() {
  static vx::text::FontManager fm;
  static bool inited = []() {
    auto s = fm.Init();
    if (!s.ok()) {
      std::fprintf(stderr, "FATAL: FontManager.Init failed\n");
      std::exit(1);
    }
    auto r = fm.LoadFont(vx::StringView(kFontPath), vx::StringView("DejaVu"));
    if (!r.ok()) {
      std::fprintf(stderr,
                   "FATAL: cannot load font %s — bench requires this font\n",
                   kFontPath);
      std::exit(1);
    }
    return true;
  }();
  (void)inited;
  return fm;
}

vx::text::GlyphCache& SharedGC() {
  static vx::text::GlyphCache gc;
  return gc;
}

vx::gfx::sw::SoftwareCanvas& CanvasFallback() {
  static vx::u32 pixels[kW * kH] = {};
  static vx::gfx::sw::SoftwareCanvas c(pixels, kW, kH, kW);
  return c;
}

vx::gfx::sw::SoftwareCanvas& CanvasReal() {
  static vx::u32 pixels[kW * kH] = {};
  static vx::gfx::sw::SoftwareCanvas c(pixels, kW, kH, kW, &SharedFM(),
                                       &SharedGC());
  return c;
}

void WarmUp(vx::StringView s) {
  auto& c = CanvasReal();
  vx::gfx::Rect r{0, 0, 200, 16};
  auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  for (int i = 0; i < 3; ++i) c.DrawText(s, r, 16.0f, br);
}
}  // namespace
```

### 任务 3.2：A1 直接 DrawText 微基准 4 BMs（15 min）

```cpp
constexpr const char* kShort = "hi";
constexpr const char* kMedium = "The quick brown fox";
constexpr const char* kLong =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua";

static void BM_DrawTextFallback_Short(benchmark::State& state) {
  auto& c = CanvasFallback();
  vx::gfx::Rect r{0, 0, 200, 16};
  auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  for (auto _ : state) {
    c.DrawText(vx::StringView(kShort), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_DrawTextFallback_Short);

static void BM_DrawTextFallback_Medium(benchmark::State& state) {
  auto& c = CanvasFallback();
  vx::gfx::Rect r{0, 0, 200, 16};
  auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  for (auto _ : state) {
    c.DrawText(vx::StringView(kMedium), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 19);
}
BENCHMARK(BM_DrawTextFallback_Medium);

static void BM_DrawTextReal_Cold_Medium(benchmark::State& state) {
  (void)CanvasReal();  // ensure init
  vx::gfx::Rect r{0, 0, 200, 16};
  auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  auto& c = CanvasReal();
  for (auto _ : state) {
    state.PauseTiming();
    SharedGC().Clear();
    state.ResumeTiming();
    c.DrawText(vx::StringView(kMedium), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 19);
}
BENCHMARK(BM_DrawTextReal_Cold_Medium);

static void BM_DrawTextReal_Warm_Medium(benchmark::State& state) {
  WarmUp(vx::StringView(kMedium));
  vx::gfx::Rect r{0, 0, 200, 16};
  auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  auto& c = CanvasReal();
  for (auto _ : state) {
    c.DrawText(vx::StringView(kMedium), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 19);
}
BENCHMARK(BM_DrawTextReal_Warm_Medium);
```

### 任务 3.3：A1 长度扫描 + A2 端到端 4 BMs（20 min）

```cpp
static void BM_DrawTextReal_Warm_Short(benchmark::State& state) {
  WarmUp(vx::StringView(kShort));
  auto& c = CanvasReal();
  vx::gfx::Rect r{0, 0, 200, 16};
  auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  for (auto _ : state) {
    c.DrawText(vx::StringView(kShort), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_DrawTextReal_Warm_Short);

static void BM_DrawTextReal_Warm_Long(benchmark::State& state) {
  WarmUp(vx::StringView(kLong));
  auto& c = CanvasReal();
  vx::gfx::Rect r{0, 0, 1024, 16};
  auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  for (auto _ : state) {
    c.DrawText(vx::StringView(kLong), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 124);
}
BENCHMARK(BM_DrawTextReal_Warm_Long);

// A2: end-to-end Replay TextHeavy with both canvases.
namespace {
struct TextRec {
  std::unique_ptr<vx::ArenaAllocator> arena;
  vx::layout::LayoutBox* root = nullptr;
  vx::render::DisplayList list;
};

const vx::Vector<vx::css::Stylesheet>& Empty() {
  static const vx::Vector<vx::css::Stylesheet> s;
  return s;
}

TextRec& CachedTextRec(int n) {
  static std::mutex mu;
  static std::map<int, std::unique_ptr<TextRec>> mp;
  std::lock_guard<std::mutex> lk(mu);
  auto it = mp.find(n);
  if (it != mp.end()) return *it->second;
  auto rec = std::make_unique<TextRec>();
  rec->arena = std::make_unique<vx::ArenaAllocator>();
  vx::layout::SimpleTextShaper shaper;
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = kW;
  ctx.viewport_height = kH;
  ctx.root_font_size = 16.0f;
  ctx.text_shaper = &shaper;
  ctx.stylesheets = &Empty();
  auto& doc = vx::bench::CachedTextHeavyStyledDocument(n);
  rec->root = vx::layout::LayoutEngine::Layout(&doc, ctx, *rec->arena);
  rec->list = vx::render::Record(rec->root);
  auto* out = rec.get();
  mp.emplace(n, std::move(rec));
  return *out;
}
}  // namespace

static void BM_ReplayTextHeavyFallback(benchmark::State& state) {
  auto& rec = CachedTextRec(32);
  auto& c = CanvasFallback();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &c);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK(BM_ReplayTextHeavyFallback);

static void BM_ReplayTextHeavyReal(benchmark::State& state) {
  auto& rec = CachedTextRec(32);
  WarmUp(vx::StringView("The quick brown fox"));
  auto& c = CanvasReal();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &c);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK(BM_ReplayTextHeavyReal);

BENCHMARK_MAIN();
```

### 任务 3.4：构建 + smoke 三件套（5 min）

```bash
cmake --build build-bench --target bench_drawtext -j
./build-bench/benchmarks/bench_drawtext --benchmark_min_time=0.05s
```

**预期 8 BMs：** Fallback_Short / Fallback_Medium / Real_Cold_Medium / Real_Warm_Medium / Real_Warm_Short / Real_Warm_Long / ReplayTextHeavyFallback / ReplayTextHeavyReal。

**关键比对（即时数据分析）：**
- `Real_Cold_Medium` >> `Real_Warm_Medium` 量级差（如 10×+）→ glyph_cache 是 ROI 最高优化
- `Fallback_Medium` ≈ `Real_Warm_Medium` → fallback 误报为 hot path（K1 的 8200 ns/cmd 其实是 ~ 真路径数值，800× 仍成立但根因解释不同）
- `Fallback_Medium` >> `Real_Warm_Medium` → fallback 才是慢源（K1 假阳性）；真路径已 OK

### 任务 3.5：commit（5 min）

```
feat(bench): add bench_drawtext full suite + K1 verdict (TASK-20260419-09 phase-3)
```

---

## Phase 4：baseline JSON 入仓 + README 更新（~30 min）

### 任务 4.1：生成 2 baseline JSON（10 min）

```bash
mkdir -p benchmarks/baseline
./build-bench/benchmarks/bench_drawtext \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true \
  --benchmark_min_time=0.05s \
  --benchmark_format=json > benchmarks/baseline/bench_drawtext.json

./build-bench/benchmarks/bench_imagecache \
  --benchmark_repetitions=3 \
  --benchmark_report_aggregates_only=true \
  --benchmark_min_time=0.05s \
  --benchmark_format=json > benchmarks/baseline/bench_imagecache.json
```

**4-piece 验收（同 TASK-05）：**
1. JSON 合法（`python3 -c 'import json,sys; json.load(open(sys.argv[1]))' file.json` exit 0）
2. 含 `aggregate_name=median` 行
3. `iterations > 0` 全部 BM
4. `cpu_time` 单位 ns，数值非零

### 任务 4.2：更新 `benchmarks/README.md`（10 min）

**[共享文件]** exe 表 11→13 行；新增「TASK-09 K1/K5 量化结论」段（具体数值 phase 3 跑完后填入）。

### 任务 4.3：更新 `benchmarks/baseline/README.md`（5 min）

**[共享文件]** 在 TASK-05 章节后追加 TASK-09 章节，标注 4-piece 协议套用 + 关键发现 K1/K5 结论。

### 任务 4.4：commit（5 min）

```
docs(bench): add 2 deepbench baselines + README updates (TASK-20260419-09 phase-4)
```

---

## Phase 5：techContext + systemPatterns + MB 收尾（~30 min）

### 任务 5.1：更新 `memory-bank/techContext.md`（10 min）

**[共享文件]** 在「Render 性能基线 → ### Replay」段追加 K1 真值 + ImageCache 通路 + B1 O(N) 验证结果。

### 任务 5.2：更新 `memory-bank/systemPatterns.md`（10 min）

**[共享文件]** 「Render Bench 前置清单」段补 1 行：「DrawText 真路径需 SoftwareCanvas ctor 同时传 `font_manager + glyph_cache`（line 145 fallback gate），任一缺失即走逐字符 FillRect」。

### 任务 5.3：更新 `memory-bank/{tasks,activeContext,progress}.md`（5 min）

- `tasks.md`：状态 → `🟢 构建完成`，验收标准全部勾选 + 关键发现表
- `activeContext.md`：阶段 → `构建完成`，下一步 `/reflect`
- `progress.md`：5 phase 完成记录 + 关键发现摘要

### 任务 5.4：构建完成验证（5 min）

```bash
# 所有 bench 全跑 smoke 一次确认零回归
for b in bench_allocators bench_containers bench_hash_map bench_strings \
         bench_css_tokenizer bench_css_parser bench_css_property_lookup \
         bench_layout_buildtree bench_layout_flex bench_render_record \
         bench_render_replay bench_drawtext bench_imagecache; do
  ./build-bench/benchmarks/$b --benchmark_min_time=0.05s --benchmark_filter='.*Smoke.*|.*Small.*|.*Short.*|.*Get.*' 2>&1 \
    | tail -3 | head -1
done

# Debug ctest 防回归
cd build && ctest -j8 --output-on-failure
```

### 任务 5.5：finalize commit（5 min）

```
chore(build): finalize TASK-20260419-09 memory bank state
```

---

## 提交清单（预计 7 commits）

| # | 主题 |
|---|------|
| 1 | `docs(plan): TASK-20260419-09 replay deepbench design + plan` |
| 2 | `wip(TASK-20260419-09): phase-1 register bench_drawtext + bench_imagecache smoke` |
| 3 | `feat(bench): add bench_imagecache full suite (TASK-20260419-09 phase-2)` |
| 4 | `feat(bench): add bench_drawtext full suite + K1 verdict (TASK-20260419-09 phase-3)` |
| 5 | `docs(bench): add 2 deepbench baselines + README updates (TASK-20260419-09 phase-4)` |
| 6 | `docs(mb): TASK-20260419-09 techContext/systemPatterns updates (phase-5)` |
| 7 | `chore(build): finalize TASK-20260419-09 memory bank state` |

## 时间总览

| Phase | 任务 | 估时 |
|-------|------|------|
| 1 | corpus 扩 + 2 smoke + CMake | 30 min |
| 2 | bench_imagecache 全套 7 BMs | 50 min |
| 3 | bench_drawtext 全套 8 BMs | 60 min |
| 4 | 2 baseline JSON + README | 30 min |
| 5 | techContext + systemPatterns + MB 收尾 | 30 min |
| **总计** | **5 phase / 15 BMs** | **~3.5h** |

## 不需要 `/creative` 阶段

理由：所有设计决策已在 plan 阶段头脑风暴 D1-D5 完成，无 UI/算法/架构空白。
