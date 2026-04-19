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
//   A2 — End-to-end Replay TextHeavy true-path vs fallback (validates K1)
//   BM_ReplayTextHeavyFallback         32-div text-heavy DOM, fallback canvas
//   BM_ReplayTextHeavyReal             same DOM, real canvas (warm)
//
// Setup: SharedFM() lazily loads /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
// (system-installed; bench fail-fast exits if missing rather than silently
// falling back -- otherwise "real-path" BMs would degenerate to fallback,
// repeating TASK-05 K1's false positive).

#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

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
