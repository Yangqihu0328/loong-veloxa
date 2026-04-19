// benchmarks/bench_css_tokenizer.cc
//
// CSS tokenizer benchmarks for vx::css::CssTokenizer.
// Run: ./build-bench/benchmarks/bench_css_tokenizer
//
// Coverage:
//   BM_TokenizeAll         — generated stylesheet, byte-range sweep (7 cases)
//   BM_TokenizeNumericHeavy — declaration list dominated by lengths/numbers
//   BM_TokenizeStringHeavy  — declaration list dominated by quoted strings
//   BM_TokenizeWhitespaceHeavy — input padded with whitespace + comments

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "benchmarks/css_corpus.h"
#include "veloxa/core/css/tokenizer.h"
#include "veloxa/foundation/strings/string_view.h"

namespace {

inline void DrainTokens(vx::css::CssTokenizer& t) {
  while (true) {
    auto tk = t.Next();
    benchmark::DoNotOptimize(tk);
    if (tk.type == vx::css::CssTokenType::kEof) break;
  }
}

// ---- Range sweep over generated stylesheet ----------------------------------
//
// state.range(0) is the *target byte size* of the input; we pick the smallest
// stylesheet (rules x 5 decls) whose serialized length covers it. With
// RangeMultiplier(2)->Range(64, 4096) we get ceil(log2(4096/64))+1 = 7 sample
// points (64/128/256/512/1024/2048/4096).

void BM_TokenizeAll(benchmark::State& state) {
  const auto target_bytes = static_cast<std::size_t>(state.range(0));
  // Each rule of 5 decls is roughly ~140 bytes; pick rule count to match.
  const int rules =
      static_cast<int>((target_bytes + 139) / 140);
  const std::string& css = vx::bench::StylesheetCorpus(rules, 5);
  vx::StringView sv(css.data(), css.size());
  std::size_t bytes = 0;
  for (auto _ : state) {
    vx::css::CssTokenizer t(sv);
    DrainTokens(t);
    bytes += sv.size();
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(bytes));
}
BENCHMARK(BM_TokenizeAll)->RangeMultiplier(2)->Range(64, 4096);

// ---- Numeric-heavy ----------------------------------------------------------

void BM_TokenizeNumericHeavy(benchmark::State& state) {
  static const std::string css = [] {
    std::string s;
    s.reserve(200 * 30);
    for (int i = 0; i < 200; ++i) {
      s += "padding-top: ";
      s += std::to_string(i);
      s += "px; ";
    }
    return s;
  }();
  vx::StringView sv(css.data(), css.size());
  for (auto _ : state) {
    vx::css::CssTokenizer t(sv);
    DrainTokens(t);
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(sv.size()));
}
BENCHMARK(BM_TokenizeNumericHeavy);

// ---- String-heavy -----------------------------------------------------------

void BM_TokenizeStringHeavy(benchmark::State& state) {
  static const std::string css = [] {
    std::string s;
    s.reserve(200 * 40);
    for (int i = 0; i < 200; ++i) {
      s += "font-family: \"some-font-family-name-";
      s += std::to_string(i);
      s += "\"; ";
    }
    return s;
  }();
  vx::StringView sv(css.data(), css.size());
  for (auto _ : state) {
    vx::css::CssTokenizer t(sv);
    DrainTokens(t);
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(sv.size()));
}
BENCHMARK(BM_TokenizeStringHeavy);

// ---- Whitespace + comments --------------------------------------------------

void BM_TokenizeWhitespaceHeavy(benchmark::State& state) {
  static const std::string css = [] {
    std::string s;
    s.reserve(200 * 50);
    for (int i = 0; i < 200; ++i) {
      s += "  /* a fairly long comment ";
      s += std::to_string(i);
      s += " */   \n   ";
      s += "color: red;\n\n";
    }
    return s;
  }();
  vx::StringView sv(css.data(), css.size());
  for (auto _ : state) {
    vx::css::CssTokenizer t(sv);
    DrainTokens(t);
  }
  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(sv.size()));
}
BENCHMARK(BM_TokenizeWhitespaceHeavy);

}  // namespace

BENCHMARK_MAIN();
