# Design: Replay hot path 深度基准 + 真 ImageCache 通路（A+B）

**任务 ID：** TASK-20260419-09
**复杂度：** Level 2-3
**作者：** AI Agent
**日期：** 2026-04-19
**分支：** `feature/TASK-20260419-09-replay-deepbench`（基于 main `bfe44ae`）

## 1. 任务概述

承接 TASK-20260419-05 的 K1（Replay hot path = `DrawText` 8200 ns/cmd vs FillRect 10 ns/cmd = 820×）和 K5（ImageCache 三阶段链断），新增 **2 个 bench exe** + 复用 `layout_corpus.h`，目标量化：

1. **K1 真路径根因证伪/证实**：当前 8200 ns/cmd 是 `DrawText` 的 **fallback 路径**（line 145 `font_manager==nullptr` gate → `DrawTextFallback` 逐字符 FillRect），不是 FreeType+HarfBuzz 真路径；本任务给出 fallback / 真路径 cold / 真路径 warm 三档数据
2. **K5 ImageCache 真路径**：完整 layout→Record→Replay 三阶段同传 cache 的端到端开销 + Load hit/miss 对 cache size 的扩展性

VAN 阶段已 grep 推翻 K1/K5 的两个原假设并简化范围，原候选第三个子目标 C（Layout super-linear 调查）拆为 TASK-20260419-10（研究类，本任务不做）。

## 2. 已确认事实（VAN grep 实证）

| # | API/约束 | 来源 |
|---|---------|------|
| E1 | `SoftwareCanvas::SoftwareCanvas(pixels, w, h, stride, font_manager=nullptr, glyph_cache=nullptr)` 后两参可选 | `software_canvas.h:20-23` |
| E2 | `SoftwareCanvas::DrawText` line 145：`font_manager == nullptr \|\| glyph_cache == nullptr` → `DrawTextFallback`（逐字符 FillRect） | `software_canvas.cc:143-148` |
| E3 | `ImageCache::Load` hit 路径是 **O(N) 线性扫 path 字符串相等**（`for i: images_[i].path == path`），未来 cache size 大时本身可能是 hot path | `image_cache.cc:7-12` |
| E4 | layout image 三阶段链：`tag==kImg && ctx.image_cache!=nullptr` → `Load` → set `image_handle` → Record `box->image_handle != 0` 才 emit kDrawImage → Replay `image_cache && cmd.image_handle != 0` 才 dispatch | `layout_engine.cc:59-77`, `renderer.cc:125-129,179-188` |
| E5 | `layout_corpus.h` 已有 9 个 builder（含 `BuildImageStyled`），但**无带 src 属性的 img builder** — 本任务需新增 `BuildImageWithSrcStyled(doc, n, paths)` |  `layout_corpus.h:95-104` |
| E6 | TTF 已知路径 `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`（`font_manager_test` 已用）；PNG 程序化构造模式 `image_decoder_test::CreateTestPng()` 用 libpng 写 `/tmp/vx_test_<pid>.png` | `tests/text/font_manager_test.cc:21`、`tests/core/image/image_decoder_test.cc:12-41` |
| E7 | CMake `vx_add_benchmark(name [extra_libs...])` 已支持额外链接库；`bench_render_replay` 已链 `vx_core vx_graphics vx_platform`；新 bench 需加链 `vx_text`（GlyphCache/FontManager 来自 vx_text） | `benchmarks/CMakeLists.txt:22-31,49`，`tests/CMakeLists.txt:187` |

## 3. 设计决策（D1-D5，用户确认）

| # | 维度 | 选择 |
|---|------|------|
| D1 | DrawText 层次 | **A1 + A2**：A1 直接 SoftwareCanvas::DrawText 微基准 + A2 端到端 Replay TextHeavy 真 vs fallback 对比 |
| D2 | 文本维度 | 文本长度 3 档（短/中/长）+ **glyph cache cold/warm 各一组** |
| D3 | ImageCache 维度 | B1 Load hit/miss × cache size {1, 16, 256} + B2 端到端真路径 N={16, 64} + B3 Get 单次 |
| D4 | fixture 策略 | **多张 distinct PNG**：bench 进程启动 setup 写 N 张 1×1 PNG 到 `/tmp/vx_bench_<pid>_<i>.png`（N = max cache size = 256） |
| D5 | Phase 划分 | **5 phase**（P1 corpus 扩 + 2 smoke / P2 bench_imagecache 全套 / P3 bench_drawtext 全套 / P4 baseline JSON + README / P5 techContext + MB 收尾） |

## 4. BM 详细设计

### 4.1 bench_drawtext.cc（A 组，6-8 BMs）

**目标 API：** `vx::gfx::sw::SoftwareCanvas::DrawText`（直接调用，不经 Record/Replay）；以及 Replay TextHeavy 真路径变体。

**Setup（process-static，一次）：**
- `FontManager fm; fm.Init(); fm.LoadFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "DejaVu");`
- `GlyphCache gc;`
- 两个 SoftwareCanvas 实例：`canvas_fallback`（无 font_manager）/`canvas_real`（带 font_manager + glyph_cache）

**A1 直接微基准（4 BMs）：**

| BM | 文本 | Canvas | 备注 |
|----|------|--------|------|
| `BM_DrawTextFallback_Short` | "hi"（2 char） | fallback | 基线对比 |
| `BM_DrawTextFallback_Medium` | "The quick brown fox" (19 char) | fallback | K1 当前测的 ~ |
| `BM_DrawTextReal_Cold_Medium` | 同上 | real | 每次 iter 前 `glyph_cache.Clear()`；测 FT_Load_Glyph + FT_Render_Glyph + hb_shape 全成本 |
| `BM_DrawTextReal_Warm_Medium` | 同上 | real | 不清 cache；测 hb_shape + glyph_cache.Get hit + memcpy 摊还成本 |

**A2 文本长度扫描（2-4 BMs，可选 BENCHMARK_TEMPLATE）：**

| BM | 文本 | Canvas |
|----|------|--------|
| `BM_DrawTextReal_Warm_Short` | "hi" | real |
| `BM_DrawTextReal_Warm_Long` | 80 char ASCII | real |
| `BM_ReplayTextHeavyFallback` | TASK-05 同 corpus，canvas 不传 font | fallback（基线对照 K1） |
| `BM_ReplayTextHeavyReal` | 同上 corpus，canvas 带 font + warm cache | real（验证 K1 在真路径下的数值） |

**预期发现：**
- 如果 `BM_DrawTextReal_Warm_Medium << BM_DrawTextFallback_Medium` → K1 误判，真 hot path 是 fallback FillRect 串联
- 如果 `BM_DrawTextReal_Cold_Medium >> BM_DrawTextReal_Warm_Medium` → glyph_cache 是关键优化（FreeType raster 是真贵）
- 如果两者都 ≈ 8200 ns → 真 hot path 是 hb_shape 本身

### 4.2 bench_imagecache.cc（B 组，7-8 BMs）

**目标 API：** `vx::image::ImageCache::Load` / `Get`，以及完整 layout→Record→Replay 真 image 通路。

**Setup（process-static）：**
- `WriteFixturePngs(int n=256)`：用 libpng 写 256 张 1×1 RGBA PNG 到 `/tmp/vx_bench_<pid>_<i>.png`，每张内容相同（红 RGBA(255,0,0,255)）；返回 `std::vector<std::string>` 路径列表
- 进程退出时清理（atexit）

**B1 Load 微基准（4 BMs，BENCHMARK_TEMPLATE 或 Range）：**

| BM | 操作 | cache 状态 |
|----|------|-----------|
| `BM_ImageCacheLoad_Miss` | 每 iter `Load(unique_path)` | cache 持续增长（pre-state 0） |
| `BM_ImageCacheLoad_Hit/1` | 反复 `Load(path_0)` | cache size = 1 |
| `BM_ImageCacheLoad_Hit/16` | 反复 `Load(path_15)`（最坏命中位置） | cache size = 16 |
| `BM_ImageCacheLoad_Hit/256` | 反复 `Load(path_255)`（最坏命中位置） | cache size = 256，验证 O(N) 是否真扩 |

> ⚠️ Miss BM 必须在每 iter 用唯一 path（如 path_<iter_count>），否则第 2 iter 起就变成 hit。Google Benchmark 提供 `state.iterations()` 但不保证 setup-per-iter 的纯净性 → 用 `state.PauseTiming()` / `ResumeTiming()` 包住 path 生成或预生成 path 池。

**B2 端到端真路径（2 BMs）：**

| BM | DOM | Layout/Record/Replay |
|----|------|----|
| `BM_ReplayImageReal/16` | 16 个 `<img src=path_i>`，每张唯一 path | layout(ctx.image_cache=&cache) → Record(&cache) → Replay(canvas, &cache)；cache pre-loaded warm |
| `BM_ReplayImageReal/64` | 64 个 | 同上 |

**B3 Get 微基准（1 BM，可选）：**

| BM | 操作 |
|----|------|
| `BM_ImageCacheGet` | 反复 `cache.Get(handle_random)` — 验证是否真 O(1)（确认是否数组下标） |

**预期发现：**
- B1 Hit/256 vs Hit/1：如果 ~256× 慢 → O(N) 扫描确认是 hot path（推动 Load 用 HashMap 改造的 follow-up）
- B2 Real vs TASK-05 BM_ReplayImgVsNoImg（nullptr 路径）：量化「真路径含解码 + Get」的开销
- B2 Real（N=16）vs A2 BM_ReplayTextHeavyReal：直接对比 K1 命题「ImageCache 真路径仍 < DrawText 真路径」

## 5. fixture 实现策略

### 5.1 PNG fixture（B 用）

```cpp
// 内联到 bench_imagecache.cc，复用 image_decoder_test.cc:12-41 的 libpng 模式
namespace {
struct PngFixturePool {
  std::vector<std::string> paths;
  ~PngFixturePool() { for (auto& p : paths) std::remove(p.c_str()); }
};

PngFixturePool& EnsureFixturePool(int n) {
  static PngFixturePool pool;
  if (static_cast<int>(pool.paths.size()) >= n) return pool;
  for (int i = pool.paths.size(); i < n; ++i) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "/tmp/vx_bench_%d_%d.png", getpid(), i);
    // libpng 写 1x1 RGBA red（同 image_decoder_test 模式，简化为 1x1）
    FILE* fp = std::fopen(buf, "wb");
    png_structp png = png_create_write_struct(...);
    // ... 4 字节红 RGBA ...
    std::fclose(fp);
    pool.paths.emplace_back(buf);
  }
  return pool;
}
}  // namespace
```

`getpid()` 隔离避免并发 bench 进程互踩。`atexit` cleanup 通过 `~PngFixturePool` 自动完成。

### 5.2 Font fixture（A 用）

```cpp
namespace {
constexpr const char* kFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

vx::text::FontManager& SharedFontManager() {
  static vx::text::FontManager fm;
  static bool inited = []() {
    fm.Init();
    auto r = fm.LoadFont(kFontPath, "DejaVu");
    if (!r.ok()) {
      std::fprintf(stderr, "FATAL: cannot load %s — bench requires this font\n",
                   kFontPath);
      std::exit(1);
    }
    return true;
  }();
  (void)inited;
  return fm;
}

vx::text::GlyphCache& SharedGlyphCache() {
  static vx::text::GlyphCache gc;
  return gc;
}
}  // namespace
```

如果 DejaVuSans.ttf 不存在（如 CI 容器无该字体），bench 进程 fail-fast 退出；不向 `DrawTextFallback` 静默降级（否则 A1 的"真路径"BM 实际还是 fallback，再次重蹈 K1 假阳性覆辙）。

### 5.3 layout_corpus.h 扩展

新增 1 个 builder（带 src 属性的 img）+ 1 个 cache：

```cpp
inline void BuildImageWithSrcStyled(vx::dom::Document& doc, int n,
                                    const std::vector<std::string>& paths) {
  VX_DCHECK(static_cast<int>(paths.size()) >= n);
  auto* body = doc.CreateElement(vx::dom::TagId::kBody);
  doc.AppendChild(body);
  for (int i = 0; i < n; ++i) {
    auto* img = doc.CreateElement(vx::dom::TagId::kImg);
    img->SetAttribute(vx::InternedString::Intern("src"),
                      vx::String(paths[i].c_str()));
    img->SetInlineDeclaration(vx::css::PropertyId::kBackgroundColor,
                              vx::css::CssValue::Color(0xff993366u));
    body->AppendChild(img);
  }
}

inline vx::dom::Document& CachedImageWithSrcStyledDocument(
    int n, const std::vector<std::string>& paths) {
  static std::mutex mu;
  static std::map<int, vx::dom::Document*> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto it = cache.find(n);
  if (it != cache.end()) return *it->second;
  auto* doc = new vx::dom::Document();
  detail::BuildImageWithSrcStyled(*doc, n, paths);
  cache.emplace(n, doc);
  return *doc;
}
```

> ⚠️ Plan 阶段必须 grep 验证 `Element::SetAttribute(InternedString, String)` 签名 + `InternedString::Intern("src")` 可用 — Phase 1 smoke 时第一时间验证。

## 6. CMakeLists 更新

```cmake
# Append to benchmarks/CMakeLists.txt after line 49
# Replay deep-bench (TASK-20260419-09): need vx_text for FontManager/GlyphCache
# and PNG::PNG for fixture writes.
vx_add_benchmark(bench_drawtext   vx_core vx_graphics vx_platform vx_text)
vx_add_benchmark(bench_imagecache vx_core vx_graphics vx_platform vx_text PNG::PNG)
```

> 验证：vx_core 已 PRIVATE vx_text，但 bench 直接用 `vx::text::FontManager` 需要 vx_text 在 link line 上 — 必须显式加。`PNG::PNG` 是 vcpkg/system 已提供的 imported target（被 vx_core 用过）。

## 7. 验收标准

| # | 标准 | 验证方法 |
|---|------|---------|
| 1 | 2 bench exe Release build 0 errors | `cmake --build build-bench --target bench_drawtext bench_imagecache` |
| 2 | bench_drawtext 6-8 BMs，bench_imagecache 7-8 BMs，全 exit 0 | `./build-bench/benchmarks/bench_<name> --benchmark_min_time=0.05s` |
| 3 | smoke 三件套全过：每 BM 数字非零 + `SetItemsProcessed > 0` + JSON `items_per_second > 0`（落实 `writing-plans.mdc`「性能基准任务必检项 §4」） | 手动检查 |
| 4 | 6+2+2=10 layout/render+css 系列 + 4 foundation = 14 共存 + 2 新 = 16 bench targets 共存零冲突 | `ctest -N \| wc -l`（如适用）+ build target 列表 |
| 5 | 2 baseline JSON 入仓 `benchmarks/baseline/bench_drawtext.json`、`bench_imagecache.json`，复用 TASK-05 的 4-piece 失真兜底协议 | 文件存在 + JSON 合法 + `aggregate_name=median` 行存在 |
| 6 | `benchmarks/README.md` exe 表 11→13；新增「TASK-09 K1/K5 量化结论」段 | 内容审 |
| 7 | `memory-bank/techContext.md`「Render 性能基线」段补 K1 fallback vs 真路径数值 + ImageCache O(N) 扫描验证；`systemPatterns.md`「Render Bench 前置清单」段补 DrawText fallback gate 条目 | 段落存在 |
| 8 | Debug ctest 全过（不引入回归） | `cd build && ctest -j8`（Release 也跑一次防回归） |
| 9 | **K1 命题给出明确判定**：8200 ns/cmd 是「fallback FillRect 串联 / FreeType raster cold / hb_shape / glyph_cache miss / SoftwareCanvas DrawTextFallback」中的哪个根因（必须给出数值证据） | A1+A2 数据交叉对比 |
| 10 | **K5 命题给出明确判定**：完整 layout→Record→Replay 真 image 路径的 ns/cmd vs DrawText 真路径 vs FillRect 基线 | B2 vs A2 vs TASK-05 ReplayMediumList |

**安全相关：** 否（性能测量任务，无外部输入/无认证/无新依赖）。

## 8. 风险预案

| 风险 | 概率 | 处理 |
|-----|------|-----|
| DejaVuSans.ttf 在沙箱不可用 | 低（系统字体） | fail-fast + 报清晰错误；后续可加 ENV var 覆盖 |
| `Element::SetAttribute` 签名与假设不符 | 中 | Phase 1 smoke 第一时间验证；如不符则改用 `dom::Element` 的实际 API（必要时读 element.h） |
| `PauseTiming/ResumeTiming` 引入额外开销噪声（B1 miss BM 用） | 中 | 改用预生成 path 池 + `state.iterations()` 索引避免 pause |
| Real path glyph cache warm-up 不充分（首次 iter 仍 cold） | 低 | bench fixture init 阶段先做 N 次 throwaway DrawText 把 cache 热好 |
| Bench BM 数比预估多 5+ 行（RangeMultiplier 展开） | 低 | 预估时直接列每 BM；不用 RangeMultiplier 给单个 BM 多 case |
| ImageCache Hit/256 性能差异 < 10ns（在 noise floor 下） | 中 | `--benchmark_min_time=0.5s` 提高样本数；如仍不可分辨，报告"低于测量精度" |
| build-bench 目录已含 TASK-05 cache，build 不增量 | 低 | 用 `--build` 增量；CMakeLists 改动会自动重 configure |
| 新 bench 链 vx_text 触发循环依赖 | 低 | vx_text 不依赖 vx_core；P1 静态库循环依赖审计已固化 |

## 9. 文件清单

| 操作 | 文件 | 说明 |
|------|------|------|
| 新建 | `benchmarks/bench_drawtext.cc` | A 组 6-8 BMs |
| 新建 | `benchmarks/bench_imagecache.cc` | B 组 7-8 BMs + libpng fixture |
| 修改 | `benchmarks/layout_corpus.h` | 新增 `BuildImageWithSrcStyled` + `CachedImageWithSrcStyledDocument`；**[共享文件]** 注意保持现有 9 个 builder 不变 |
| 修改 | `benchmarks/CMakeLists.txt` | 追加 2 行 `vx_add_benchmark` 注册；**[共享文件]** |
| 新建 | `benchmarks/baseline/bench_drawtext.json` | Release 3-rep median |
| 新建 | `benchmarks/baseline/bench_imagecache.json` | Release 3-rep median |
| 修改 | `benchmarks/README.md` | exe 表 11→13 行；新增 K1/K5 量化结论段 |
| 修改 | `benchmarks/baseline/README.md` | 新增 2 个 bench 的章节，标注 4-piece 协议套用 |
| 修改 | `memory-bank/techContext.md` | 「Render 性能基线」段补 K1/K5 真值 |
| 修改 | `memory-bank/systemPatterns.md` | 「Render Bench 前置清单」段补 DrawText fallback gate（line 145） |
| 修改 | `memory-bank/tasks.md` | 状态推进 |
| 修改 | `memory-bank/activeContext.md` | 阶段切换 |
| 修改 | `memory-bank/progress.md` | phase 进度 |
