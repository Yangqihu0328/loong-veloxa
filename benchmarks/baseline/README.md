# Veloxa Benchmark Baselines

本目录保存 **CSS 模块** 三个 bench 的 google/benchmark JSON 输出，作为入仓的「形态参考基线」。

## ⚠️ 失真警告（必读）

这些 JSON 是**形态参考**，不是绝对性能契约：

1. **绝对数字与硬件 / 编译器版本 / OS 调度器 / CPU 频率治理强相关。** 同一份代码在不同环境下 `ns/op` 可漂移 2~10×，吞吐 (`MiB/s`) 漂移 30~100% 司空见惯。
2. **不要**用这些数字做 CI 卡点（例如「比 baseline 慢 ±10% 就 fail」）— 跨硬件漂移远大于 ±10%，会造成大量假阳性。
3. **正确用法**：本地评估改动时，先在当前 HEAD 跑一份新 JSON，再 `git switch <other> && cmake --build && 跑同一份 JSON 名`，用 `compare.py` 看**相对变化**。
4. 入仓的 baseline 数字仅用于：
   - 让人类读者快速建立「Tokenizer ~300 MiB/s 数量级、Parser ~100 MiB/s 数量级、PropertyLookup ~10 ns 数量级」的直觉
   - 跨任务（如 TASK-05 Layout/Render bench）做对照参考时的形态锚点
   - reflection / archive 文档引用时有具体数字可指

## 当前生成环境

| 维度 | 值 |
|------|----|
| 主机 | qihooz（开发笔记本，WSL2）|
| OS | Linux 6.6.87.2-microsoft-standard-WSL2 (Ubuntu 22.04 host) |
| CPU | x86_64, 8 logical, ~2918 MHz；L1d 48 KiB ×4 / L1i 32 KiB ×4 / L2 1280 KiB ×4 / L3 12288 KiB ×1 |
| 编译器 | gcc 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04.3), C++17 |
| google/benchmark 版本 | v1.9.1 (FetchContent) |
| 构建模式 | Release（`-DCMAKE_BUILD_TYPE=Release`，独立 `build-bench/`）|
| 生成命令的 `--benchmark_min_time` | 0.5s |
| 生成日期 | 2026-04-19 |

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

# 跑 3 份 baseline JSON（顺序无关，互不影响）
for b in bench_css_tokenizer bench_css_parser bench_css_property_lookup; do
  ./build-bench/benchmarks/$b \
    --benchmark_format=json \
    --benchmark_min_time=0.5s \
    --benchmark_out="benchmarks/baseline/${b}.json"
done

# 体检（必须看到 library_build_type=release，否则数字废）
for j in benchmarks/baseline/bench_css_*.json; do
  echo "=== $j ==="
  python3 -c "import json,sys; d=json.load(open('$j')); print('build_type =', d['context']['library_build_type']); assert d['context']['library_build_type']=='release', 'NOT release!'"
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
| 2026-04-19 | TASK-20260419-03 | 初次入仓（CSS 基准首次落地） | 全部 30 行 (10 + 11 + 9) |
