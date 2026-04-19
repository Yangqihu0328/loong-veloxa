// benchmarks/bench_css_tokenizer.cc
//
// CSS tokenizer benchmarks. Phase 1 smoke — full BM suite added in Phase 2.

#include <benchmark/benchmark.h>

#include <string>

#include "benchmarks/css_corpus.h"
#include "veloxa/core/css/tokenizer.h"
#include "veloxa/foundation/strings/string_view.h"

namespace {

void BM_TokenizeAllSmoke(benchmark::State& state) {
  const std::string& css = vx::bench::StylesheetCorpus(2, 5);
  vx::StringView sv(css.data(), css.size());
  for (auto _ : state) {
    vx::css::CssTokenizer t(sv);
    while (true) {
      auto tk = t.Next();
      benchmark::DoNotOptimize(tk);
      if (tk.type == vx::css::CssTokenType::kEof) break;
    }
  }
}
BENCHMARK(BM_TokenizeAllSmoke);

}  // namespace

BENCHMARK_MAIN();
