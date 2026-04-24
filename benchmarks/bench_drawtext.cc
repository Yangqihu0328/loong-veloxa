// benchmarks/bench_drawtext.cc
//
// DrawText hot-path microbenchmarks (TASK-20260419-09 phase-3).
// Run: ./build-bench/benchmarks/bench_drawtext
//
// Coverage (8 BMs):
//   A1 — Direct SoftwareCanvas::DrawText, fallback vs FreeType+HarfBuzz path
//   BM_DrawTextFallback_Short          "hi"          (no font_manager)
//   BM_DrawTextFallback_Medium         19-char text  (no font_manager)
//   BM_DrawTextReal_Cold_Medium        19-char text  (clear glyph_cache per iter)
//   BM_DrawTextReal_Warm_Medium        19-char text  (warm glyph_cache)
//   BM_DrawTextReal_Warm_Short         "hi"          (warm)
//   BM_DrawTextReal_Warm_Long          124-char text (warm)
//
//   A3 — TASK-20260424-04 hb_shape cache pressure (cache-hit / cache-miss)
//   BM_DrawTextReal_Warm_TextVarying_RoundRobin
//                                     128 unique texts, linear round-robin
//                                     -> steady-state 100% hit; stresses
//                                        ShapeCache::LookupOrNull scan over
//                                        a full 128-slot cache.
//   BM_DrawTextReal_Warm_TextVarying_AllMiss
//                                     1024 unique texts, linear sequential
//                                     -> steady-state 100% miss; stresses
//                                        hb_shape + Insert (reference for
//                                        the cache-disabled baseline via
//                                        VX_SHAPE_CACHE_OFF=1).
//
//   A2 — End-to-end Replay TextHeavy true-path vs fallback (validates K1)
//   BM_ReplayTextHeavyFallback         32-div text-heavy DOM, fallback canvas
//   BM_ReplayTextHeavyReal             same DOM, real canvas (warm)
//
// Setup: SharedFM() lazily loads /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
// (system-installed; bench fail-fast exits if missing rather than silently
// falling back -- otherwise "real-path" BMs would degenerate to fallback,
// repeating TASK-05 K1's false positive).

#include <benchmark/benchmark.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/graphics/types.h"
#include "veloxa/text/font_manager.h"
#include "veloxa/text/glyph_cache.h"

namespace {

constexpr vx::u32 kW = 256;
constexpr vx::u32 kH = 256;
constexpr const char* kFontPath =
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

constexpr const char* kShort = "hi";
constexpr const char* kMedium = "The quick brown fox";  // 19 chars
constexpr const char* kLong =
    "Lorem ipsum dolor sit amet consectetur adipiscing elit sed do eiusmod "
    "tempor incididunt ut labore et dolore magna";  // 124 chars

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
      std::fprintf(
          stderr,
          "FATAL: cannot load font %s -- bench requires this system font\n",
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
  vx::gfx::Rect r{0.0f, 0.0f, 2048.0f, 16.0f};
  auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  for (int i = 0; i < 3; ++i) c.DrawText(s, r, 16.0f, br);
}

vx::gfx::Brush WhiteBrush() {
  return vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
}

// ---- A1: direct SoftwareCanvas::DrawText ---------------------------------

static void BM_DrawTextFallback_Short(benchmark::State& state) {
  auto& c = CanvasFallback();
  vx::gfx::Rect r{0.0f, 0.0f, 200.0f, 16.0f};
  auto br = WhiteBrush();
  for (auto _ : state) {
    c.DrawText(vx::StringView(kShort), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_DrawTextFallback_Short);

static void BM_DrawTextFallback_Medium(benchmark::State& state) {
  auto& c = CanvasFallback();
  vx::gfx::Rect r{0.0f, 0.0f, 256.0f, 16.0f};
  auto br = WhiteBrush();
  for (auto _ : state) {
    c.DrawText(vx::StringView(kMedium), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 19);
}
BENCHMARK(BM_DrawTextFallback_Medium);

static void BM_DrawTextReal_Cold_Medium(benchmark::State& state) {
  auto& c = CanvasReal();
  vx::gfx::Rect r{0.0f, 0.0f, 256.0f, 16.0f};
  auto br = WhiteBrush();
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
  auto& c = CanvasReal();
  vx::gfx::Rect r{0.0f, 0.0f, 256.0f, 16.0f};
  auto br = WhiteBrush();
  for (auto _ : state) {
    c.DrawText(vx::StringView(kMedium), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 19);
}
BENCHMARK(BM_DrawTextReal_Warm_Medium);

static void BM_DrawTextReal_Warm_Short(benchmark::State& state) {
  WarmUp(vx::StringView(kShort));
  auto& c = CanvasReal();
  vx::gfx::Rect r{0.0f, 0.0f, 200.0f, 16.0f};
  auto br = WhiteBrush();
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
  vx::gfx::Rect r{0.0f, 0.0f, 2048.0f, 16.0f};
  auto br = WhiteBrush();
  for (auto _ : state) {
    c.DrawText(vx::StringView(kLong), r, 16.0f, br);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 124);
}
BENCHMARK(BM_DrawTextReal_Warm_Long);

// ---- A3: TASK-20260424-04 hb_shape cache pressure ------------------------

// Deterministic medium-length text pool. Each text is ~19 ASCII chars so
// the per-iteration glyph count is close to BM_DrawTextReal_Warm_Medium,
// isolating the cache hit-rate effect from glyph-count variation.
const std::vector<std::string>& VaryingTextPool(std::size_t n) {
  static std::map<std::size_t, std::vector<std::string>> cache;
  static std::mutex mu;
  std::lock_guard<std::mutex> lk(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return it->second;
  std::vector<std::string> pool;
  pool.reserve(n);
  char buf[32];
  for (std::size_t i = 0; i < n; ++i) {
    // 19 chars, varies in all positions across i to defeat any prefix-only
    // fingerprint optimization a future hash might introduce.
    std::snprintf(buf, sizeof(buf), "T%07zu quick brwn", i);
    pool.emplace_back(buf);
  }
  return cache.emplace(n, std::move(pool)).first->second;
}

// pool size == ShapeCache::kCapacity. Linear round-robin access lands
// each text back in cache after one pool traversal; steady-state is
// 100% hit. Warm-up pre-fills the cache.
constexpr std::size_t kPoolRR = 128;

static void BM_DrawTextReal_Warm_TextVarying_RoundRobin(
    benchmark::State& state) {
  const auto& pool = VaryingTextPool(kPoolRR);
  auto& c = CanvasReal();
  vx::gfx::Rect r{0.0f, 0.0f, 256.0f, 16.0f};
  auto br = WhiteBrush();
  // Warm-up: prime the cache so the steady-state loop is entirely hits.
  for (std::size_t i = 0; i < kPoolRR; ++i) {
    c.DrawText(vx::StringView(pool[i].data(), pool[i].size()), r, 16.0f, br);
  }
  std::size_t idx = 0;
  for (auto _ : state) {
    c.DrawText(vx::StringView(pool[idx].data(), pool[idx].size()), r,
               16.0f, br);
    benchmark::ClobberMemory();
    idx = (idx + 1) % kPoolRR;
  }
  state.SetItemsProcessed(state.iterations() * 19);
  state.SetLabel("hit=100%");
}
BENCHMARK(BM_DrawTextReal_Warm_TextVarying_RoundRobin);

// pool size >> cap (1024 > 128). Linear sequential access -> steady-state
// 100% miss. Acts as the "cache-disabled" ceiling; comparing this BM vs
// the pre-cache baseline (VX_SHAPE_CACHE_OFF=1) measures the insert +
// linear-scan overhead introduced by the cache on the miss path.
constexpr std::size_t kPoolMiss = 1024;

static void BM_DrawTextReal_Warm_TextVarying_AllMiss(
    benchmark::State& state) {
  const auto& pool = VaryingTextPool(kPoolMiss);
  auto& c = CanvasReal();
  // Pre-touch glyph cache with the medium base text so glyph raster is
  // warm and we isolate hb_shape / ShapeCache insertion cost.
  WarmUp(vx::StringView(kMedium));
  vx::gfx::Rect r{0.0f, 0.0f, 256.0f, 16.0f};
  auto br = WhiteBrush();
  std::size_t idx = 0;
  for (auto _ : state) {
    c.DrawText(vx::StringView(pool[idx].data(), pool[idx].size()), r,
               16.0f, br);
    benchmark::ClobberMemory();
    idx = (idx + 1) % kPoolMiss;
  }
  state.SetItemsProcessed(state.iterations() * 19);
  state.SetLabel("miss=100%");
}
BENCHMARK(BM_DrawTextReal_Warm_TextVarying_AllMiss);

// ---- A2: end-to-end Replay TextHeavy true vs fallback --------------------

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
  ctx.viewport_width = static_cast<vx::f32>(kW);
  ctx.viewport_height = static_cast<vx::f32>(kH);
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

}  // namespace

BENCHMARK_MAIN();
