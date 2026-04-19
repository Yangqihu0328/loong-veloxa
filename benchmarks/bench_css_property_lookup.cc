// benchmarks/bench_css_property_lookup.cc
//
// CSS PropertyIdFromName benchmarks. Phase 1 smoke — full BM suite + cluster
// probes added in Phase 4.

#include <benchmark/benchmark.h>

#include <cstddef>
#include <string>

#include "benchmarks/css_corpus.h"
#include "veloxa/core/css/property.h"
#include "veloxa/foundation/strings/string_view.h"

namespace {

void BM_PropertyLookupSmoke(benchmark::State& state) {
  // Warm up the lazy PropertyMap init outside the timed loop.
  benchmark::DoNotOptimize(
      vx::css::PropertyIdFromName(vx::StringView("display", 7)));

  const auto& names = vx::bench::Hot5PropertyNames();
  std::size_t i = 0;
  for (auto _ : state) {
    const std::string& n = names[i++ % names.size()];
    auto id = vx::css::PropertyIdFromName(vx::StringView(n.data(), n.size()));
    benchmark::DoNotOptimize(id);
  }
}
BENCHMARK(BM_PropertyLookupSmoke);

}  // namespace

BENCHMARK_MAIN();
