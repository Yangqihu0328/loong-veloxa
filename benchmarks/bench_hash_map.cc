// benchmarks/bench_hash_map.cc — smoke (Phase 1)
#include <benchmark/benchmark.h>

namespace {
void BM_Smoke(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(state.iterations());
  }
}
BENCHMARK(BM_Smoke);
}  // namespace

BENCHMARK_MAIN();
