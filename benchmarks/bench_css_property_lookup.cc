// benchmarks/bench_css_property_lookup.cc
//
// CSS PropertyIdFromName benchmarks.
// Run: ./build-bench/benchmarks/bench_css_property_lookup
//
// Coverage (9 BM cases):
//   BM_PropertyLookupHitAll      — round-robin all 60 real keys
//   BM_PropertyLookupHitHot5     — round-robin 5 high-frequency keys
//   BM_PropertyLookupHitSingle/* — five fixed single keys (cluster probes)
//   BM_PropertyLookupMiss        — round-robin 5 unknown keys
//   BM_BuildPropertyMapInit      — first-call cost anchor (lazy init)
//
// Cluster probe interpretation (per design spec §3.4):
//   If any HitSingle/<key> is more than ~5x slower than HitHot5, the
//   60-entry HashMap<StringView, PropertyId> behind PropertyMap (djb2
//   hash + H1 = h >> 7 probing) has uneven probe-distance distribution
//   on at least that key — direct evidence for the TASK-05 candidate
//   (HashMap hash mixing optimisation), parallel to the std::hash<int>
//   identity-mapping clustering finding from TASK-20260419-02.

#include <benchmark/benchmark.h>

#include <cstddef>
#include <string>

#include "benchmarks/css_corpus.h"
#include "veloxa/core/css/property.h"
#include "veloxa/foundation/strings/string_view.h"

namespace {

inline vx::StringView SV(const std::string& s) {
  return vx::StringView(s.data(), s.size());
}

// Pay the lazy PropertyMap build cost outside the timed loop. The first
// PropertyIdFromName call across the whole process triggers BuildPropertyMap()
// (one-shot ~60-entry insert); subsequent calls are O(1) probes.
inline void Warmup() {
  benchmark::DoNotOptimize(
      vx::css::PropertyIdFromName(vx::StringView("display", 7)));
}

void BM_PropertyLookupHitAll(benchmark::State& state) {
  Warmup();
  const auto& names = vx::bench::AllPropertyNames();
  std::size_t i = 0;
  for (auto _ : state) {
    auto id = vx::css::PropertyIdFromName(SV(names[i++ % names.size()]));
    benchmark::DoNotOptimize(id);
  }
}
BENCHMARK(BM_PropertyLookupHitAll);

void BM_PropertyLookupHitHot5(benchmark::State& state) {
  Warmup();
  const auto& names = vx::bench::Hot5PropertyNames();
  std::size_t i = 0;
  for (auto _ : state) {
    auto id = vx::css::PropertyIdFromName(SV(names[i++ % names.size()]));
    benchmark::DoNotOptimize(id);
  }
}
BENCHMARK(BM_PropertyLookupHitHot5);

template <const char* Key, std::size_t Len>
void BM_PropertyLookupHitSingle(benchmark::State& state) {
  Warmup();
  vx::StringView k(Key, Len);
  for (auto _ : state) {
    auto id = vx::css::PropertyIdFromName(k);
    benchmark::DoNotOptimize(id);
  }
}

constexpr char kK1[] = "display";
constexpr char kK2[] = "color";
constexpr char kK3[] = "margin-top";
constexpr char kK4[] = "border-radius";
constexpr char kK5[] = "transition-timing-function";

BENCHMARK_TEMPLATE(BM_PropertyLookupHitSingle, kK1, sizeof(kK1) - 1)
    ->Name("BM_PropertyLookupHitSingle/display");
BENCHMARK_TEMPLATE(BM_PropertyLookupHitSingle, kK2, sizeof(kK2) - 1)
    ->Name("BM_PropertyLookupHitSingle/color");
BENCHMARK_TEMPLATE(BM_PropertyLookupHitSingle, kK3, sizeof(kK3) - 1)
    ->Name("BM_PropertyLookupHitSingle/margin-top");
BENCHMARK_TEMPLATE(BM_PropertyLookupHitSingle, kK4, sizeof(kK4) - 1)
    ->Name("BM_PropertyLookupHitSingle/border-radius");
BENCHMARK_TEMPLATE(BM_PropertyLookupHitSingle, kK5, sizeof(kK5) - 1)
    ->Name("BM_PropertyLookupHitSingle/transition-timing-function");

void BM_PropertyLookupMiss(benchmark::State& state) {
  Warmup();
  const auto& names = vx::bench::MissPropertyNames();
  std::size_t i = 0;
  for (auto _ : state) {
    auto id = vx::css::PropertyIdFromName(SV(names[i++ % names.size()]));
    benchmark::DoNotOptimize(id);
  }
}
BENCHMARK(BM_PropertyLookupMiss);

// One-shot lazy init cost cannot be measured per iteration — we pay it once
// per process. The body re-invokes the lookup on a different key than Warmup
// to keep the hot path inhabited; the bulk of the build cost is paid by
// Warmup() on iteration 0 of the program.
void BM_BuildPropertyMapInit(benchmark::State& state) {
  Warmup();
  for (auto _ : state) {
    auto id = vx::css::PropertyIdFromName(vx::StringView("z-index", 7));
    benchmark::DoNotOptimize(id);
  }
}
BENCHMARK(BM_BuildPropertyMapInit);

}  // namespace

BENCHMARK_MAIN();
