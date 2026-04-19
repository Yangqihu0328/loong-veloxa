// benchmarks/bench_layout_flex.cc
//
// Flex layout benchmarks (TASK-20260419-05 phase-4).
// Run: ./build-bench/benchmarks/bench_layout_flex
//
// Coverage (6 BMs):
//   BM_LayoutFlex<1,8>     single row, few cells
//   BM_LayoutFlex<1,32>    single row, mid cells
//   BM_LayoutFlex<1,128>   single row, many cells (single-axis stress)
//   BM_LayoutFlex<8,8>     mid-square (row x col interaction)
//   BM_LayoutFlex<16,16>   large-square
//   BM_LayoutFlexNested    flex>flex>flex 3-level nesting (4-fanout)
//
// Setup: ctx.stylesheets points at an empty Vector so LayoutEngine routes
// through StyleResolver and reads inline declarations set by layout_corpus.h.

#include <benchmark/benchmark.h>

#include <string>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/css/selector.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/memory/arena_allocator.h"

namespace {

const vx::Vector<vx::css::Stylesheet>& EmptySheets() {
  static const vx::Vector<vx::css::Stylesheet> sheets;
  return sheets;
}

vx::layout::LayoutContext MakeFlexCtx() {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.root_font_size = 16.0f;
  ctx.stylesheets = &EmptySheets();
  return ctx;
}

template <int Rows, int Cols>
static void BM_LayoutFlex(benchmark::State& state) {
  auto& doc = vx::bench::CachedFlexDocument(Rows, Cols);
  auto ctx = MakeFlexCtx();
  for (auto _ : state) {
    vx::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(Rows) * Cols);
  state.SetLabel(std::to_string(Rows) + "x" + std::to_string(Cols));
}
BENCHMARK_TEMPLATE(BM_LayoutFlex, 1, 8);
BENCHMARK_TEMPLATE(BM_LayoutFlex, 1, 32);
BENCHMARK_TEMPLATE(BM_LayoutFlex, 1, 128);
BENCHMARK_TEMPLATE(BM_LayoutFlex, 8, 8);
BENCHMARK_TEMPLATE(BM_LayoutFlex, 16, 16);

static void BM_LayoutFlexNested(benchmark::State& state) {
  auto& doc = vx::bench::CachedNestedFlexDocument();
  auto ctx = MakeFlexCtx();
  for (auto _ : state) {
    vx::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_LayoutFlexNested);

}  // namespace

BENCHMARK_MAIN();
