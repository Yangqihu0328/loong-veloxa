// benchmarks/bench_render_replay.cc
//
// DisplayList replay benchmarks (TASK-20260419-05 phase-6).
// Run: ./build-bench/benchmarks/bench_render_replay
//
// Coverage (5 BMs):
//   BM_ReplaySmallList     8-div flat layout   (sub-10 paint commands)
//   BM_ReplayMediumList    64-div flat layout
//   BM_ReplayLargeList     512-div flat layout
//   BM_ReplayTextHeavy     32 divs + text     (DrawText-dominated)
//   BM_ReplayImgVsNoImg    16 <img>, image_cache=nullptr (Replay-only path)
//
// Setup: layout once + Record once per shape (cached); inner loop only times
// Replay onto a 256x256 software canvas (process-static buffer to avoid
// inner-loop allocation noise).
//
// ImageCache "with vs without" verdict was deferred to TASK-20260419-09:
// constructing a populated ImageCache requires DecodeFromFile (I/O); when
// layout runs without `ctx.image_cache`, LayoutEngine never marks <img>
// boxes as kReplaced, so Record emits zero kDrawImage commands and Replay
// has nothing to dispatch through Get(). Captured as a Phase-6 finding for
// /reflect and tasks.md follow-up.

#include <benchmark/benchmark.h>

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
#include "veloxa/graphics/software/software_canvas.h"

namespace {

constexpr vx::u32 kW = 256;
constexpr vx::u32 kH = 256;

struct Recorded {
  std::unique_ptr<vx::ArenaAllocator> arena;
  vx::layout::LayoutBox* root;
  vx::render::DisplayList list;
};

const vx::Vector<vx::css::Stylesheet>& EmptySheets() {
  static const vx::Vector<vx::css::Stylesheet> sheets;
  return sheets;
}

vx::layout::LayoutContext MakeCtx(vx::layout::TextShaper* shaper = nullptr) {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = static_cast<vx::f32>(kW);
  ctx.viewport_height = static_cast<vx::f32>(kH);
  ctx.root_font_size = 16.0f;
  ctx.text_shaper = shaper;
  // Same StyleResolver gating as bench_render_record — see comment there.
  ctx.stylesheets = &EmptySheets();
  return ctx;
}

enum class Shape { kFlat, kTextHeavy, kImage };

Recorded& CachedRecorded(Shape shape, int n) {
  static std::mutex mu;
  static std::map<std::pair<Shape, int>, std::unique_ptr<Recorded>> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto key = std::make_pair(shape, n);
  auto it = cache.find(key);
  if (it != cache.end()) return *it->second;

  auto rec = std::make_unique<Recorded>();
  rec->arena = std::make_unique<vx::ArenaAllocator>();

  vx::dom::Document* doc = nullptr;
  vx::layout::SimpleTextShaper shaper;
  auto ctx = MakeCtx(shape == Shape::kTextHeavy ? &shaper : nullptr);
  switch (shape) {
    case Shape::kFlat:
      doc = &vx::bench::CachedFlatStyledDocument(n);
      break;
    case Shape::kTextHeavy:
      doc = &vx::bench::CachedTextHeavyStyledDocument(n);
      break;
    case Shape::kImage:
      doc = &vx::bench::CachedImageStyledDocument(n);
      break;
  }
  rec->root = vx::layout::LayoutEngine::Layout(doc, ctx, *rec->arena);
  rec->list = vx::render::Record(rec->root);
  auto* out = rec.get();
  cache.emplace(key, std::move(rec));
  return *out;
}

vx::gfx::sw::SoftwareCanvas& SharedCanvas() {
  static vx::u32 pixels[kW * kH] = {};
  static vx::gfx::sw::SoftwareCanvas canvas(pixels, kW, kH, kW);
  return canvas;
}

static void BM_ReplaySmallList(benchmark::State& state) {
  auto& rec = CachedRecorded(Shape::kFlat, 8);
  auto& canvas = SharedCanvas();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &canvas);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK(BM_ReplaySmallList);

static void BM_ReplayMediumList(benchmark::State& state) {
  auto& rec = CachedRecorded(Shape::kFlat, 64);
  auto& canvas = SharedCanvas();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &canvas);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK(BM_ReplayMediumList);

static void BM_ReplayLargeList(benchmark::State& state) {
  auto& rec = CachedRecorded(Shape::kFlat, 512);
  auto& canvas = SharedCanvas();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &canvas);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK(BM_ReplayLargeList);

static void BM_ReplayTextHeavy(benchmark::State& state) {
  auto& rec = CachedRecorded(Shape::kTextHeavy, 32);
  auto& canvas = SharedCanvas();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &canvas);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK(BM_ReplayTextHeavy);

static void BM_ReplayImgVsNoImg(benchmark::State& state) {
  auto& rec = CachedRecorded(Shape::kImage, 16);
  auto& canvas = SharedCanvas();
  for (auto _ : state) {
    vx::render::Replay(rec.list, &canvas, /*image_cache=*/nullptr);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(rec.list.size()));
}
BENCHMARK(BM_ReplayImgVsNoImg);

}  // namespace

BENCHMARK_MAIN();
