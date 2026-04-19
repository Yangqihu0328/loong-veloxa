# 设计规格：CSS 解析性能基准（Tokenizer / Parser / Property Lookup）

**任务 ID：** TASK-20260419-03
**日期：** 2026-04-19
**复杂度：** Level 2（简单增强 / 单子系统 — CSS 基线，与 TASK-02 同构）
**基线分支：** `main`（commit `9482fb5`，890/890 GTest）
**功能分支：** `feature/TASK-20260419-03-css-benchmarks`

---

## 1. 目标

为 `vx::css` 三大热路径建立首版微基准，给后续 CSS 优化任务（包括 TASK-05 HashMap hash mixing 落地后的回归验证）提供「对比起点」与「量级直觉」。

### 必达

- **3 个新 bench exe**：
  - `bench_css_tokenizer` — `CssTokenizer::Next()` 全文 token 化吞吐
  - `bench_css_parser` — `CssParser::Parse()` 完整规则解析 + `ParseDeclarationList()` inline 路径
  - `bench_css_property_lookup` — `PropertyIdFromName()` 命中/未命中/cluster 度量
- **CMake 函数升级**：`vx_add_benchmark()` 增加可选参数指定额外 link 库，CSS bench 链接 `vx_core`
- **共享 CSS 生成器**：`benchmarks/css_corpus.h` 提供参数化 CSS 字符串（按 `rules × decls_per_rule` 缓存）
- **基线 JSON**：`benchmarks/baseline/bench_css_*.json` × 3 入仓，配套强失真警告
- **文档**：`benchmarks/README.md` 新增 CSS 章节 + Baseline 工作流；新建 `benchmarks/baseline/README.md`

### 非目标

- 不引入 CI 性能门禁（与 TASK-02 一致）
- 不覆盖 selector matcher / style resolver（应进入「CSS 应用阶段」基准，可能合并入 TASK-04 Layout pipeline）
- 不接入新的第三方依赖（沿用 google/benchmark v1.9.1）
- 不做 SIMD / 多线程 / 编译器对比

---

## 2. 关键决策（4 轮 brainstorm）

| ID | 维度 | 选项 | 选择 | 理由 |
|----|------|------|------|------|
| D1 | CSS 语料策略 | A inline 字面量 / B testdata 文件 / C 程序生成器 | **C 程序生成器** | `Range(...)` 直接控规模、零 IO、确定性、参数化对比方便 |
| D2 | 覆盖深度 | A 浅 ~5 BM/exe / B 中 ~10 BM/exe / C 深 ~15 BM/exe | **B 中** | 与 TASK-02 同档，能看出量级与趋势，单 exe Release < 5s |
| D3 | Property Lookup 是否独立 exe + 是否量化 cluster | A 独立 + 量化 cluster / B 独立不量化 / C 合入 parser | **A 独立 + cluster 度量** | TASK-02 副发现需实测数据支撑 TASK-05 立项；PropertyMap 60 entry 落在 cap=64/128 桶正好是 cluster 风险窗口 |
| D4 | 基线留存 | A 仅控制台 / B 提交 baseline JSON | **B 提交 baseline JSON**（带失真缓解机制） | 把"对比起点"留在仓库内，PR 可直接 diff；TASK-02 反思的失真问题用「单机参考 + 元信息嵌入 + 更新协议 + 不接 CI」缓解 |

> Phase 划分追加调整：Phase 5（仅文档）/ Phase 6（生成 baseline + 收尾）拆分，便于排查 baseline 序列化问题。

---

## 3. 架构

### 3.1 文件结构

```
benchmarks/
├── CMakeLists.txt                       # ⚠ 修改：vx_add_benchmark 增加 ARGN 链接库
├── css_corpus.h                         # ★ 新建：CSS 生成器（inline header-only）
├── bench_allocators.cc                  # 不变
├── bench_containers.cc                  # 不变
├── bench_hash_map.cc                    # 不变
├── bench_strings.cc                     # 不变
├── bench_css_tokenizer.cc               # ★ 新建（~10 BM）
├── bench_css_parser.cc                  # ★ 新建（~10 BM）
├── bench_css_property_lookup.cc         # ★ 新建（~10 BM，含 cluster 度量）
├── README.md                            # ⚠ 修改：CSS 章节 + Baseline 工作流升级
└── baseline/                            # ★ 新建目录
    ├── README.md                        # ★ 新建：失真缓解策略 + 更新协议
    ├── bench_css_tokenizer.json         # ★ 新建（Release WSL）
    ├── bench_css_parser.json            # ★ 新建（Release WSL）
    └── bench_css_property_lookup.json   # ★ 新建（Release WSL）
```

### 3.2 CMake 扩展

```cmake
# benchmarks/CMakeLists.txt
function(vx_add_benchmark name)
  # 第二个起的可变参数为 PRIVATE 链接的额外库（如 vx_core）。
  set(extra_libs ${ARGN})
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} PRIVATE
    vx_foundation
    benchmark::benchmark
    ${extra_libs})
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks")
endfunction()

# Foundation 既有 4 个：保持单参签名向后兼容
vx_add_benchmark(bench_allocators)
vx_add_benchmark(bench_containers)
vx_add_benchmark(bench_hash_map)
vx_add_benchmark(bench_strings)

# CSS 新 3 个：链接 vx_core
vx_add_benchmark(bench_css_tokenizer        vx_core)
vx_add_benchmark(bench_css_parser           vx_core)
vx_add_benchmark(bench_css_property_lookup  vx_core)
```

### 3.3 css_corpus.h（共享生成器）

```cpp
// benchmarks/css_corpus.h
#ifndef VELOXA_BENCHMARKS_CSS_CORPUS_H_
#define VELOXA_BENCHMARKS_CSS_CORPUS_H_

#include <string>
#include <vector>

namespace vx::bench {

// Deterministic, cached generators. The first call for a given (rules,
// decls_per_rule) builds the string once and stores it in a static map; all
// later calls hand back the same const reference, so generation cost is paid
// outside the BM loop and never times.
//
// Synthesis pattern (for ParseStylesheet / TokenizeAll):
//   .cls<i> { padding: 10px; margin: 5px; color: #336699; ... }   (i = 0..rules-1)
// Each rule has `decls_per_rule` declarations rotated through a fixed pool of
// real CSS property/value pairs (see kPool in css_corpus.cc-equivalent inline).
const std::string& StylesheetCorpus(int rules, int decls_per_rule);

// Synthesis pattern (for ParseDeclarationList): "padding: 10px; color: red; ..."
const std::string& InlineStyleCorpus(int decls);

// Real PropertyIdFromName keys (60 names from kPropertyTable[]).
// Stable across calls; safe to capture by reference inside BM body.
const std::vector<std::string>& AllPropertyNames();

// Hot subset reflecting realistic parser hit distribution.
const std::vector<std::string>& Hot5PropertyNames();  // {display,color,width,padding-top,margin-top}

// Names that miss (typos, unknown vendor prefixes).
const std::vector<std::string>& MissPropertyNames();  // {-webkit-foo,bar,baz123,...}

}  // namespace vx::bench

#endif
```

实现要点：
- 用 `static std::map<std::pair<int,int>, std::string>` 缓存
- 生成时使用 `snprintf` / `std::to_string` 按确定性模板拼接，**不依赖随机数**
- header-only `inline` 实现 — 7 个 bench exe 中只有 3 个 CSS exe `#include` 此头，其余不变

### 3.4 用例分布（D2 中等档，每 exe 约 10 BM）

#### 3.4.1 `bench_css_tokenizer.cc`（~10 BM）

| BM | Range / Args | 度量目标 |
|----|--------------|---------|
| `BM_TokenizeAll` | `Range(64, 4096)` 按字节算 → ~7 用例 | 对生成 stylesheet 全文 `Next()` 直到 EOF 的吞吐 |
| `BM_TokenizeNumericHeavy` | `Arg(2048)` | 数值 + 单位密集（dimension/percentage） |
| `BM_TokenizeStringHeavy` | `Arg(2048)` | 引号字符串密集（content / url） |
| `BM_TokenizeWhitespaceHeavy` | `Arg(2048)` | 大量空白与注释 |

> Range 估算（应用 P2 的公式 `ceil(log_m(hi/lo))+1`）：默认 `RangeMultiplier=8`，64→4096 → `ceil(log_8(64))+1 = ceil(2)+1 = 3`，3 用例不够 → 改用 `RangeMultiplier(2)`：`ceil(log_2(64))+1 = 7` 用例 ✅

#### 3.4.2 `bench_css_parser.cc`（~10 BM）

| BM | Range / Args | 度量目标 |
|----|--------------|---------|
| `BM_ParseStylesheetSmall` | `Args({2, 5})` rules×decls | 短规则吞吐基线 |
| `BM_ParseStylesheetMedium` | `Args({20, 5})` | 典型应用样式 |
| `BM_ParseStylesheetLarge` | `Args({200, 5})` | 大型框架风格 |
| `BM_ParseStylesheetWideRules` | `Args({20, 20})` | 每 rule 声明数膨胀 |
| `BM_ParseDeclarationListInline` | `Range(1, 32)` (decls) → ~6 用例 | inline-style 路径（HTML `style="..."`） |
| `BM_ParseSelectorListMixed` | `Arg(50)` | 选择器解析（复合 / 组合器 / class） |

#### 3.4.3 `bench_css_property_lookup.cc`（~10 BM）

| BM | 形态 | cluster 探测意图 |
|----|------|----------------|
| `BM_PropertyLookupHitAll` | 轮转所有 60 个真实 key | 平均探测代价 |
| `BM_PropertyLookupHitHot5` | 轮转 5 个高频 key | 真实解析的命中分布 |
| `BM_PropertyLookupHitSingle/<key>` | 5 个固定单 key（display / color / margin-top / border-radius / transition-timing-function） | 探不同 key 是否落在长链尾 — **如果 single 与 hot5 量级差超过 5×，证实 cluster** |
| `BM_PropertyLookupMiss` | 轮转 5 个未知 key（vendor 前缀 / typo） | 全表扫描代价 |
| `BM_BuildPropertyMap` | 0 args | 量化 `BuildPropertyMap()` 一次性 init 成本（懒初始化的 amortized cost） |

### 3.5 Baseline JSON 工作流（D4 失真缓解）

#### 3.5.1 生成命令（README 强制写明）

```bash
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON
cmake --build build-bench -j

for b in bench_css_tokenizer bench_css_parser bench_css_property_lookup; do
  ./build-bench/benchmarks/$b \
    --benchmark_format=json \
    --benchmark_out=benchmarks/baseline/$b.json \
    --benchmark_min_time=0.5s
done
```

#### 3.5.2 失真缓解 4 件套

| 机制 | 实现 |
|------|------|
| **元信息嵌入** | GBench JSON 的 `context.host_name / num_cpus / mhz_per_cpu / cpu_scaling_enabled / library_build_type` 自动包含；diff 时直接看到差异硬件 |
| **目录隔离 + 顶部警告** | `benchmarks/baseline/README.md` 红字写「单机 WSL Release 参考；跨硬件请重新生成」 |
| **更新协议** | baseline 仅在 `bench_css_*.cc` 自身改动后由提交者刷新，写入 PR 描述；无关变更 → 不动 baseline |
| **不接 CI** | 与 TASK-02 决策一致；baseline 仅作 PR 评审视觉对比锚点 |

#### 3.5.3 README.md 改动范围

- 删除当前第 79 行「**本仓库不提交基线 JSON**」
- 新增 `## CSS 基准与基线对比` 章节，包含：
  - CSS 三个 exe 的覆盖与用例数表
  - Baseline 生成命令 + 更新协议
  - 失真警告引用 `baseline/README.md`
- 现有「已知量级」表追加 CSS 行（4–5 条代表性数据，DEBUG WSL 形态）
- 现有「可执行文件」表从 4 行扩到 7 行

---

## 4. CMake 链接方向 + 第三方编译选项审计

> 来源：`writing-plans.mdc` 必检项 + TASK-02 反思固化

### 4.1 链接方向

| Bench exe | 直接链接 | 传递依赖 |
|-----------|---------|---------|
| `bench_css_tokenizer` | `vx_foundation` `benchmark::benchmark` `vx_core` | `vx_core` PUBLIC `vx_foundation vx_graphics` + PRIVATE `vx_text vx_script PNG::PNG JPEG::JPEG` |
| `bench_css_parser` | 同上 | 同上 |
| `bench_css_property_lookup` | 同上 | 同上 |

- **方向：** `bench_css_* → vx_core → {vx_foundation, vx_graphics, vx_text, vx_script}`，单向无环
- **风险：** 这些 bench 实际只用 `<veloxa/core/css/...>` 头，但 link 时拉入 PNG/JPEG/QuickJS 静态库 → exe 体积上升数 MB（可接受，不影响测量）

### 4.2 第三方编译选项

- 本任务**不引入新第三方依赖**
- google/benchmark 已在 TASK-02 通过 `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES` 屏蔽 `-Werror -Wpedantic`，本任务沿用，无需重做
- `vx_core` 传递的 PNG/JPEG/QuickJS 已在历史任务中处理 SYSTEM include，无需重做

### 4.3 ALIAS 识别（P1 #3）

- 本任务不操作任何第三方 target 属性 → ALIAS 风险 N/A
- 仅修改 `vx_add_benchmark()` 函数体（非 target 属性）

---

## 5. 测试策略（TDD「覆盖补充」模式）

与 TASK-02 一致：

- **不新增 GTest** — bench 是工具性补充，正确性由 `tests/core/css/{tokenizer,parser,property}_test.cc` 既有 GTest 保障
- **每 phase 验收 = 编译过 + 跑出非零数字 + BM 行计数 ≥ 设计值**
- **Smoke 阈值：** 每 phase 用 `--benchmark_min_time=0.05s` 在 ≤ 5 秒内验证，避免开发期等待
- **Release 验收：** Phase 6 用 `--benchmark_min_time=0.5s` 跑出最终 baseline

`activeContext.md` 在 Phase 0 标注：`TDD 模式：覆盖补充`

---

## 6. 安全分析

任务非安全相关：
- 纯内部性能测量基础设施
- CSS 生成器输入皆为代码硬编码模板，**非外部输入**
- 不涉及网络 / 认证 / 存储 / 部署 / 沙箱
- 无新外部依赖（google/benchmark v1.9.1 已通过 TASK-02 CVE 审计）

→ Phase 6 收尾跳过依赖安全审计步骤（无新增依赖）

---

## 7. 错误处理与边界情况

| 风险 | 缓解 |
|------|------|
| 生成器跨 BM 调用反复构建大字符串 → 污染测量 | `static` 缓存 + header-only inline，首次访问后均摊为 O(1) 引用 |
| `bench_css_property_lookup` 的 `PropertyIdFromName` 内部首次调用触发 `BuildPropertyMap`（懒初始化）→ 第一个 BM 偏慢 | 在每个 BM 函数体顶部先调用 `vx::css::PropertyIdFromName(StringView("display"))` 预热（warm-up） |
| `vx_core` 链接拉入 PNG/JPEG → exe 体积增大但不影响 BM | 文档化（README CSS 章节注脚），不试图剥离 |
| Release 与 Debug build dir 串扰 | 强制 `build-bench/` 独立目录（来源 P1 #2） |
| `PropertyMap` 内部 hash 是 djb2（非 `std::hash<int>`），cluster 风险弱于 TASK-02 副发现 | bench 量化结果若与预期不符，反思阶段记录"实测优于预期"作为 TASK-05 立项前的反向证据 |

---

## 8. 文件变更清单（最终）

| 操作 | 文件 | 行数估计 |
|------|------|---------|
| 新建 | `docs/specs/2026-04-19-css-benchmarks-design.md` | 本文档 |
| 新建 | `docs/plans/2026-04-19-css-benchmarks.md` | ~600 行 |
| 修改 | `benchmarks/CMakeLists.txt` | +5 / -1 |
| 新建 | `benchmarks/css_corpus.h` | ~120 行 |
| 新建 | `benchmarks/bench_css_tokenizer.cc` | ~120 行 |
| 新建 | `benchmarks/bench_css_parser.cc` | ~140 行 |
| 新建 | `benchmarks/bench_css_property_lookup.cc` | ~150 行 |
| 修改 | `benchmarks/README.md` | +60 / -10 |
| 新建 | `benchmarks/baseline/README.md` | ~50 行 |
| 新建 | `benchmarks/baseline/bench_css_tokenizer.json` | GBench 输出 |
| 新建 | `benchmarks/baseline/bench_css_parser.json` | GBench 输出 |
| 新建 | `benchmarks/baseline/bench_css_property_lookup.json` | GBench 输出 |
| 修改 | `memory-bank/activeContext.md` | 阶段流转 |
| 修改 | `memory-bank/tasks.md` | 任务状态流转 |
| 修改 | `memory-bank/progress.md` | phase 进度记录 |
| 修改 | `memory-bank/techContext.md` | 「Benchmark 启用」段补 baseline 工作流条目 |

---

## 9. 完成标准

- [ ] 7 个 phase 全绿（0 ~ 6）
- [ ] 全测试 890/890 通过（无回归）
- [ ] 全量构建 0 警告 0 错误（Debug 与 Release 各一次）
- [ ] 3 个 CSS bench exe 编译通过且各打印 ≥ 设计值的 BM 行
- [ ] 3 个 baseline JSON 入仓且 `context.library_build_type == "release"`
- [ ] `benchmarks/README.md` CSS 章节完整 + baseline 工作流可独立复现
- [ ] `cluster` 度量数据写入回顾，作为 TASK-05 立项判据

---

## 10. 风险与权衡

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| baseline JSON 入仓违背 TASK-02 反思 | 高 | 中 | 失真缓解 4 件套已设计；不接 CI；目录隔离 + README 红字 |
| `vx_core` 拉入大量传递依赖导致 link 时间长 | 中 | 低 | 接受；CSS bench 用例少，build 时间可接受 |
| WSL 时钟分辨率致 cluster 单 BM 数据不稳 | 中 | 中 | 提高 `--benchmark_min_time=0.5s`；在 README 标注「WSL 数据仅形态参考」 |
| `BuildPropertyMap` 懒初始化干扰首 BM | 中 | 低 | 每 BM 顶部预热调用 |
| 生成器实现复杂度膨胀 | 低 | 中 | 限定为 inline header-only ~120 行；只支持参数化模板，不做 grammar |

---

**设计冻结。** 进入实现计划撰写。
