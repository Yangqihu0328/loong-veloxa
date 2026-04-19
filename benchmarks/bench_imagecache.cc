// benchmarks/bench_imagecache.cc
//
// Phase 1 smoke (TASK-20260419-09): one Load() to a non-existent path so we
// exercise the path-scan + DecodeFromFile fopen fail path. Cheap enough to
// run in <1us per iter; full B1/B2/B3 suite lands in phase-2.
//
// Run: ./build-bench/benchmarks/bench_imagecache --benchmark_min_time=0.05s

#include <benchmark/benchmark.h>

#include "veloxa/core/image/image_cache.h"
#include "veloxa/foundation/strings/string_view.h"

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
