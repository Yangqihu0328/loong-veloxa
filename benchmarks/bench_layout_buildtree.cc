// benchmarks/bench_layout_buildtree.cc
//
// Layout build-tree + block-flow benchmarks (TASK-20260419-05 phase-1 smoke).
// Run: ./build-bench/benchmarks/bench_layout_buildtree

#include <benchmark/benchmark.h>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/foundation/memory/arena_allocator.h"

namespace {

vx::layout::LayoutContext MakeCtx() {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.root_font_size = 16.0f;
  return ctx;
}

vx::dom::Document& SmokeDocument() {
  static vx::dom::Document doc;
  static bool built = false;
  if (!built) {
    auto* body = doc.CreateElement(vx::dom::TagId::kBody);
    doc.AppendChild(body);
    auto* div = doc.CreateElement(vx::dom::TagId::kDiv);
    body->AppendChild(div);
    built = true;
  }
  return doc;
}

static void BM_LayoutBuildTreeSmoke(benchmark::State& state) {
  auto& doc = SmokeDocument();
  auto ctx = MakeCtx();
  for (auto _ : state) {
    vx::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_LayoutBuildTreeSmoke);

}  // namespace

BENCHMARK_MAIN();
