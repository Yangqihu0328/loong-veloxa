// benchmarks/bench_hash_map.cc
//
// Foundation HashMap benchmarks.
// Run: ./build/benchmarks/bench_hash_map

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

#include "veloxa/foundation/containers/hash_map.h"

namespace {

// ---- Insert: build map of n distinct keys from empty -------------------------

void BM_HashMapInsertInt(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  for (auto _ : state) {
    vx::HashMap<int, int> m;
    for (int i = 0; i < n; ++i) m.Insert(i, i);
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * n);
}
BENCHMARK(BM_HashMapInsertInt)->RangeMultiplier(16)->Range(64, 16384);

// ---- Lookup hit: cycle through known-present keys ----------------------------

void BM_HashMapLookupHitInt(benchmark::State& state) {
  const int n = static_cast<int>(state.range(0));
  vx::HashMap<int, int> m;
  for (int i = 0; i < n; ++i) m.Insert(i, i * 2);
  int i = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(m.Find(i));
    if (++i == n) i = 0;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HashMapLookupHitInt)->RangeMultiplier(16)->Range(64, 16384);

// ---- Lookup miss: query keys outside the populated range ---------------------

void BM_HashMapLookupMissInt(benchmark::State& state) {
  constexpr int kN = 1024;
  vx::HashMap<int, int> m;
  for (int i = 0; i < kN; ++i) m.Insert(i, i);
  int i = kN;
  for (auto _ : state) {
    benchmark::DoNotOptimize(m.Find(i));
    if (++i == 2 * kN) i = kN;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HashMapLookupMissInt);

// ---- Rehash: insert from 16-default capacity up to 1024 entries --------------

void BM_HashMapRehash(benchmark::State& state) {
  constexpr int kTarget = 1024;
  for (auto _ : state) {
    vx::HashMap<int, int> m;  // starts at default capacity 16, grows by 2x
    for (int i = 0; i < kTarget; ++i) m.Insert(i, i);
    benchmark::DoNotOptimize(m.size());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kTarget);
}
BENCHMARK(BM_HashMapRehash);

}  // namespace

BENCHMARK_MAIN();
