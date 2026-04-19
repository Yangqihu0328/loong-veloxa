# 实现计划：CSS 解析性能基准

**任务 ID：** TASK-20260419-03
**复杂度：** Level 2
**设计规格：** `docs/specs/2026-04-19-css-benchmarks-design.md`
**功能分支：** `feature/TASK-20260419-03-css-benchmarks`（已基于 main `9482fb5` 创建）
**预估总时长：** ~2 小时
**预估提交：** 7 个（plan × 1 + 6 phase × 1，phase 7 为收尾合并入最后一个 phase 提交）

---

## TDD 模式声明

**覆盖补充模式**（与 TASK-02 一致）：
- 不新增 GTest，正确性靠 `tests/core/css/{tokenizer,parser,property}_test.cc` 既有测试
- 每 phase 验收 = 编译过 + 跑出非零数字 + BM 行计数 ≥ 设计值
- 在 `activeContext.md` 标注 `TDD 模式：覆盖补充`

---

## Phase 0 — 基线验证（无提交）

**目标：** 确认起点干净 + Release build dir 通路打通。

**任务 0.1：分支与工作区**
```bash
git rev-parse --abbrev-ref HEAD     # 期望：feature/TASK-20260419-03-css-benchmarks
git status                          # 期望：working tree clean
git log --oneline -1                # 期望：分支头是 9482fb5 (main)
```

**任务 0.2：基线 GTest（Debug）**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DVX_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
# 期望：890/890 passed, 0 warnings
```

**任务 0.3：Release bench build dir 通路（不构建任何新东西）**
```bash
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON
cmake --build build-bench --target bench_allocators -j
./build-bench/benchmarks/bench_allocators --benchmark_min_time=0.01s
# 期望：无 "***WARNING*** Library was built as DEBUG"；至少打印 13 行 BM
```

**任务 0.4：代理验证（如需 FetchContent 重新拉取，本任务无需，因 google/benchmark 已在 build-bench/ 缓存）**
```bash
ls build-bench/_deps/benchmark-src/include/benchmark/benchmark.h
# 期望：存在
```

**验收：** 全 4 项通过。
**提交：** 0 提交（仅验证）。
**Memory Bank：** `activeContext.md` 阶段：规划中 → 构建中；标注 `TDD 模式：覆盖补充`。

---

## Phase 1 — CMake 扩展 + css_corpus.h + 3 个 smoke .cc

**目标：** 把"3 个 CSS bench 能编 + 能跑"打通，每个 exe 仅 1 个最小 BM。

### 任务 1.1：升级 `benchmarks/CMakeLists.txt` [共享文件]

**修改：** `benchmarks/CMakeLists.txt`

```cmake
# benchmarks/CMakeLists.txt
#
# Foundation + CSS performance benchmarks. Enabled via -DVX_BUILD_BENCHMARKS=ON
# (see root CMakeLists.txt). Each .cc file becomes its own executable; add
# more with vx_add_benchmark(<name> [extra_lib1 extra_lib2 ...]).

# Mark Google Benchmark headers as SYSTEM on every consumer so the strict
# -Werror -Wpedantic from the root CMakeLists.txt does not flag third-party
# code we cannot fix. `benchmark::benchmark` is an ALIAS — operate on the
# real target `benchmark`.
get_target_property(_bench_includes benchmark
                    INTERFACE_INCLUDE_DIRECTORIES)
if(_bench_includes)
  set_target_properties(benchmark PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_bench_includes}")
endif()

# vx_add_benchmark(<name> [extra_lib1 extra_lib2 ...])
#   Adds a benchmark executable from <name>.cc and links vx_foundation +
#   benchmark::benchmark plus any extra libraries passed via ARGN (e.g.
#   vx_core for CSS benches that need parser/tokenizer symbols).
function(vx_add_benchmark name)
  set(extra_libs ${ARGN})
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} PRIVATE
    vx_foundation
    benchmark::benchmark
    ${extra_libs})
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks")
endfunction()

# Foundation benches (TASK-20260419-02): single arg form is back-compatible.
vx_add_benchmark(bench_allocators)
vx_add_benchmark(bench_containers)
vx_add_benchmark(bench_hash_map)
vx_add_benchmark(bench_strings)

# CSS benches (TASK-20260419-03): need vx_core for tokenizer/parser/property symbols.
vx_add_benchmark(bench_css_tokenizer        vx_core)
vx_add_benchmark(bench_css_parser           vx_core)
vx_add_benchmark(bench_css_property_lookup  vx_core)
```

**变更影响：**
- [共享文件] 修改 `vx_add_benchmark` 函数签名为可变参数；既有 4 个 Foundation 调用以单参形式调用，`ARGN` 为空 → 完全向后兼容
- [影响前序测试] 否（benchmarks 不参与 GTest）

### 任务 1.2：新建 `benchmarks/css_corpus.h`

```cpp
// benchmarks/css_corpus.h
//
// Deterministic CSS corpus generators for CSS benchmarks. All synthesized
// strings are lazily built and cached behind a static map keyed by their
// generation parameters; subsequent calls hand back the same const reference.
// This keeps generation cost outside the benchmark inner loop entirely.
//
// Usage:
//   const std::string& css = vx::bench::StylesheetCorpus(20, 5);
//   for (auto _ : state) {
//     CssParser::Parse(StringView(css.data(), css.size()));
//   }

#ifndef VELOXA_BENCHMARKS_CSS_CORPUS_H_
#define VELOXA_BENCHMARKS_CSS_CORPUS_H_

#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace vx::bench {

namespace detail {

inline const char* kDeclPool[] = {
    "padding: 10px",
    "margin: 5px",
    "color: #336699",
    "background-color: #ffffff",
    "border: 1px solid #cccccc",
    "display: flex",
    "flex-direction: row",
    "justify-content: center",
    "width: 100%",
    "height: 32px",
    "font-size: 14px",
    "line-height: 1.5",
    "text-align: left",
    "opacity: 0.95",
    "z-index: 10",
};
inline constexpr int kDeclPoolSize =
    sizeof(kDeclPool) / sizeof(kDeclPool[0]);

inline std::string BuildStylesheet(int rules, int decls_per_rule) {
  std::string out;
  out.reserve(static_cast<std::size_t>(rules) * 96 +
              static_cast<std::size_t>(rules * decls_per_rule) * 32);
  for (int i = 0; i < rules; ++i) {
    out += ".cls";
    out += std::to_string(i);
    out += " { ";
    for (int j = 0; j < decls_per_rule; ++j) {
      out += kDeclPool[(i + j) % kDeclPoolSize];
      out += "; ";
    }
    out += "}\n";
  }
  return out;
}

inline std::string BuildInlineStyle(int decls) {
  std::string out;
  out.reserve(static_cast<std::size_t>(decls) * 32);
  for (int i = 0; i < decls; ++i) {
    out += kDeclPool[i % kDeclPoolSize];
    if (i + 1 < decls) out += "; ";
  }
  return out;
}

}  // namespace detail

inline const std::string& StylesheetCorpus(int rules, int decls_per_rule) {
  using Key = std::pair<int, int>;
  static std::map<Key, std::string> cache;
  static std::mutex mu;
  std::lock_guard<std::mutex> g(mu);
  Key k{rules, decls_per_rule};
  auto it = cache.find(k);
  if (it == cache.end()) {
    it = cache.emplace(k, detail::BuildStylesheet(rules, decls_per_rule)).first;
  }
  return it->second;
}

inline const std::string& InlineStyleCorpus(int decls) {
  static std::map<int, std::string> cache;
  static std::mutex mu;
  std::lock_guard<std::mutex> g(mu);
  auto it = cache.find(decls);
  if (it == cache.end()) {
    it = cache.emplace(decls, detail::BuildInlineStyle(decls)).first;
  }
  return it->second;
}

inline const std::vector<std::string>& AllPropertyNames() {
  static const std::vector<std::string> names = {
      "display", "position", "top", "right", "bottom", "left",
      "width", "height", "min-width", "min-height", "max-width", "max-height",
      "margin-top", "margin-right", "margin-bottom", "margin-left",
      "padding-top", "padding-right", "padding-bottom", "padding-left",
      "box-sizing", "overflow", "z-index",
      "flex-direction", "flex-wrap", "justify-content", "align-items",
      "align-self", "flex-grow", "flex-shrink", "flex-basis", "gap",
      "background-color", "color", "opacity",
      "border-top-width", "border-right-width", "border-bottom-width", "border-left-width",
      "border-top-style", "border-right-style", "border-bottom-style", "border-left-style",
      "border-top-color", "border-right-color", "border-bottom-color", "border-left-color",
      "border-radius", "visibility",
      "font-family", "font-size", "font-weight", "font-style",
      "line-height", "text-align", "white-space", "letter-spacing",
      "transition-property", "transition-duration", "transition-timing-function", "transition-delay",
  };
  return names;
}

inline const std::vector<std::string>& Hot5PropertyNames() {
  static const std::vector<std::string> names = {
      "display", "color", "width", "padding-top", "margin-top",
  };
  return names;
}

inline const std::vector<std::string>& MissPropertyNames() {
  static const std::vector<std::string> names = {
      "-webkit-transform", "moz-foo", "color2", "wid", "marg-top",
  };
  return names;
}

}  // namespace vx::bench

#endif  // VELOXA_BENCHMARKS_CSS_CORPUS_H_
```

**变更影响：**
- 仅本任务三 CSS bench 文件 include，不污染其它 4 个 Foundation bench
- header-only inline，无 .cc，不需要单独加进 CMakeLists

### 任务 1.3：新建 3 个 smoke `.cc`

#### `benchmarks/bench_css_tokenizer.cc`（smoke）

```cpp
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
```

#### `benchmarks/bench_css_parser.cc`（smoke）

```cpp
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
```

#### `benchmarks/bench_css_property_lookup.cc`（smoke）

```cpp
// benchmarks/bench_css_property_lookup.cc
//
// CSS PropertyIdFromName benchmarks. Phase 1 smoke — full BM suite + cluster
// probes added in Phase 4.

#include <benchmark/benchmark.h>

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
```

### 任务 1.4：编译验证

```bash
cmake --build build-bench --target \
  bench_css_tokenizer bench_css_parser bench_css_property_lookup -j

./build-bench/benchmarks/bench_css_tokenizer        --benchmark_min_time=0.05s
./build-bench/benchmarks/bench_css_parser           --benchmark_min_time=0.05s
./build-bench/benchmarks/bench_css_property_lookup  --benchmark_min_time=0.05s
# 期望：每个 exe 至少打印 1 行 BM；无 -Werror 失败；exit 0
```

### 任务 1.5：提交

```bash
git add benchmarks/CMakeLists.txt \
        benchmarks/css_corpus.h \
        benchmarks/bench_css_tokenizer.cc \
        benchmarks/bench_css_parser.cc \
        benchmarks/bench_css_property_lookup.cc
git commit -m "feat(benchmarks): scaffold CSS bench targets [TASK-20260419-03 P1]

- Extend vx_add_benchmark() to accept extra link libs via ARGN; back-compat
  for the 4 existing Foundation single-arg calls.
- Add benchmarks/css_corpus.h: deterministic, cached CSS string + property
  name generators (header-only, mutex-protected static cache).
- Smoke-only: bench_css_tokenizer (1 BM), bench_css_parser (1 BM),
  bench_css_property_lookup (1 BM with PropertyMap warm-up).
- Full BM suites land in P2/P3/P4."
```

**Phase 1 验收：** 3 个 CSS exe 编译通过 + 跑出 1 BM 行 + 0 -Werror 失败。

---

## Phase 2 — bench_css_tokenizer.cc 完整 ~10 BM

### 任务 2.1：替换 smoke 为完整套件

**完整替换 `benchmarks/bench_css_tokenizer.cc`：**

```cpp
// benchmarks/bench_css_tokenizer.cc
//
// CSS tokenizer benchmarks for vx::css::CssTokenizer.
// Run: ./build-bench/benchmarks/bench_css_tokenizer
//
// Coverage:
//   BM_TokenizeAll      — generated stylesheet, byte-range sweep
//   BM_TokenizeNumeric  — declaration list dominated by lengths/numbers
//   BM_TokenizeString   — declaration list dominated by quoted strings
//   BM_TokenizeWhitespace — input padded with whitespace + comments

#include <benchmark/benchmark.h>

#include <cstddef>
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
// stylesheet (rules x decls) whose serialized length covers it. RangeMultiplier
// is set to 2 so we get ceil(log2(4096/64))+1 = 7 sample points.

void BM_TokenizeAll(benchmark::State& state) {
  const auto target_bytes = static_cast<std::size_t>(state.range(0));
  // Each rule of 5 decls is roughly ~140 bytes; pick rule count to match.
  const int rules = static_cast<int>((target_bytes + 139) / 140);
  const std::string& css = vx::bench::StylesheetCorpus(rules, 5);
  vx::StringView sv(css.data(), css.size());
  std::size_t bytes = 0;
  for (auto _ : state) {
    vx::css::CssTokenizer t(sv);
    DrainTokens(t);
    bytes += sv.size();
  }
  state.SetBytesProcessed(static_cast<int64_t>(bytes));
}
BENCHMARK(BM_TokenizeAll)->RangeMultiplier(2)->Range(64, 4096);

// ---- Numeric-heavy ----------------------------------------------------------

void BM_TokenizeNumericHeavy(benchmark::State& state) {
  // Build once (outside the loop): 200 numeric declarations.
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
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(sv.size()));
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
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(sv.size()));
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
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(sv.size()));
}
BENCHMARK(BM_TokenizeWhitespaceHeavy);

}  // namespace

BENCHMARK_MAIN();
```

### 任务 2.2：编译 + 验收

```bash
cmake --build build-bench --target bench_css_tokenizer -j
./build-bench/benchmarks/bench_css_tokenizer --benchmark_min_time=0.05s
# 期望：BM 行 ≥ 7 (Range) + 3 (single) = 10 行；都打印非零 ns/op；exit 0
```

### 任务 2.3：提交

```bash
git add benchmarks/bench_css_tokenizer.cc
git commit -m "feat(benchmarks): CSS tokenizer suite [TASK-20260419-03 P2]

10 BM cases over CssTokenizer::Next():
- BM_TokenizeAll/Range(64,4096) RangeMultiplier=2 -> 7 byte-size samples
- BM_TokenizeNumericHeavy / StringHeavy / WhitespaceHeavy -> 3 fixed shapes

All BMs use SetBytesProcessed for throughput readout."
```

---

## Phase 3 — bench_css_parser.cc 完整 ~10 BM

### 任务 3.1：替换 smoke 为完整套件

**完整替换 `benchmarks/bench_css_parser.cc`：**

```cpp
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
//   BM_ParseSelectorListMixed — selector-heavy stylesheet (compound + combos)

#include <benchmark/benchmark.h>

#include <cstddef>
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
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(sv.size()));
}
BENCHMARK_TEMPLATE(BM_ParseStylesheet, 2, 5)->Name("BM_ParseStylesheetSmall");
BENCHMARK_TEMPLATE(BM_ParseStylesheet, 20, 5)->Name("BM_ParseStylesheetMedium");
BENCHMARK_TEMPLATE(BM_ParseStylesheet, 200, 5)->Name("BM_ParseStylesheetLarge");
BENCHMARK_TEMPLATE(BM_ParseStylesheet, 20, 20)->Name("BM_ParseStylesheetWide");

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
// Range count = ceil(log2(32/1))+1 = 6.

void BM_ParseSelectorListMixed(benchmark::State& state) {
  // 50 rules, each with a 3-component compound + descendant combinator.
  // Synthesized inline (small enough not to need the cache).
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
```

### 任务 3.2：编译 + 验收

```bash
cmake --build build-bench --target bench_css_parser -j
./build-bench/benchmarks/bench_css_parser --benchmark_min_time=0.05s
# 期望：BM 行 = 4 (template) + 6 (Range) + 1 (selector) = 11 行
```

### 任务 3.3：提交

```bash
git add benchmarks/bench_css_parser.cc
git commit -m "feat(benchmarks): CSS parser suite [TASK-20260419-03 P3]

11 BM cases over CssParser::Parse() and ::ParseDeclarationList():
- ParseStylesheet Small/Medium/Large/Wide (2x5 / 20x5 / 200x5 / 20x20)
- ParseDeclarationListInline/Range(1,32) -> 6 inline-style decl-count samples
- ParseSelectorListMixed (50 compound + descendant-combinator rules)"
```

---

## Phase 4 — bench_css_property_lookup.cc 完整 ~10 BM（含 cluster 度量）

### 任务 4.1：替换 smoke 为完整套件

**完整替换 `benchmarks/bench_css_property_lookup.cc`：**

```cpp
// benchmarks/bench_css_property_lookup.cc
//
// CSS PropertyIdFromName benchmarks.
// Run: ./build-bench/benchmarks/bench_css_property_lookup
//
// Coverage:
//   BM_PropertyLookupHitAll      — round-robin all 60 real keys
//   BM_PropertyLookupHitHot5     — round-robin 5 high-frequency keys
//   BM_PropertyLookupHitSingle/* — five fixed single keys
//   BM_PropertyLookupMiss        — round-robin 5 unknown keys
//   BM_BuildPropertyMapInit      — first-call cost (lazy init)
//
// Cluster probe: if HitSingle and HitHot5 differ by >5x, the 60-entry
// HashMap<StringView,PropertyId> in property.cc has uneven probe-distance
// distribution under djb2 hash (parallel to TASK-02 finding for
// std::hash<int>). Records baseline data for TASK-05 candidate.

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

inline void Warmup() {
  benchmark::DoNotOptimize(
      vx::css::PropertyIdFromName(vx::StringView("display", 7)));
}

// ---- Hit: round-robin all keys ---------------------------------------------

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

// ---- Hit: hot 5 keys -------------------------------------------------------

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

// ---- Hit: single fixed key (cluster probe) ---------------------------------

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

// ---- Miss ------------------------------------------------------------------

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

// ---- BuildPropertyMap one-shot init cost -----------------------------------
//
// The first PropertyIdFromName call on process start allocates and fills the
// PropertyMap; subsequent calls are O(1) lookups. We can only measure this
// once per process, so it's just an informational anchor — the BM body
// re-invokes the lookup, but the bulk of the cost is paid in Warmup().

void BM_BuildPropertyMapInit(benchmark::State& state) {
  Warmup();  // pays the build cost on iteration 0 of the program lifetime
  for (auto _ : state) {
    auto id = vx::css::PropertyIdFromName(vx::StringView("z-index", 7));
    benchmark::DoNotOptimize(id);
  }
}
BENCHMARK(BM_BuildPropertyMapInit);

}  // namespace

BENCHMARK_MAIN();
```

### 任务 4.2：编译 + 验收

```bash
cmake --build build-bench --target bench_css_property_lookup -j
./build-bench/benchmarks/bench_css_property_lookup --benchmark_min_time=0.05s
# 期望：BM 行 = 1 (HitAll) + 1 (HitHot5) + 5 (HitSingle) + 1 (Miss) + 1 (Init) = 9 行
```

### 任务 4.3：提交

```bash
git add benchmarks/bench_css_property_lookup.cc
git commit -m "feat(benchmarks): CSS property lookup + cluster probe [TASK-20260419-03 P4]

9 BM cases over PropertyIdFromName():
- HitAll (rotate 60 real keys), HitHot5 (5 frequent keys)
- HitSingle x 5 fixed keys (display/color/margin-top/border-radius/
  transition-timing-function) -- if any differs from HitHot5 by >5x, the
  PropertyMap (HashMap<StringView,PropertyId>, ~60 entries, djb2 hash) has
  uneven probe distribution -- reference data for TASK-05 candidate.
- Miss (rotate 5 unknown keys), BuildPropertyMapInit (anchor for lazy init)."
```

---

## Phase 5 — README + baseline/README（仅文档）

### 任务 5.1：新建 `benchmarks/baseline/README.md`

```markdown
# Baseline JSON 目录

本目录存放 `bench_css_*` 的 Release 单机基线 JSON，由 google/benchmark 的
`--benchmark_format=json --benchmark_out=...` 直接生成。

## ⚠️ 强失真警告 — 必读

> 这些 JSON **不是性能 SLA**，也不是 CI 回归门禁数据。它们是「PR 评审视觉对比锚点」。

数字会随以下任一因素显著漂移：
- 硬件（CPU 型号 / 频率 / 缓存）
- OS 调度器（WSL2 与裸 Linux 表现不同）
- 编译器版本与优化级别
- 同时运行的其它进程

GBench JSON 自带 `context.host_name / num_cpus / mhz_per_cpu /
cpu_scaling_enabled / library_build_type` 元信息；diff 时务必先看这些字段
是否一致，再对比 `real_time / cpu_time`。

## 更新协议

| 触发条件 | 是否需刷新 baseline |
|----------|---------------------|
| `benchmarks/bench_css_*.cc` 自身改动（增删 BM、改循环体） | 是 |
| `benchmarks/css_corpus.h` 改动（生成器算法变化） | 是 |
| 涉及 `vx::css::CssTokenizer / CssParser / PropertyIdFromName` 实现优化 | 是（在 PR 中附旧/新对比） |
| 仅文档 / 无关模块改动 | 否 |
| 切换硬件 / 编译器 | 否（不应替换基线，应在新机器本地重新生成对比用） |

## 生成命令

```bash
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON
cmake --build build-bench --target \
  bench_css_tokenizer bench_css_parser bench_css_property_lookup -j

for b in bench_css_tokenizer bench_css_parser bench_css_property_lookup; do
  ./build-bench/benchmarks/$b \
    --benchmark_format=json \
    --benchmark_out=benchmarks/baseline/$b.json \
    --benchmark_min_time=0.5s
done
```

## 对比命令

```bash
python3 build-bench/_deps/benchmark-src/tools/compare.py benchmarks \
  benchmarks/baseline/bench_css_parser.json \
  /tmp/new_run.json
```

## 当前生成环境

| 字段 | 值 |
|------|---|
| 主机 | （首次提交时由提交者从 JSON `context.host_name` 字段填入） |
| OS | WSL2 (Linux 6.6.x) |
| CPU | （由提交者从 `context.num_cpus / mhz_per_cpu` 填入） |
| 编译器 | （由提交者从 `g++ --version` 填入） |
| 构建类型 | Release（验证 JSON `context.library_build_type == "release"`） |
```

### 任务 5.2：修改 `benchmarks/README.md`

修改要点：
1. **删除第 79 行** `> **本仓库不提交基线 JSON。** ...` 整段
2. **「可执行文件」表扩展**到 7 行（追加 `bench_css_tokenizer` / `bench_css_parser` / `bench_css_property_lookup`）
3. **新增 `## CSS 基准与基线对比` 章节** 在「JSON 导出 + 对比」之后，「结果说明 / 已知量级」之前
4. **「已知量级」表追加 CSS 行** 4–5 条
5. **「注意事项」追加** PropertyMap 懒初始化与 cluster 解读

具体文本（替换 `## JSON 导出 + 对比` 整段及之后到 `## 结果说明` 之前）：

```markdown
## JSON 导出 + 对比

google/benchmark 自带 JSON 输出与 `compare.py` 对比工具：

```bash
./build-bench/benchmarks/bench_css_parser \
  --benchmark_format=json \
  --benchmark_out=/tmp/new.json

python3 build-bench/_deps/benchmark-src/tools/compare.py benchmarks \
  benchmarks/baseline/bench_css_parser.json \
  /tmp/new.json
```

## CSS 基准与基线对比

CSS 三个 exe 的基线 JSON 入仓 `benchmarks/baseline/`，作为 PR 评审的视觉
对比锚点（**非 SLA、非 CI 门禁**）。详见 `benchmarks/baseline/README.md`
的失真警告与更新协议。

| 触发条件 | 是否需刷新 baseline |
|----------|---------------------|
| `bench_css_*.cc` 自身或 `css_corpus.h` 改动 | 是 |
| `vx::css` 实现优化（在 PR 附对比） | 是 |
| 仅文档 / 无关模块改动 | 否 |

Foundation 4 个 exe 不入仓基线（与 TASK-02 决策一致；其变化频率低、覆盖
点更集中）。
```

「已知量级」表追加（DEBUG WSL 形态参考，将在 Phase 6 跑出 Release 数据后回填）：

```markdown
| `BM_TokenizeAll/4096` | TBD ns | bytes/sec 由 SetBytesProcessed 上报 |
| `BM_ParseStylesheetMedium` (20×5) | TBD µs | 包含 selector + decl 完整路径 |
| `BM_ParseStylesheetLarge` (200×5) | TBD µs | 框架风格规模 |
| `BM_PropertyLookupHitHot5` | TBD ns | 平均探测代价 |
| `BM_PropertyLookupHitSingle/transition-timing-function` | TBD ns | cluster 探针 — 与 Hot5 对比 |
```

「注意事项」追加：

```markdown
- **CSS PropertyMap 懒初始化**：`PropertyIdFromName` 第一次调用会 `BuildPropertyMap()`（一次性 ~60 entry insert）。`bench_css_property_lookup` 在每个 BM 函数体顶部用 `Warmup()` 预先调用 `display` 一次，把建表成本逐到 BM 计时之外。
- **cluster 解读**：如果 `BM_PropertyLookupHitSingle/<key>` 各 key 之间或与 `BM_PropertyLookupHitHot5` 出现 5× 以上的差异，说明 djb2 + `H1=h>>7` 在 60-entry 规模下也产生了不均探测距离 — TASK-05 候选项的实测证据。
```

### 任务 5.3：编译验收（仅文档，不必触发构建，但跑一次确认 README 中命令可执行）

```bash
# 只需文档审查 — 命令在 Phase 6 实际验收
ls benchmarks/baseline/README.md
ls benchmarks/README.md
```

### 任务 5.4：提交

```bash
git add benchmarks/README.md benchmarks/baseline/README.md
git commit -m "docs(benchmarks): CSS section + baseline workflow [TASK-20260419-03 P5]

- benchmarks/README.md: drop 'no baseline files' clause; add CSS section
  with update protocol and 5 placeholder magnitude rows (filled in P6).
- benchmarks/baseline/README.md: distortion warning + update protocol
  + generate/compare commands + per-host metadata template.
- Foundation benches keep no-baseline policy (lower change frequency,
  per TASK-20260419-02 decision)."
```

---

## Phase 6 — 生成 baseline JSON × 3 + 收尾验证

### 任务 6.1：Release 全量构建

```bash
rm -rf build-bench
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON
cmake --build build-bench -j
# 期望：0 警告 0 错误；7 个 bench exe 全部成功；google/benchmark v1.9.1 缓存命中
```

### 任务 6.2：跑 3 个 baseline JSON

```bash
mkdir -p benchmarks/baseline
for b in bench_css_tokenizer bench_css_parser bench_css_property_lookup; do
  ./build-bench/benchmarks/$b \
    --benchmark_format=json \
    --benchmark_out=benchmarks/baseline/$b.json \
    --benchmark_min_time=0.5s
done

# 期望：3 个 JSON 文件生成；每个含 context + benchmarks 数组；
# context.library_build_type == "release"
```

### 任务 6.3：JSON 体检 + 文档回填

```bash
# 验证 build_type
for f in benchmarks/baseline/bench_css_*.json; do
  python3 -c "import json; d=json.load(open('$f')); \
    assert d['context']['library_build_type']=='release', '$f not release'; \
    print('$f OK:', len(d['benchmarks']), 'BM')"
done
# 期望：3 行 OK，BM 计数与设计值匹配
```

回填 `benchmarks/README.md` 「已知量级」表的 5 行 TBD 为实际数字（取 ns/µs 量级，保留个位精度）。

回填 `benchmarks/baseline/README.md` 「当前生成环境」表的 4 行 TBD（host_name / num_cpus / mhz_per_cpu / 编译器版本）。

### 任务 6.4：全量回归验证

```bash
# 1) Debug 全测试无回归
cmake --build build -j
ctest --test-dir build --output-on-failure
# 期望：890/890 passed

# 2) Release bench 全部能跑
for b in bench_allocators bench_containers bench_hash_map bench_strings \
         bench_css_tokenizer bench_css_parser bench_css_property_lookup; do
  ./build-bench/benchmarks/$b --benchmark_min_time=0.01s > /dev/null
  echo "$b: exit $?"
done
# 期望：7 行 "exit 0"
```

### 任务 6.5：依赖安全审计

本任务**未引入新依赖**（google/benchmark v1.9.1 已在 TASK-20260419-02 完成 CVE 审计）→ 跳过此步骤，记录到回顾。

### 任务 6.6：Memory Bank 收尾更新

更新 `memory-bank/techContext.md`「Benchmark 启用」段，追加 baseline JSON 工作流条目（在原 JSON 导出说明后插入）。

更新 `memory-bank/activeContext.md`：
- 阶段保持「构建中」（待 `/reflect` 切换）
- 「构建结果」段写入：7 phase 全绿、3 个新 exe、~30 BM 用例、3 个 baseline JSON 入仓、890/890 测试无回归、cluster 度量结果数据点

### 任务 6.7：提交（含收尾）

```bash
git add benchmarks/baseline/bench_css_tokenizer.json \
        benchmarks/baseline/bench_css_parser.json \
        benchmarks/baseline/bench_css_property_lookup.json \
        benchmarks/README.md \
        benchmarks/baseline/README.md \
        memory-bank/techContext.md \
        memory-bank/activeContext.md \
        memory-bank/progress.md
git commit -m "feat(benchmarks): CSS baselines + finalize [TASK-20260419-03 P6]

- Generate 3 release baseline JSONs (Tokenizer / Parser / PropertyLookup);
  context.library_build_type == release verified.
- Backfill 5 TBD magnitude rows in benchmarks/README.md from real numbers.
- Backfill host metadata in benchmarks/baseline/README.md.
- techContext.md: Benchmark section gains baseline JSON workflow paragraph.

Verification:
- 890/890 GTest pass (Debug build)
- 7/7 bench exe runnable under Release build-bench/
- No new external dep -> CVE audit skipped (google/benchmark v1.9.1 already
  cleared in TASK-20260419-02).
"
```

**Phase 6 验收：**
- 7 phase 全绿
- 3 个 baseline JSON 入仓且元信息正确
- README 数据回填完成
- 全测试无回归
- working tree 干净

**下一步：** `/reflect`

---

## 完成标准（汇总）

来自设计规格 §9：

- [ ] 7 个 phase 全绿（0 ~ 6）
- [ ] 全测试 890/890 通过（无回归）
- [ ] 全量构建 0 警告 0 错误（Debug 与 Release 各一次）
- [ ] 3 个 CSS bench exe 编译通过且各打印 ≥ 设计值的 BM 行
- [ ] 3 个 baseline JSON 入仓且 `context.library_build_type == "release"`
- [ ] `benchmarks/README.md` CSS 章节完整 + baseline 工作流可独立复现
- [ ] cluster 度量数据写入回顾，作为 TASK-05 立项判据

---

## 风险预案

| 风险 | 触发条件 | 应对 |
|------|---------|------|
| Phase 1 编译失败：`vx_core` 依赖链拉入未安装的 PNG/JPEG | `cmake -B build-bench` 时 find_package 失败 | 用 Phase 0 的 build dir 检查（main 已能编 vx_core 进 examples），如果 main 都能过那 build-bench 也能过 |
| Phase 4 cluster 度量数据偏离预期（如 djb2 + 60 entry 实测均匀） | HitSingle 各 key 与 HitHot5 量级差 < 2× | 接受结果；在回顾文档把"PropertyMap 实测均匀，TASK-05 优先级降为 P3"作为产出 |
| Phase 6 baseline JSON 出现极小数（< 1 ns/op） | WSL 时钟分辨率不够 | 提高 `--benchmark_min_time=1.0s` 重跑出问题 BM；不接受 < 1ns 数据 |
| Release 编译时间过长（拉入 quickjs 全量） | 首次 `cmake --build build-bench` > 5 分钟 | 接受；后续增量编译只编 bench exe |

---

**计划冻结。** 进入 Memory Bank 更新与 `/build` 启动。
