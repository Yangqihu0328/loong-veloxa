// benchmarks/bench_css_parser.cc
//
// CSS parser benchmarks. Phase 1 smoke — full BM suite added in Phase 3.

#include <benchmark/benchmark.h>

#include <string>

#include "benchmarks/css_corpus.h"
#include "veloxa/core/css/parser.h"
#include "veloxa/foundation/strings/string_view.h"

namespace {

void BM_ParseStylesheetSmoke(benchmark::State& state) {
  const std::string& css = vx::bench::StylesheetCorpus(2, 5);
  vx::StringView sv(css.data(), css.size());
  for (auto _ : state) {
    auto sheet = vx::css::CssParser::Parse(sv);
    benchmark::DoNotOptimize(sheet);
  }
}
BENCHMARK(BM_ParseStylesheetSmoke);

}  // namespace

BENCHMARK_MAIN();
