// benchmarks/bench_strings.cc
//
// Foundation string benchmarks.
// Run: ./build/benchmarks/bench_strings
//
// Note: BasicString SSO threshold is 22 bytes; lengths >22 take the
// heap path. InternedString::ClearInternedStrings() resets the global
// table between SetUp/TearDown to keep the dup-intern case meaningful.

#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdint>
#include <string>

#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/foundation/strings/string.h"

namespace {

// ---- Construct: SSO path -----------------------------------------------------

void BM_StringConstructSso(benchmark::State& state) {
  const std::string src(static_cast<std::size_t>(state.range(0)), 'x');
  for (auto _ : state) {
    vx::String s(src.c_str());
    benchmark::DoNotOptimize(s.data());
  }
}
BENCHMARK(BM_StringConstructSso)->Arg(8)->Arg(22);

// ---- Construct: heap path ----------------------------------------------------

void BM_StringConstructHeap(benchmark::State& state) {
  const std::string src(static_cast<std::size_t>(state.range(0)), 'x');
  for (auto _ : state) {
    vx::String s(src.c_str());
    benchmark::DoNotOptimize(s.data());
  }
}
BENCHMARK(BM_StringConstructHeap)->Arg(64)->Arg(256);

// ---- Append: stays inside SSO buffer -----------------------------------------

void BM_StringAppendSso(benchmark::State& state) {
  for (auto _ : state) {
    vx::String s;
    for (int i = 0; i < 16; ++i) s.append('a');
    benchmark::DoNotOptimize(s.data());
  }
}
BENCHMARK(BM_StringAppendSso);

// ---- Append: triggers SSO->heap and heap regrowth ----------------------------

void BM_StringAppendHeap(benchmark::State& state) {
  for (auto _ : state) {
    vx::String s;
    for (int i = 0; i < 256; ++i) s.append('a');
    benchmark::DoNotOptimize(s.data());
  }
}
BENCHMARK(BM_StringAppendHeap);

// ---- InternedString: each iteration interns a fresh, unique string -----------

void BM_InternedStringInternUnique(benchmark::State& state) {
  vx::InternedString::ClearInternedStrings();
  std::uint64_t i = 0;
  char buf[32];
  for (auto _ : state) {
    int n = std::snprintf(buf, sizeof(buf), "k%llu",
                          static_cast<unsigned long long>(i++));
    auto s = vx::InternedString::Intern(vx::StringView(buf, static_cast<vx::usize>(n)));
    benchmark::DoNotOptimize(s.data());
  }
  vx::InternedString::ClearInternedStrings();
}
BENCHMARK(BM_InternedStringInternUnique);

// ---- InternedString: cache-hit path on the same key --------------------------

void BM_InternedStringInternDup(benchmark::State& state) {
  vx::InternedString::ClearInternedStrings();
  for (auto _ : state) {
    auto s = vx::InternedString::Intern(vx::StringView("hot-key", 7));
    benchmark::DoNotOptimize(s.data());
  }
  vx::InternedString::ClearInternedStrings();
}
BENCHMARK(BM_InternedStringInternDup);

// ---- InternedString: O(1) pointer equality -----------------------------------

void BM_InternedStringEquality(benchmark::State& state) {
  vx::InternedString::ClearInternedStrings();
  auto a = vx::InternedString::Intern(vx::StringView("equality", 8));
  auto b = vx::InternedString::Intern(vx::StringView("equality", 8));
  for (auto _ : state) {
    bool eq = (a == b);
    benchmark::DoNotOptimize(eq);
  }
  vx::InternedString::ClearInternedStrings();
}
BENCHMARK(BM_InternedStringEquality);

}  // namespace

BENCHMARK_MAIN();
