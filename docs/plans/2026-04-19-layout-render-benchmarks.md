# 实现计划：Layout + Render 性能基准

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-05
**复杂度级别：** Level 2-3（4 文件新建 + 1 共享 CMakeLists 修改 + README + 4 baseline JSON）
**关联设计：** `docs/specs/2026-04-19-layout-render-benchmarks-design.md`
**TDD 模式：** 性能基准（覆盖补充模式）— 既有 layout/render GTest 已保证正确性，本次不写新 unit test；每 phase 的"测试通过"判据为 **Release build 零 -Werror + bench 数字非零 + BM 数量与设计一致**。

---

## 0. 全局约束

- **分支**：`feature/TASK-20260419-05-layout-render-benchmarks`（已建，基于 main `2985220`）
- **build 目录**：`build-bench/`（已存在，复用，**无 FetchContent**）
- **构建命令模板**：`cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON`
- **每 phase 必须**：
  1. 提交带 `wip(TASK-20260419-05): phase-X ...` 主题的中间 commit（**禁止外部任务状态字样**，参照 TASK-03 P1）
  2. 更新 `memory-bank/activeContext.md` 进度行
  3. 更新 `memory-bank/progress.md` 增 phase 完成记录
- **所有 BM 命名**：`BM_<Module><Operation><Variant>` 一致风格

---

## Phase 1：CMake 注册 + 4 个 smoke bench（30 分钟）

**目标**：4 个 bench exe 注册成功 + 跑 1 case 各产 1 行非零数字。

### 任务 1.1 — 修改 `benchmarks/CMakeLists.txt`

[共享文件] 在 CSS bench 注册行后追加 4 行：

```cmake
vx_add_benchmark(bench_layout_buildtree bench_layout_buildtree.cc vx_core)
vx_add_benchmark(bench_layout_flex      bench_layout_flex.cc      vx_core)
vx_add_benchmark(bench_render_record    bench_render_record.cc    vx_core)
vx_add_benchmark(bench_render_replay    bench_render_replay.cc    vx_core)
```

**验证**：`cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON 2>&1 | grep "Configuring done"`。

### 任务 1.2 — 4 个 smoke `.cc` 文件

每个文件 ~25 行，结构：

```cpp
#include <benchmark/benchmark.h>
// 按需 include layout / render / dom / css 头

namespace {
static void BM_<Name>Smoke(benchmark::State& state) {
  // setup（不计时）
  for (auto _ : state) {
    // 调一次目标 API
    benchmark::DoNotOptimize(...);
  }
}
BENCHMARK(BM_<Name>Smoke);
}  // namespace

BENCHMARK_MAIN();
```

四个 smoke 各调一次：
- `bench_layout_buildtree.cc` → `LayoutEngine::Layout(empty_doc, ctx)` 一次
- `bench_layout_flex.cc` → 同上 + 1 个 flex 子项
- `bench_render_record.cc` → `Record(layout_root)` 一次
- `bench_render_replay.cc` → `Replay(empty_list, canvas)` 一次

### 验证 1（TDD：bench 通过 = build 零 error + 数字非零）

```bash
cmake --build build-bench --target bench_layout_buildtree bench_layout_flex bench_render_record bench_render_replay -j 2>&1 | tail -10
for b in bench_layout_buildtree bench_layout_flex bench_render_record bench_render_replay; do
  ./build-bench/benchmarks/$b --benchmark_min_time=0.001s 2>&1 | tail -3
done
```

**验收**：4 binary 全跑出 `BM_<Name>Smoke <数字> ns ...` + exit 0。

### 提交 1
```
wip(TASK-20260419-05): phase-1 register 4 smoke benches (layout/render)
```

---

## Phase 2：`benchmarks/layout_corpus.h`（30 分钟）

**目标**：header-only inline corpus，6 个程序化构造 + 6 个 cached getter。

### 任务 2.1 — 新建 `benchmarks/layout_corpus.h`

仿 `benchmarks/css_corpus.h` 结构（mutex-protected static map cache），实现：

```cpp
#ifndef VX_BENCHMARKS_LAYOUT_CORPUS_H_
#define VX_BENCHMARKS_LAYOUT_CORPUS_H_

#include <map>
#include <mutex>

#include "veloxa/core/css/parser.h"
#include "veloxa/core/dom/document.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::bench {

// ----- 构造（每次新建，非缓存版本，单元测试用） -----
inline dom::Document* MakeFlatDocument(int n_children);
inline dom::Document* MakeNestedDocument(int depth);
inline dom::Document* MakeMixedDocument();
inline dom::Document* MakeFlexDocument(int rows, int cols);
inline dom::Document* MakeNestedFlexDocument();
inline dom::Document* MakeImageDocument(int n_images);

// ----- Cached getter（bench setup 用，进程生命周期持有） -----
inline dom::Document& CachedFlatDocument(int n_children);
inline dom::Document& CachedNestedDocument(int depth);
inline dom::Document& CachedMixedDocument();
inline dom::Document& CachedFlexDocument(int rows, int cols);
inline dom::Document& CachedNestedFlexDocument();
inline dom::Document& CachedImageDocument(int n_images);

}  // namespace vx::bench

#endif
```

**关键实现细节**：
- Flex inline declaration：用 `css::CssParser::ParseDeclarationList("display:flex; flex-direction:row")` 拿到 `Vector<css::Declaration>` 后逐条 `element->SetInlineDeclaration(decl)`（API 验证见 `veloxa/core/dom/element.h:46`）
- Image element：`doc.CreateElement(dom::TagId::kImg)` + `element->SetAttribute("src", "dummy.png")`（不真加载，仅触发 layout 走 image 分支判定）
- Mixed document：3 层嵌套 × 4 div/层 × 1 text = 53 节点
- Cache key：`std::pair<Type, int>` 或单 int（按场景）→ `std::map<...>` static + `std::mutex`

### 验证 2

通过 Phase 3-5 的 bench 编译间接验证（header-only 无独立 build target）。本 phase 单独验证：把 corpus include 到 `bench_layout_buildtree.cc` smoke 的 setup 中，确认编译通过。

### 提交 2
```
wip(TASK-20260419-05): phase-2 add layout_corpus.h (DOM programmatic builder + cache)
```

---

## Phase 3：`bench_layout_buildtree.cc` 完整套件（45 分钟）

**目标**：smoke 替换为 9 个完整 BMs（详 design §3.1）。

### 任务 3.1 — 重写 `bench_layout_buildtree.cc`

```cpp
#include <benchmark/benchmark.h>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/layout/text_shaper.h"
#include "veloxa/core/memory/arena.h"

namespace {

vx::layout::LayoutContext MakeCtx(vx::layout::TextShaper* shaper = nullptr) {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.root_font_size = 16.0f;
  ctx.text_shaper = shaper;
  return ctx;
}

static void BM_LayoutBuildTreeFlat(benchmark::State& state) {
  auto& doc = vx::bench::CachedFlatDocument(static_cast<int>(state.range(0)));
  auto ctx = MakeCtx();
  for (auto _ : state) {
    vx::memory::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_LayoutBuildTreeFlat)->RangeMultiplier(2)->Range(8, 512);

static void BM_LayoutBuildTreeNested(benchmark::State& state) {
  auto& doc = vx::bench::CachedNestedDocument(static_cast<int>(state.range(0)));
  auto ctx = MakeCtx();
  for (auto _ : state) {
    vx::memory::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_LayoutBuildTreeNested)->RangeMultiplier(2)->Range(2, 64);

static void BM_LayoutBuildTreeMixed(benchmark::State& state) {
  auto& doc = vx::bench::CachedMixedDocument();
  vx::layout::SimpleTextShaper shaper;
  auto ctx = MakeCtx(&shaper);
  for (auto _ : state) {
    vx::memory::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
  }
}
BENCHMARK(BM_LayoutBuildTreeMixed);

}  // namespace

BENCHMARK_MAIN();
```

**RangeMultiplier 估算（沿用 TASK-03 公式）**：
- `Flat Range(8, 512)` → `ceil(log2(64)) + 1` = 7 case
- `Nested Range(2, 64)` → `ceil(log2(32)) + 1` = 6 case
- `Mixed` → 1 case
- **总计 14 行 BM 输出**（含 BENCHMARK 行裂分）

### 验证 3
```bash
cmake --build build-bench --target bench_layout_buildtree -j 2>&1 | tail -3
./build-bench/benchmarks/bench_layout_buildtree --benchmark_min_time=0.01s 2>&1 | tee /tmp/bm_buildtree.log
# 验收：14 行 BM_ 输出，全数字非零
grep -c "^BM_" /tmp/bm_buildtree.log    # 应等于 14
```

**否定判据**：若任一 BM 数字 < 100 ns（异常快）→ 怀疑 setup 漏算 / DCE 优化掉 → debug。

### 提交 3
```
feat(bench): add bench_layout_buildtree full suite (TASK-20260419-05 phase-3)
```

---

## Phase 4：`bench_layout_flex.cc` 完整套件（45 分钟）

**目标**：smoke 替换为 6 个 BMs（design §3.2），用 `BENCHMARK_TEMPLATE` 二维点。

### 任务 4.1 — 重写 `bench_layout_flex.cc`

```cpp
#include <benchmark/benchmark.h>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/memory/arena.h"

namespace {

vx::layout::LayoutContext MakeFlexCtx() {
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 800;
  ctx.viewport_height = 600;
  ctx.root_font_size = 16.0f;
  return ctx;
}

template <int Rows, int Cols>
static void BM_LayoutFlex(benchmark::State& state) {
  auto& doc = vx::bench::CachedFlexDocument(Rows, Cols);
  auto ctx = MakeFlexCtx();
  for (auto _ : state) {
    vx::memory::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
  }
  state.SetItemsProcessed(state.iterations() * Rows * Cols);
  state.SetLabel(std::to_string(Rows) + "x" + std::to_string(Cols));
}
BENCHMARK_TEMPLATE(BM_LayoutFlex, 1, 8);
BENCHMARK_TEMPLATE(BM_LayoutFlex, 1, 32);
BENCHMARK_TEMPLATE(BM_LayoutFlex, 1, 128);
BENCHMARK_TEMPLATE(BM_LayoutFlex, 8, 8);
BENCHMARK_TEMPLATE(BM_LayoutFlex, 16, 16);

static void BM_LayoutFlexNestedFlex(benchmark::State& state) {
  auto& doc = vx::bench::CachedNestedFlexDocument();
  auto ctx = MakeFlexCtx();
  for (auto _ : state) {
    vx::memory::ArenaAllocator arena;
    auto* root = vx::layout::LayoutEngine::Layout(&doc, ctx, arena);
    benchmark::DoNotOptimize(root);
  }
}
BENCHMARK(BM_LayoutFlexNestedFlex);

}  // namespace

BENCHMARK_MAIN();
```

### 验证 4
```bash
cmake --build build-bench --target bench_layout_flex -j 2>&1 | tail -3
./build-bench/benchmarks/bench_layout_flex --benchmark_min_time=0.01s 2>&1 | tee /tmp/bm_flex.log
grep -c "^BM_" /tmp/bm_flex.log    # 应等于 6
```

**否定判据**：若 16x16 比 8x8 慢得不到 4x（线性预期）→ 怀疑 cache miss / 退化算法 → progress.md 记录数字交反思。

### 提交 4
```
feat(bench): add bench_layout_flex 2D matrix suite (TASK-20260419-05 phase-4)
```

---

## Phase 5：`bench_render_record.cc` 完整套件（30 分钟）

**目标**：smoke 替换为 5 个 BMs（design §3.3），含 1 个 ImgVsNoImg 对比项。

### 任务 5.1 — 重写 `bench_render_record.cc`

```cpp
#include <benchmark/benchmark.h>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/core/memory/arena.h"
#include "veloxa/core/render/renderer.h"

namespace {

struct LaidOut {
  vx::memory::ArenaAllocator arena;
  vx::layout::LayoutBox* root;
};

// 不缓存 layout 输出（arena 复杂），每次重新 layout（比 Record 慢 N 倍，但受 Phase 3 数字担保仍 << 测试时间）
LaidOut LayoutFlat(int n) {
  LaidOut out;
  vx::layout::LayoutContext ctx;
  ctx.viewport_width = 800; ctx.viewport_height = 600; ctx.root_font_size = 16.0f;
  auto& doc = vx::bench::CachedFlatDocument(n);
  out.root = vx::layout::LayoutEngine::Layout(&doc, ctx, out.arena);
  return out;
}
// 同上 LayoutMixed / LayoutImage / LayoutText（按需）

static void BM_RecordSmallTree(benchmark::State& state) {
  auto laid = LayoutFlat(8);
  for (auto _ : state) {
    auto list = vx::render::Record(laid.root);
    benchmark::DoNotOptimize(list);
  }
}
BENCHMARK(BM_RecordSmallTree);
// ... BM_RecordMediumTree (64), BM_RecordLargeTree (512)
// BM_RecordTextHeavy (32 + text + SimpleTextShaper)
// BM_RecordImgVsNoImg/Img (16 包含 8 img)

}  // namespace

BENCHMARK_MAIN();
```

### 验证 5
```bash
cmake --build build-bench --target bench_render_record -j 2>&1 | tail -3
./build-bench/benchmarks/bench_render_record --benchmark_min_time=0.01s 2>&1 | tee /tmp/bm_record.log
grep -c "^BM_" /tmp/bm_record.log    # 应等于 5
```

**否定判据**：若 LargeTree 比 SmallTree 慢小于 50x（Record 是 O(N) 的 N=64x）→ 怀疑 DCE 优化或 corpus 错误。

### 提交 5
```
feat(bench): add bench_render_record full suite + img comparison (TASK-20260419-05 phase-5)
```

---

## Phase 6：`bench_render_replay.cc` 完整套件 + ImageCache 对比（45 分钟）

**目标**：smoke 替换为 6 个 BMs（含 ImgVsNoImg/NoCache + ImgVsNoImg/Cache 对比项）。

### 任务 6.1 — 探查 ImageCache API

[影响后续步骤] 在写 BM 前先 grep `class ImageCache` / `ImageCache::Load`，确认能否构造 dummy image 实例。**预算 15 分钟**：
- ✅ API 可用 → 实现完整 6 BMs
- ❌ API 复杂（>30min） → 退化：仅写 5 BMs（删 ImgVsNoImg/Cache），把 cache 测量推到 TASK-09 候选

### 任务 6.2 — 重写 `bench_render_replay.cc`

```cpp
#include <benchmark/benchmark.h>

#include "benchmarks/layout_corpus.h"
#include "veloxa/core/layout/layout_engine.h"
#include "veloxa/core/render/renderer.h"
#include "veloxa/graphics/software/software_canvas.h"

namespace {

// 800x600 RGBA, 静态分配避免 BM hot loop 申请
constexpr vx::u32 kW = 800, kH = 600;
static vx::u32 g_pixels[kW * kH] = {};

vx::gfx::Canvas& Canvas() {
  static vx::graphics::software::SoftwareCanvas canvas(g_pixels, kW, kH, kW);
  return canvas;
}

vx::render::DisplayList RecordOf(int n_flat) { /* layout + Record */ }

static void BM_ReplaySmallList(benchmark::State& state) {
  auto list = RecordOf(8);
  auto& canvas = Canvas();
  for (auto _ : state) {
    vx::render::Replay(list, &canvas);
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(list.size()));
}
BENCHMARK(BM_ReplaySmallList);
// ... Medium, Large, TextHeavy, ImgVsNoImg/NoCache, ImgVsNoImg/Cache（如 6.1 通过）

}  // namespace

BENCHMARK_MAIN();
```

### 验证 6（含 ImageCache 判定）
```bash
cmake --build build-bench --target bench_render_replay -j 2>&1 | tail -3
./build-bench/benchmarks/bench_render_replay --benchmark_min_time=0.01s 2>&1 | tee /tmp/bm_replay.log
grep -c "^BM_" /tmp/bm_replay.log    # 应等于 5 或 6
```

**ImageCache 判定**（写入 `progress.md`）：
- 计算 `t(ImgVsNoImg/Cache) / t(ImgVsNoImg/NoCache)` 比值
- ≥ 5x：ImageCache 是 hot path → P1 follow-up
- < 5x：非 hot path → P3 / 不立项（与 TASK-03 cluster 阈值一致）

### 提交 6
```
feat(bench): add bench_render_replay full suite + ImageCache comparison (TASK-20260419-05 phase-6)
```

---

## Phase 7：README + baseline JSON + Memory Bank 收尾（30 分钟）

**目标**：4 baseline JSON 入仓 + README 更新 + techContext.md 沉淀性能数据。

### 任务 7.1 — 生成 4 个 baseline JSON

```bash
mkdir -p benchmarks/baseline
for b in bench_layout_buildtree bench_layout_flex bench_render_record bench_render_replay; do
  ./build-bench/benchmarks/$b \
    --benchmark_format=json \
    --benchmark_repetitions=3 \
    --benchmark_report_aggregates_only=true \
    > benchmarks/baseline/$b.json
done
```

### 任务 7.2 — 更新 `benchmarks/baseline/README.md`

[共享文件] 在 CSS 章节后追加 Layout / Render 章节，每段含：
- bench 列表与 BM 简介
- 失真兜底 4 piece（沿用 TASK-03 模板）
- 更新协议 + 比较命令模板

### 任务 7.3 — 更新 `benchmarks/README.md`

[共享文件] 在 CSS 段后追加 Layout + Render 章节（数字从 baseline JSON 摘要）。

### 任务 7.4 — 更新 `memory-bank/techContext.md`

[共享文件] 在「CSS 性能基线」段后追加：
- 「Layout 性能基线」段（buildtree flat ns/box、flex ns/cell、嵌套 flex 总耗时）
- 「Render 性能基线」段（record ns/cmd、replay ns/cmd、ImgVsNoImg cache 判定结果）

### 任务 7.5 — 更新 `memory-bank/systemPatterns.md`

如 Phase 6 ImageCache 判定为 hot path → 沉淀「跨子库 cache 性能侧验证模式」。

### 验证 7（完成验证 — 来自 verification.mdc）
```bash
# 1. Release 全 11 bench 共存
cmake --build build-bench -j --target bench_allocators bench_containers bench_hash_map bench_strings bench_css_tokenizer bench_css_parser bench_css_property_lookup bench_layout_buildtree bench_layout_flex bench_render_record bench_render_replay 2>&1 | tail -5
for b in build-bench/benchmarks/bench_*; do "$b" --benchmark_min_time=0.001s > /dev/null && echo "OK $b" || echo "FAIL $b"; done

# 2. Debug ctest 全绿（不变）
cd build && ctest -j --output-on-failure 2>&1 | tail -5

# 3. baseline 入仓校验
git ls-files benchmarks/baseline/bench_layout_*.json benchmarks/baseline/bench_render_*.json | wc -l    # 应 = 4
```

### 任务 7.6 — Memory Bank 收尾 + 最终提交
- `memory-bank/activeContext.md`：阶段保留 `构建中`，等 `/reflect` 改 `回顾中`
- `memory-bank/progress.md`：phase-7 完成 + 11 bench 共存验证结果
- `memory-bank/tasks.md`：所有验收标准打 ✅

### 提交 7
```
docs(bench): add 4 layout/render baselines + tech-context perf section (TASK-20260419-05 phase-7)
```

---

## 任务总览

| Phase | 时间 | 文件变更 | BM 数 | 提交 |
|-------|------|---------|------|-----|
| 1 | 30 min | CMake + 4 smoke .cc | 4 | wip phase-1 |
| 2 | 30 min | layout_corpus.h | 0 | wip phase-2 |
| 3 | 45 min | bench_layout_buildtree.cc | 9 → 14 行 | feat phase-3 |
| 4 | 45 min | bench_layout_flex.cc | 6 | feat phase-4 |
| 5 | 30 min | bench_render_record.cc | 5 | feat phase-5 |
| 6 | 45 min | bench_render_replay.cc | 5-6 | feat phase-6 |
| 7 | 30 min | README + baseline + MB | 0 | docs phase-7 |
| **合计** | **~4.25h** | **8 文件** | **~25 BM** | **7 commits** |

---

## 风险与中止判据

| Phase | 中止判据 | 处置 |
|-------|---------|------|
| 1 | smoke build 出现非 IPA 类型 -Werror | 暂停 `/build`，立 TASK-09 |
| 2 | layout_corpus 写法触发 TASK-04 / TASK-07 类似 IPA 误报 | 用 `[[gnu::noinline]]` 兜底（不立新任务）|
| 3-4 | 任一 BM 数字 < 100ns 或全相同 | 怀疑 DCE，加 `benchmark::DoNotOptimize(arena)` |
| 5-6 | Layout 在 Record/Replay setup 中失败（segfault） | 退化 — Record/Replay 用更小 corpus，progress.md 记录原因 |
| 6 | ImageCache API 探查 > 30 min | 退化为 5 BMs，把 ImgVsNoImg/Cache 推到 TASK-09 |
| 7 | Debug ctest 出现 regression | 立即停止，git bisect |

---

## TASK-03 模式复刻清单

| 模式 | 应用位置 |
|-----|---------|
| `vx_add_benchmark` 三参（含额外 link） | Phase 1 CMake |
| `RangeMultiplier(m)->Range(lo,hi)` 计数公式 ceil(log_m(hi/lo))+1 | Phase 3-4 验收 |
| 程序化 corpus + mutex-protected static cache | Phase 2 layout_corpus.h |
| 一文件一 .cc 一 exe | 4 bench 文件 |
| Release + 独立 build-bench/ | Phase 0-7 全程 |
| 带否定判据的发现型 phase（cluster / hot path 阈值 5x） | Phase 6 ImageCache 判定 |
| baseline JSON + 4-piece 失真兜底 README | Phase 7 |
| WIP commit subject 不含外部状态 | 全 7 commit |
| 完成验证用证据（命令输出） | Phase 7 验证 7 |

---

## 验收标准（来自 design §9）

逐项 Phase 7 验证 7 完成后打 ✅，写入 `memory-bank/tasks.md`。
