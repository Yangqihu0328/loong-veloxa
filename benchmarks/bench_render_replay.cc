// benchmarks/bench_render_replay.cc
//
// DisplayList replay benchmarks (TASK-20260419-05 phase-1 smoke).
// Run: ./build-bench/benchmarks/bench_render_replay

#include <benchmark/benchmark.h>

#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/tag.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace {

constexpr vx::u32 kW = 256;
constexpr vx::u32 kH = 256;

vx::layout::LayoutContext MakeCtx() {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = static_cast<vx::f32>(kW);
  ctx.viewport_height = static_cast<vx::f32>(kH);
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

static void BM_ReplaySmoke(benchmark::State& state) {
  auto& doc = SmokeDocument();
  auto ctx = MakeCtx();
  vx::ArenaAllocator arena;
  auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
  auto list = vx::render::Record(root);

  static vx::u32 pixels[kW * kH] = {};
  vx::gfx::sw::SoftwareCanvas canvas(pixels, kW, kH, kW);

  for (auto _ : state) {
    vx::render::Replay(list, &canvas);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() *
                          static_cast<int64_t>(list.size()));
}
BENCHMARK(BM_ReplaySmoke);

}  // namespace

BENCHMARK_MAIN();
