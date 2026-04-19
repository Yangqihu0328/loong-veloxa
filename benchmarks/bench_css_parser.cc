// benchmarks/bench_css_parser.cc
//
// CSS parser benchmarks for vx::css::CssParser.
// Run: ./build-bench/benchmarks/bench_css_parser
//
// Coverage:
//   BM_ParseStylesheetSmall  — 2 rules x 5 decls
//   BM_ParseStylesheetMedium — 20 rules x 5 decls (typical app sheet)
//   BM_ParseStylesheetLarge  — 200 rules x 5 decls (framework-style)
//   BM_ParseStylesheetWide   — 20 rules x 20 decls (decl explosion per rule)
//   BM_ParseDeclarationListInline/Range(1,32) — inline style="..." path
//                                                (6 cases via RangeMultiplier=2)
//   BM_ParseSelectorListMixed — selector-heavy stylesheet (compound + combos)

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "benchmarks/css_corpus.h"
#include "veloxa/core/css/parser.h"
#include "veloxa/foundation/strings/string_view.h"

namespace {

template <int Rules, int Decls>
void BM_ParseStylesheet(benchmark::State& state) {
  const std::string& css = vx::bench::StylesheetCorpus(Rules, Decls);
  vx::StringView sv(css.data(), css.size());
  for (auto _ : state) {
    auto sheet = vx::css::CssParser::Parse(sv);
    benchmark::DoNotOptimize(sheet);
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(sv.size()));
}
BENCHMARK_TEMPLATE(BM_ParseStylesheet, 2, 5)->Name("BM_ParseStylesheetSmall");
BENCHMARK_TEMPLATE(BM_ParseStylesheet, 20, 5)->Name("BM_ParseStylesheetMedium");
BENCHMARK_TEMPLATE(BM_ParseStylesheet, 200, 5)->Name("BM_ParseStylesheetLarge");
BENCHMARK_TEMPLATE(BM_ParseStylesheet, 20, 20)->Name("BM_ParseStylesheetWide");

// Inline style declaration list — 6 sample sizes via Range(1, 32) x2.
// ceil(log_2(32/1))+1 = 6 cases.
void BM_ParseDeclarationListInline(benchmark::State& state) {
  const auto decls = static_cast<int>(state.range(0));
  const std::string& css = vx::bench::InlineStyleCorpus(decls);
  vx::StringView sv(css.data(), css.size());
  for (auto _ : state) {
    auto v = vx::css::CssParser::ParseDeclarationList(sv);
    benchmark::DoNotOptimize(v);
  }
}
BENCHMARK(BM_ParseDeclarationListInline)->RangeMultiplier(2)->Range(1, 32);

void BM_ParseSelectorListMixed(benchmark::State& state) {
  // 50 rules, each with a 3-component compound + descendant combinator.
  // Built once into a TU-local static; the IIFE keeps generation off the
  // benchmark hot path (matches the pattern used in bench_css_tokenizer.cc).
  static const std::string css = [] {
    std::string s;
    s.reserve(50 * 64);
    for (int i = 0; i < 50; ++i) {
      s += "div.cls";
      s += std::to_string(i);
      s += "#id";
      s += std::to_string(i);
      s += " > span.inner { color: red; }\n";
    }
    return s;
  }();
  vx::StringView sv(css.data(), css.size());
  for (auto _ : state) {
    auto sheet = vx::css::CssParser::Parse(sv);
    benchmark::DoNotOptimize(sheet);
  }
}
BENCHMARK(BM_ParseSelectorListMixed);

}  // namespace

BENCHMARK_MAIN();
