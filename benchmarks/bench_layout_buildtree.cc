// benchmarks/bench_layout_buildtree.cc
//
// Layout build-tree + block-flow benchmarks (TASK-20260419-05 phase-3).
// Run: ./build-bench/benchmarks/bench_layout_buildtree
//
// Coverage (3 BM groups → 14 reported rows):
//   BM_LayoutBuildTreeFlat/Range(8, 512)   — flat <body><div>×N      (7 cases)
//   BM_LayoutBuildTreeNested/Range(2, 64)  — single-chain N-level    (6 cases)
//   BM_LayoutBuildTreeMixed                — 3-level x4-fanout + text (1 case)
//
// All cases use a default block-flow LayoutContext (no stylesheets pointer →
// LayoutEngine takes the default ComputedStyle path; block flow only).

#include <benchmark/benchmark.h>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/foundation/memory/arena_allocator.h"

namespace {

vx::layout::LayoutContext MakeCtx(vx::layout::TextShaper* shaper = nullptr) {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.root_font_size = 16.0f;
  ctx.text_shaper = shaper;
  return ctx;
}

static void BM_LayoutBuildTreeFlat(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  auto& doc = vx::bench::CachedFlatDocument(n);
  auto ctx = MakeCtx();
  for (auto _ : state) {
    vx::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_LayoutBuildTreeFlat)->RangeMultiplier(2)->Range(8, 512);

static void BM_LayoutBuildTreeNested(benchmark::State& state) {
  const int depth = static_cast<int>(state.range(0));
  auto& doc = vx::bench::CachedNestedDocument(depth);
  auto ctx = MakeCtx();
  for (auto _ : state) {
    vx::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * depth);
}
BENCHMARK(BM_LayoutBuildTreeNested)->RangeMultiplier(2)->Range(2, 64);

static void BM_LayoutBuildTreeMixed(benchmark::State& state) {
  auto& doc = vx::bench::CachedMixedDocument();
  vx::layout::SimpleTextShaper shaper;
  auto ctx = MakeCtx(&shaper);
  for (auto _ : state) {
    vx::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_LayoutBuildTreeMixed);

}  // namespace

BENCHMARK_MAIN();
