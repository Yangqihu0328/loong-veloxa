# Veloxa Benchmark Baselines

本目录保存 **CSS / Layout / Render / Replay-Deepbench 模块** 共 9 个 bench 的 google/benchmark JSON 输出，作为入仓的「形态参考基线」（Foundation 4 bench 不入仓 — 见 `../README.md` §策略）。

## ⚠️ 失真警告（必读）

这些 JSON 是**形态参考**，不是绝对性能契约：

1. **绝对数字与硬件 / 编译器版本 / OS 调度器 / CPU 频率治理强相关。** 同一份代码在不同环境下 `ns/op` 可漂移 2~10×，吞吐 (`MiB/s`) 漂移 30~100% 司空见惯。
2. **不要**用这些数字做 CI 卡点（例如「比 baseline 慢 ±10% 就 fail」）— 跨硬件漂移远大于 ±10%，会造成大量假阳性。
3. **正确用法**：本地评估改动时，先在当前 HEAD 跑一份新 JSON，再 `git switch <other> && cmake --build && 跑同一份 JSON 名`，用 `compare.py` 看**相对变化**。
4. 入仓的 baseline 数字仅用于：
   - 让人类读者快速建立量级直觉：
     - **CSS** — Tokenizer ~300 MiB/s、Parser ~100 MiB/s、PropertyLookup ~10 ns
     - **Layout** — buildtree flat ~ 117 ns/box (small)、512 box flat ~ 196 µs（super-linear knee 在 N=128~256 之间）、flex 8x8 ~ 4.9 µs / 16x16 ~ 73 µs（super-linear）
     - **Render** — Record ~26 ns/box (linear)、Replay FillRect ~10 ns/cmd (~100 M/s)、**Replay DrawText ~8200 ns/cmd（fallback 路径 hot — TASK-09 K1 已修正归因，见 §key findings）**
     - **DrawText** — fallback ~192 ns/char、real cold ~2777 ns/char（FT_Load+FT_Render，9.1× of warm）、real warm ~305 ns/char（hb_shape + glyph cache hit；当前 1.6× of fallback — K7）
     - **ImageCache** — `Get` ~1.16 ns（O(1)）、`Load` hit/1 ~43 ns、hit/16 ~44 ns、hit/256 **~46 ns**（HashMap O(1)，K6 已由 TASK-20260419-11 解决）、`ReplayImageReal<64>` ~37 ns/cmd
   - 跨任务做对照参考时的形态锚点
   - reflection / archive 文档引用时有具体数字可指

## Key findings

### TASK-20260419-05 入仓 (历史)

| # | 发现 | 数值 | 后续 |
|---|------|------|------|
| K1 | Replay 的真正 hot path 是 `DrawText`，不是 ImageCache | DrawText ~8200 ns/cmd vs FillRect ~10 ns/cmd = 820x | **TASK-09 phase-3 修正归因**（见下方 K1' / K7） |
| K2 | Layout buildtree-flat 在 N=128→256 出现 super-linear knee | 7.7 µs → 70 µs（10x for 2x N） | **TASK-10 候选**（VAN 否定 ArenaAllocator chunk grow 假设） |
| K3 | Layout flex 8x8→16x16 同源 super-linear | 4.9 µs → 73 µs（14.9x for 4x cells） | 同 K2，并入 TASK-10 |
| K4 | Record 对 image 元素无额外开销 | ImgVsNoImg(16) = 544 ns ≈ Medium(64)/4 = 465 ns | image 路径在 Record 是 noop；瓶颈在 Layout 的 image_cache->Load + Replay 的 cache->Get |
| K5 | ImageCache 真实例无法在 bench 内构造 | DecodeFromFile 需要文件 I/O；layout 不传 cache → image_handle=0 → Record 不 emit kDrawImage | **TASK-09 phase-2 已解决**（libpng 程序化 fixture，见 K6） |

### TASK-20260419-09 入仓 (本次 phase-2 + phase-3)

| # | 发现 | 数值 | 后续 |
|---|------|------|------|
| K1' | TASK-05 K1「DrawText 8200 ns/cmd 是 FT+HB 真路径」**修正归因** | TASK-05 测的是 fallback（`font_manager == nullptr` gate）：`Fallback_Medium` 3647 ns / 19 char = 192 ns/char ≈ FillRect ×19；"820×" 实为「1 cmd 含 N 字符 painting」vs「1 cmd 单 FillRect」的 per-cmd 工作不可比 | 文档化（已更新 systemPatterns Render Bench 前置清单） |
| K1'' | DrawText 真路径冷路径才是真正贵的 | `Real_Cold_Medium` 52763 ns vs `Real_Warm` 5807 ns = 9.1×；vs `Fallback` = 14× | glyph_cache 已是 ROI 极高的存量优化；冷启动场景仍可考虑 pre-warm |
| K6 | `ImageCache::Load` hit 路径 O(N) 字符串扫描；cache size 256 时单次 1162 ns | hit/1 9.99 ns → hit/16 48.5 ns → hit/256 **1162 ns**（116×）；**比 ReplayImageReal<16> 595 ns 还慢** | **强烈推荐**改 `HashMap<String, ImageHandle>`（O(1) 查表）— ROI 极高 |
| K6 ✅ | **K6 已由 TASK-20260419-11 解决**：`ImageCache::Load` hit 路径已改为 `HashMap<String, ImageHandle>` O(1) 查表（djb2 hash + owned String key + custom StringHash/StringEq 规避 SSO 悬空指针） | Hit<16>: 50.87 ns → **44.05 ns**（1.16×↓）；Hit<256>: 1151.77 ns → **45.70 ns**（**25.2×↓**）；Miss/ReplayImageReal/Get 不退化；Hit<1> 10.35 ns → 43.27 ns 小回归（HashMap 固有 ~32 ns 开销，绝对量微小，被 256 净增益完全压倒） | 入仓新 baseline；K6 量化命题完全满足；anomaly「size=256 cache hit 慢于 ReplayImageReal<16>」已消失 |
| K7 | DrawText 真路径 warm > fallback（1.6×） | `Real_Warm_Medium` 5807 ns vs `Fallback_Medium` 3647 ns | 如未来默认开真路径需先优化：(a) `hb_buffer` 复用避免每次 alloc / (b) glyph bitmap 直接 raster 到 canvas 避免中间 memcpy |

## 当前生成环境

| 维度 | 值 |
|------|----|
| 主机 | qihooz（开发笔记本，WSL2）|
| OS | Linux 6.6.87.2-microsoft-standard-WSL2 (Ubuntu 22.04 host) |
| CPU | x86_64, 8 logical, ~2918 MHz；L1d 48 KiB ×4 / L1i 32 KiB ×4 / L2 1280 KiB ×4 / L3 12288 KiB ×1 |
| 编译器 | gcc 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04.3), C++17 |
| google/benchmark 版本 | v1.9.1 (FetchContent) |
| 构建模式 | Release（`-DCMAKE_BUILD_TYPE=Release`，独立 `build-bench/`）|
| 生成命令的 `--benchmark_min_time` | CSS = 0.5s；Layout/Render = 0.05s（共 7 BM exe，0.5s 单文件 ~10s 接受度差，0.05s 已稳态 — 经 3 次 repetitions median 验证）；DrawText/ImageCache = 默认 0.5s（TASK-09 入仓 baseline） |
| 生成日期 | CSS = 2026-04-19 (TASK-03)；Layout/Render = 2026-04-19 (TASK-05)；DrawText/ImageCache = 2026-04-19 (TASK-09) |

> 任何对 baseline JSON 的更新都必须把上表 4 行 TBD 同步刷新；否则 baseline 失去可追溯性。

## 更新协议

更新这些 JSON **必须**走以下流程，以避免无意义的数字摆动入仓：

1. 在**真实 CSS 解析路径**有明确变更（algorithm / 数据结构 / 内联策略 / hash 函数）时才更新；
   - **不接受**：rebase 后跑了一遍 JSON 数字小幅波动就 commit
   - **接受**：feature 改动确实改变了某个 BM 的稳态量级（>= 30% 且与改动方向自洽）
2. **必须**用独立 `build-bench/` 目录的 Release 构建（`cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON`），**不要**复用日常 Debug build/。
3. **必须**加 `--benchmark_min_time=0.5s` 提升稳态质量，**不要**用开发期 smoke 的 `0.05s`。
4. 更新当前生成环境表 4 行 TBD（**不要**仅刷数字不刷环境表）。
5. PR 描述需说明：触发更新的代码改动是什么 + 哪些 BM 量级变了 + 同机对比的 `compare.py` 输出（绝对数字漂移除非新机器，否则不接受单独更新）。

## 命令模板

```bash
# Release 全量重建（必须，不能复用 Debug build/）
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON
cmake --build build-bench -j

# 跑 7 份 baseline JSON（CSS 用 0.5s，Layout/Render 用 0.05s + 3 reps aggregates）
for b in bench_css_tokenizer bench_css_parser bench_css_property_lookup; do
  ./build-bench/benchmarks/$b \
    --benchmark_format=json --benchmark_min_time=0.5s \
    --benchmark_out="benchmarks/baseline/${b}.json"
done
for b in bench_layout_buildtree bench_layout_flex bench_render_record bench_render_replay; do
  ./build-bench/benchmarks/$b \
    --benchmark_format=json --benchmark_repetitions=3 \
    --benchmark_report_aggregates_only=true --benchmark_min_time=0.05s \
    > "benchmarks/baseline/${b}.json"
done
for b in bench_drawtext bench_imagecache; do
  ./build-bench/benchmarks/$b \
    --benchmark_format=json \
    --benchmark_out="benchmarks/baseline/${b}.json"
done

# 体检（必须看到 library_build_type=release，否则数字废）
for j in benchmarks/baseline/bench_*.json; do
  echo "=== $j ==="
  python3 -c "import json; d=json.load(open('$j')); print('build_type =', d['context']['library_build_type']); assert d['context']['library_build_type']=='release', 'NOT release!'"
done

# 同机相对对比（评估改动）
git switch <other-branch>
cmake --build build-bench -j
./build-bench/benchmarks/bench_css_tokenizer \
  --benchmark_format=json --benchmark_min_time=0.5s \
  --benchmark_out=/tmp/css_tokenizer_other.json
python3 build-bench/_deps/benchmark-src/tools/compare.py \
  benchmarks benchmarks/baseline/bench_css_tokenizer.json \
  /tmp/css_tokenizer_other.json
```

## 历史

| 日期 | TASK | 触发原因 | 涉及 BM |
|------|------|---------|---------|
| 2026-04-19 | TASK-20260419-03 | 初次入仓（CSS 基准首次落地） | CSS 30 行 (10 + 11 + 9) |
| 2026-04-19 | TASK-20260419-05 | 入仓 Layout + Render 4 个 baseline | Layout 14 + 6 = 20 行；Render 5 + 5 = 10 行；共 30 行 |
| 2026-04-19 | TASK-20260419-09 | 入仓 DrawText + ImageCache 2 个 baseline；K1 修正归因 + K6/K7 新发现 | DrawText 8 BMs；ImageCache 7 BMs；共 15 行 |
| 2026-04-19 | TASK-20260419-11 | K6 已解决：ImageCache::Load HashMap 化，重生成 bench_imagecache 同机基线（带 repetitions=3 / mean+median+stddev） | ImageCache 7 BMs ×（1 main + mean + median + stddev + cv）|
