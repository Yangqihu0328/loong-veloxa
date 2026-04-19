// benchmarks/bench_drawtext.cc
//
// Phase 1 smoke (TASK-20260419-09): single fallback DrawText call to validate
// link graph (vx_text + vx_graphics + vx_platform) and prove the bench harness
// emits non-zero numbers + items_per_second > 0. The full A1+A2 suite lands in
// phase-3.
//
// Run: ./build-bench/benchmarks/bench_drawtext --benchmark_min_time=0.05s

#include <benchmark/benchmark.h>

#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/brush.h"
#include "veloxa/graphics/software/software_canvas.h"
#include "veloxa/graphics/types.h"

namespace {

constexpr vx::u32 kW = 256;
constexpr vx::u32 kH = 256;

vx::gfx::sw::SoftwareCanvas& SmokeCanvas() {
  static vx::u32 pixels[kW * kH] = {};
  static vx::gfx::sw::SoftwareCanvas c(pixels, kW, kH, kW);
  return c;
}

static void BM_DrawTextSmoke(benchmark::State& state) {
  auto& c = SmokeCanvas();
  vx::gfx::Rect r{0.0f, 0.0f, 100.0f, 16.0f};
  auto brush =
      vx::gfx::Brush::Solid(vx::gfx::Color{255, 255, 255, 255});
  for (auto _ : state) {
    c.DrawText(vx::StringView("hi"), r, 16.0f, brush);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_DrawTextSmoke);

}  // namespace

BENCHMARK_MAIN();
