// benchmarks/bench_render_record.cc
//
// DisplayList recording benchmarks (TASK-20260419-05 phase-5).
// Run: ./build-bench/benchmarks/bench_render_record
//
// Coverage (5 BMs):
//   BM_RecordSmallTree      8-div flat layout
//   BM_RecordMediumTree     64-div flat layout
//   BM_RecordLargeTree      512-div flat layout
//   BM_RecordTextHeavy      32 divs each holding 1 Text node (+SimpleTextShaper)
//   BM_RecordImgVsNoImg     16 <img> elements, image_cache=nullptr
//
// Setup: layout once into a process-static arena (cached per shape) so the
// inner loop only times Record. image_cache is nullptr everywhere because
// Record only reads layout box image_handle fields and never derefs cache.

#include <benchmark/benchmark.h>

#include <map>
#include <memory>
#include <mutex>
#include <utility>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/foundation/memory/arena_allocator.h"

namespace {

struct LaidOut {
  std::unique_ptr<vx::ArenaAllocator> arena;
  vx::layout::LayoutBox* root;
};

vx::layout::LayoutContext MakeCtx(vx::layout::TextShaper* shaper = nullptr) {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.root_font_size = 16.0f;
  ctx.text_shaper = shaper;
  return ctx;
}

enum class Shape { kFlat, kTextHeavy, kImage };

LaidOut& CachedLayout(Shape shape, int n) {
  static std::mutex mu;
  static std::map<std::pair<Shape, int>, std::unique_ptr<LaidOut>> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto key = std::make_pair(shape, n);
  auto it = cache.find(key);
  if (it != cache.end()) return *it->second;

  auto laid = std::make_unique<LaidOut>();
  laid->arena = std::make_unique<vx::ArenaAllocator>();

  vx::dom::Document* doc = nullptr;
  // SimpleTextShaper must outlive Layout call only. For text-heavy doc we
  // pre-shape via Layout below; the resulting box widths/heights are then
  // baked into the cached LayoutBox tree.
  vx::layout::SimpleTextShaper shaper;
  auto ctx = MakeCtx(shape == Shape::kTextHeavy ? &shaper : nullptr);

  switch (shape) {
    case Shape::kFlat:      doc = &vx::bench::CachedFlatDocument(n);      break;
    case Shape::kTextHeavy: doc = &vx::bench::CachedTextHeavyDocument(n); break;
    case Shape::kImage:     doc = &vx::bench::CachedImageDocument(n);     break;
  }
  laid->root = vx::layout::LayoutEngine::Layout(doc, ctx, *laid->arena);
  auto* out = laid.get();
  cache.emplace(key, std::move(laid));
  return *out;
}

static void BM_RecordSmallTree(benchmark::State& state) {
  auto& laid = CachedLayout(Shape::kFlat, 8);
  for (auto _ : state) {
    auto list = vx::render::Record(laid.root);
    benchmark::DoNotOptimize(list);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_RecordSmallTree);

static void BM_RecordMediumTree(benchmark::State& state) {
  auto& laid = CachedLayout(Shape::kFlat, 64);
  for (auto _ : state) {
    auto list = vx::render::Record(laid.root);
    benchmark::DoNotOptimize(list);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_RecordMediumTree);

static void BM_RecordLargeTree(benchmark::State& state) {
  auto& laid = CachedLayout(Shape::kFlat, 512);
  for (auto _ : state) {
    auto list = vx::render::Record(laid.root);
    benchmark::DoNotOptimize(list);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_RecordLargeTree);

static void BM_RecordTextHeavy(benchmark::State& state) {
  auto& laid = CachedLayout(Shape::kTextHeavy, 32);
  for (auto _ : state) {
    auto list = vx::render::Record(laid.root);
    benchmark::DoNotOptimize(list);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_RecordTextHeavy);

static void BM_RecordImgVsNoImg(benchmark::State& state) {
  auto& laid = CachedLayout(Shape::kImage, 16);
  for (auto _ : state) {
    auto list = vx::render::Record(laid.root, /*image_cache=*/nullptr);
    benchmark::DoNotOptimize(list);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_RecordImgVsNoImg);

}  // namespace

BENCHMARK_MAIN();
