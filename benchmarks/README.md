# Veloxa Benchmarks

Foundation + Core CSS 性能基准（基于 [google/benchmark](https://github.com/google/benchmark) v1.9.1，通过 CMake `FetchContent` 拉取）。

## 启用

基准默认关闭，需显式打开：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON
cmake --build build -j
```

> ⚠️ **务必使用 Release 构建**。Debug 模式下 google/benchmark 会打印 `***WARNING*** Library was built as DEBUG` 且数据失真（10×～100× 慢）。

如位于受限网络（例如 WSL + 公司代理），需先导出代理或在 git 全局配置中设置：

```bash
export http_proxy=http://<host>:<port>
export https_proxy=$http_proxy
# 或：
git config --global http.proxy http://<host>:<port>
git config --global https.proxy http://<host>:<port>
```

> 注：CMake 的 `FetchContent` 通过子进程调用 `git`，环境变量在某些 shell 下不会传递；上面 `git config` 是最稳妥的方式。

## 可执行文件

每个 `.cc` 编译为独立 exe，输出至 `build/benchmarks/`：

| 可执行 | 覆盖 | 用例数 |
|--------|------|--------|
| `bench_allocators` | `MallocAllocator` / `ArenaAllocator` / `PoolAllocator` | 13 |
| `bench_containers` | `Vector` / `SmallVector` / `IntrusiveList` | 8 |
| `bench_hash_map` | `HashMap` insert / lookup hit&miss / rehash | 10 |
| `bench_strings` | `BasicString` (SSO + heap) / `InternedString` | 9 |
| `bench_css_tokenizer` | `CssTokenizer::Next` (range sweep + numeric/string/whitespace heavy) | 10 |
| `bench_css_parser` | `CssParser::Parse` + `ParseDeclarationList` (4 stylesheet shapes + 6 inline + selector) | 11 |
| `bench_css_property_lookup` | `PropertyIdFromName` (HitAll/HitHot5/HitSingle×5/Miss/BuildInit, includes cluster probes) | 9 |

## 运行

```bash
# 跑单个 exe（默认所有用例，min_time≈0.5s/用例）
./build/benchmarks/bench_allocators

# 缩短最小测量时间（开发期 smoke）
./build/benchmarks/bench_allocators --benchmark_min_time=0.05s

# 过滤用例（regex）
./build/benchmarks/bench_hash_map --benchmark_filter='Lookup'

# 跑全部 7 个 exe
for b in bench_allocators bench_containers bench_hash_map bench_strings \
         bench_css_tokenizer bench_css_parser bench_css_property_lookup; do
  ./build/benchmarks/$b
done
```

## JSON 导出 + 对比

google/benchmark 自带 JSON 输出与 `compare.py` 对比工具：

```bash
# 当前分支：
./build/benchmarks/bench_allocators \
  --benchmark_format=json \
  --benchmark_out=baseline.json

# 切到对照分支重新构建：
git switch <other>
cmake --build build --target bench_allocators
./build/benchmarks/bench_allocators \
  --benchmark_format=json \
  --benchmark_out=other.json

# 对比（compare.py 来自 google/benchmark/tools）：
python3 build/_deps/benchmark-src/tools/compare.py \
  benchmarks baseline.json other.json
```

## 基线 JSON（已入仓）

本仓库**保留** 3 份 CSS 基线 JSON 在 `benchmarks/baseline/` 下，作为「形态参考」（不是绝对性能契约）：

```
benchmarks/baseline/
  bench_css_tokenizer.json
  bench_css_parser.json
  bench_css_property_lookup.json
  README.md  ← 失真警告 + 更新协议 + 命令模板
```

**任何对比之前**先读 `benchmarks/baseline/README.md`。**不要**按 JSON 数字做 ±10% 之类的 CI 卡点 — 跨硬件 / 调度器漂移远大于 10%。建议用法：本地两次编译跑同一份 JSON 名称，靠 `compare.py` 看相对变化（同机同环境）。

旧式 ad-hoc baseline.json 工作流仍然成立：

## 结果说明 / 已知量级（Debug WSL，仅供形态参考）

| 用例 | 量级 | 说明 |
|------|------|------|
| `BM_PoolAlloc/64` | ~5 ns | LIFO free-list 最快 |
| `BM_ArenaAlloc/64` | ~6 ns | 指针推进 |
| `BM_MallocAlloc/64` | ~25 ns | 含 `GlobalMemoryStats` 计数器开销 |
| `BM_VectorReservePushBackInt/4096` | ~16 µs | 无 grow 路径 |
| `BM_VectorPushBackInt/4096` | ~40 µs | 含 ~5 次 1.5× grow，~2.4× 慢 |
| `BM_HashMapLookupHitInt/16384` | ⚠️ 9 µs | 远高于 64 (69 ns)；`std::hash<int>` + `H1=h>>7` 在大 cap 时所有起始探测位置挤在 [0,127]，需后续优化 |
| `BM_StringConstructSso/8` | ~16 ns | SSO ≤22 字节路径 |
| `BM_StringConstructHeap/64` | ~38 ns | heap 分配 |
| `BM_InternedStringEquality` | ~1.7 ns | 验证 O(1) 指针相等 |
| `BM_TokenizeAll/4096` | TBD MiB/s | 详见 `baseline/bench_css_tokenizer.json`；form: 稳定吞吐 |
| `BM_ParseStylesheetMedium` (20×5) | TBD ns/op | Tokenizer 吞吐的 ~1/3，AST 构造主导 |
| `BM_ParseDeclarationListInline/8` | TBD ns/op | inline style 路径，每倍 decl 约 2× 时间（线性） |
| `BM_PropertyLookupHitHot5` | TBD ns | hot-key 平均；HashMap<StringView, PropertyId> 60 entries |
| `BM_PropertyLookupHitSingle/transition-timing-function` | TBD ns | **未触发 cluster** — 与 HitHot5 比 < 5×（详见 baseline/README） |

## 注意事项

- **MallocAllocator** 包含 `GlobalMemoryStats::RecordAlloc` / `RecordDealloc` 调用；与裸 `aligned_alloc` 数字不可直接对比。
- **InternedString** 的 `Intern` 用例会调用 `ClearInternedStrings()` 在 SetUp/TearDown 重置全局表，避免跨 BM 污染。
- **WSL 时钟分辨率** 较低；若某用例 `Time` 为 0 或样本极少，加 `--benchmark_min_time=1.0s`。
- **CSS Corpus** 由 `benchmarks/css_corpus.h` 程序化生成（`StylesheetCorpus(rules, decls)` / `InlineStyleCorpus(decls)`），**首次调用** O(rules×decls) 构造、缓存到 static map，后续 O(1)；BM 计时区间内不再重复生成。
- **PropertyMap 一次性 lazy build**（~60 entries 插入）由各 CSS property lookup BM 的 `Warmup()` 在第一行 BM 之前完成，不会污染单个 BM 的稳态 ns/op。
- **`bench_css_tokenizer` / `bench_css_parser`** 中的 IIFE-built `static const std::string css = []{...}()` 把 corpus 一次性烘到 BSS，避免 `Stylesheet/Inline` corpus map 的查表开销混入热路径。
