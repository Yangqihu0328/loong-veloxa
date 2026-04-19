# Foundation Benchmarks 设计规格

**任务 ID：** TASK-20260419-02
**日期：** 2026-04-19
**复杂度级别：** Level 2（简单增强）
**类型：** 性能测量基础设施 + Foundation 基线

---

## 1. 目标

补齐 TASK-20260405-01 Foundation 延期项 P1#1：将 Google Benchmark v1.9.1 集成到工程，按 Foundation 子模块（分配器 / 容器 / 字符串 / HashMap）建立首版性能基线，固化目录布局、命名与运行规范，作为后续 CSS / Layout / Render 模块基准（TASK-20260419-03 / 04）的脚手架。

**非目标：**
- 不引入持续回归监控（无 baseline JSON 提交、无 CI 接入）
- 不做 SIMD / 多线程 / 编译器对比等深度 profiling 维度
- 不覆盖上层模块（CSS / Layout / Render）——拆为独立后续任务

---

## 2. 决策清单

| ID | 维度 | 选择 | 理由 |
|----|------|------|------|
| D1 | 范围 | 仅 Foundation（allocators / containers / strings / hash_map） | 严格对齐 TASK-01 延期项；Foundation 接口稳定（已两轮债清理）；脚手架第一次落地聚焦 |
| D2 | 可执行文件粒度 | 一文件一 exe（4 个 exe） | Google Benchmark 官方风格；崩溃隔离；后续扩模块仅需新增 .cc + 一行 CMake |
| D3 | 用例覆盖深度 | 中等（每组件覆盖核心操作 + 1-2 个梯度，~25 个 BM） | 第一版基线：能稳定测量量级 + 找出明显回归即可；单 exe < 30s |
| D4 | 结果留存 | 仅控制台输出 + README 给出 JSON 导出方法（不提交 baseline） | 避免 baseline 失真；当前无 CI runner；YAGNI |

---

## 3. 架构

```
├── CMakeLists.txt (根)            ── 已有 VX_BUILD_BENCHMARKS option + FetchContent
└── benchmarks/                    ── 新建
    ├── CMakeLists.txt             ── vx_add_benchmark() 函数 + 4 个目标
    ├── README.md                  ── 运行方式、JSON 导出、对比说明
    ├── bench_allocators.cc        ── Malloc / Arena / Pool
    ├── bench_containers.cc        ── Vector / SmallVector / IntrusiveList
    ├── bench_hash_map.cc          ── HashMap insert / lookup / rehash
    └── bench_strings.cc           ── BasicString / InternedString
```

### 3.1 CMake 接入

**根 `CMakeLists.txt` 已就绪**（L43-53）：

```cmake
if(VX_BUILD_BENCHMARKS)
  include(FetchContent)
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
  )
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(benchmark)
  add_subdirectory(benchmarks)
endif()
```

本任务仅需创建 `benchmarks/CMakeLists.txt`：

```cmake
function(vx_add_benchmark name)
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} PRIVATE vx_foundation benchmark::benchmark)
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                            "${CMAKE_BINARY_DIR}/benchmarks")
endfunction()

vx_add_benchmark(bench_allocators)
vx_add_benchmark(bench_containers)
vx_add_benchmark(bench_hash_map)
vx_add_benchmark(bench_strings)
```

### 3.2 Benchmark 主入口

每个 `.cc` 文末统一使用 `BENCHMARK_MAIN();`，无需自定义 `main()`。

---

## 4. CMake 链接方向约束分析

按 `writing-plans.mdc` 「CMake 链接方向约束分析」要求：

| # | 项 | 说明 |
|---|----|------|
| 1 | 依赖图 | `bench_*` → `vx_foundation`（PUBLIC include via target）+ `benchmark::benchmark`（FetchContent 提供 INTERFACE/PUBLIC include） |
| 2 | 头文件暴露 | `bench_*.cc` 直接 `#include "veloxa/foundation/..."` 与 `<benchmark/benchmark.h>` |
| 3 | 符号依赖 | `vx_foundation` 静态库已包含 `Allocate` / `String::append` / `InternedString::Intern` 等导出符号；`benchmark::benchmark` 提供 `benchmark::State::KeepRunning` 等 |
| 4 | 当前链接方向 | `vx_foundation` 的 `target_include_directories(vx_foundation PUBLIC ${CMAKE_SOURCE_DIR})` 已正确（PUBLIC）→ 下游 bench 可直接 include 头文件 |
| 5 | 结论 | **无需调整 vx_foundation 的链接属性**。bench 目标只需 `PRIVATE` 链接两者 |
| 6 | 禁止项 | 不在 `benchmarks/CMakeLists.txt` 中重复 `find_package` / `pkg_check_modules`（沿用根目录已建立的依赖） |

---

## 5. FetchContent C 子项目编译选项审计

按 `writing-plans.mdc` 「FetchContent C 子项目编译选项审计」要求：

| # | 项 | 说明 |
|---|----|------|
| 1 | 第三方代码语言 | google/benchmark v1.9.1 主体为 **C++**（无 C 源） |
| 2 | 全局选项作用域 | 根 `CMakeLists.txt` 的 `-Werror`/`-Wpedantic` 已通过 `$<$<COMPILE_LANGUAGE:CXX>:...>` 限定为 C++ |
| 3 | 冲突预测 | benchmark 自身代码遵循高质量 C++17 约定，但**仍可能**触发 `-Wpedantic`/`-Werror` 中某些标志（例如未使用变量、隐式类型转换）。**必须在 Phase 1 末尾验证** `cmake --build build --target benchmark` 能编过；若失败，方案：对 benchmark 目标用 `target_compile_options(benchmark PRIVATE -Wno-error)` 屏蔽，**不**全局放开 |
| 4 | 验证方式 | Phase 0 步骤 4：`cmake -B build -DVX_BUILD_BENCHMARKS=ON && cmake --build build --target benchmark 2>&1 | tail -30` |

---

## 6. Benchmark 用例清单（决策 D3 落地）

### 6.1 `bench_allocators.cc`

| BM 名称 | 维度 | 实现要点 |
|---------|------|---------|
| `BM_MallocAlloc` | size: `Range(8, 4096)` 8/64/512/4096 | `for state: ptr=alloc.Allocate(size); alloc.Deallocate(ptr, size)` |
| `BM_ArenaAlloc` | size: 同上 | 在 setup 之外创建 ArenaAllocator(64KB)；`alloc.Allocate(size)`（不 Deallocate，每 256 次循环 Reset） |
| `BM_PoolAlloc` | size: 8/64/512（≤ block_size） | 每 size 创建 PoolAllocator(size, 256)；分配/释放 LIFO 循环 |
| `BM_ArenaReset` | 1 用例 | setup 中分配 100 个 64B 块，state 内测 Reset() |
| `BM_PoolAllocFreeChurn` | 1 用例 | 在 64B pool 上交错 Allocate/Deallocate 100 次，测稳态吞吐 |

合计 ≈ 4×3 + 3×3 + 3 + 1 + 1 = **17 用例**（按 Range 展开）

### 6.2 `bench_containers.cc`

| BM 名称 | 维度 | 实现要点 |
|---------|------|---------|
| `BM_VectorPushBackInt` | n: 16/256/4096 | `Vector<int>` push n 次，每 iteration 重建 |
| `BM_VectorReservePushBackInt` | n: 4096 | 对照组：`reserve(n)` 后 push，测无 grow 路径 |
| `BM_SmallVectorInline` | n: 4 (inline cap=8) | `SmallVector<int, 8>` push 4 次，全程 inline |
| `BM_SmallVectorOverflow` | n: 32 (溢出到 heap) | push 32 次，触发 inline→heap 迁移 |
| `BM_IntrusiveListPushFront` | n: 256 | 预分配 256 个 node 数组，push_front 入链 |
| `BM_IntrusiveListIterate` | n: 256 | setup 中建好 256 元素链，state 内全链遍历求和 |

合计 ≈ 3 + 1 + 1 + 1 + 1 + 1 = **8 用例**

### 6.3 `bench_hash_map.cc`

| BM 名称 | 维度 | 实现要点 |
|---------|------|---------|
| `BM_HashMapInsertInt` | n: 64/1024/16384 | 重建空 map，连续 Insert 不重复 key |
| `BM_HashMapLookupHitInt` | n: 64/1024/16384 | setup 预填，state 内随机命中 Find |
| `BM_HashMapLookupMissInt` | n: 1024 | setup 预填，state 内查不存在 key |
| `BM_HashMapRehash` | start_n: 16, target: 1024 | 创建小 map，连续 Insert 触发多次 Rehash 直到 1024 |

合计 ≈ 3 + 3 + 1 + 1 = **8 用例**

### 6.4 `bench_strings.cc`

| BM 名称 | 维度 | 实现要点 |
|---------|------|---------|
| `BM_StringConstructSso` | len: 8/22 | `String s("xxxxx")`，确保 ≤ 22 字节走 SSO |
| `BM_StringConstructHeap` | len: 64/256 | 触发 heap 分配 |
| `BM_StringAppendSso` | total: 16 | 反复 `s.append(c)` 直到 SSO 上限 |
| `BM_StringAppendHeap` | total: 256 | 反复 append 触发 grow，测 amortized |
| `BM_InternedStringInternUnique` | n: 100 | 每次 intern 一个新字符串（最坏路径） |
| `BM_InternedStringInternDup` | n: 100 | 反复 intern 同一字符串（缓存命中） |
| `BM_InternedStringEquality` | 1 | 比较两个相同 InternedString，验证 O(1) 指针相等 |

合计 ≈ 2 + 2 + 1 + 1 + 1 + 1 + 1 = **9 用例**

**全任务总计：≈ 42 个 BM 实例**（含 Range 展开），符合「中等深度」预期。

---

## 7. 测试策略（覆盖补充模式）

按 `writing-plans.mdc`「测试模式选择」：benchmark 文件的「测试」性质是**编译能过 + 能跑出非零数字**。本任务采用 **`[覆盖补充]` 模式**：

- benchmark 不是单元测试，无 GTest 套件覆盖
- 每个 phase 的"验收"≡ 该 .cc 编译成功 + `./bench_xxx --benchmark_min_time=0.01s` 跑完无 abort + 至少打印一个 BM 行
- 由于 Foundation 实现已存在（覆盖补充模式），benchmark 立即能跑

每个 phase 的验证命令模板：

```bash
cmake --build build --target bench_xxx
./build/benchmarks/bench_xxx --benchmark_min_time=0.01s --benchmark_format=console
```

预期输出形如：

```
-----------------------------------------------------
Benchmark           Time             CPU   Iterations
-----------------------------------------------------
BM_MallocAlloc/8   25.3 ns         25.2 ns     27000000
...
```

---

## 8. 安全考量

- **范围属性：** 纯内部性能测量，无外部输入 / 认证 / 敏感数据 / 网络 / 部署
- **新增依赖：** google/benchmark v1.9.1（FetchContent，C++17 库）
- **依赖审计任务：** 归档前 Phase 7 检索 google/benchmark v1.9.x 是否有 CRITICAL/HIGH CVE（参照 `security.mdc` 第四章）
- **供应链：** FetchContent 拉取 GitHub 官方仓库 `google/benchmark`（成熟开源项目，~1万 star，Apache-2.0 license）

---

## 9. 错误处理

| 失败模式 | 处理 |
|---------|------|
| FetchContent 拉取失败（网络/代理） | Phase 0 验证 `http_proxy`/`https_proxy` 已设；记录到 README |
| benchmark 编译失败（`-Werror` 冲突） | 按第 5 节方案 3，对 benchmark target 单独豁免 |
| BM 用例运行时 abort（如 PoolAlloc size 超限） | 在 .cc 中 `static_assert` 或 `BENCHMARK_REGISTER_F` 时校验参数 |
| WSL 时钟分辨率不足 | 使用 `--benchmark_min_time=0.5s` 提高样本 |

---

## 10. 文件清单

### 新建

| 文件 | 行数估计 | 职责 |
|------|---------|------|
| `benchmarks/CMakeLists.txt` | ~20 | `vx_add_benchmark()` 函数 + 4 个目标声明 |
| `benchmarks/README.md` | ~80 | 启用方式、运行命令、JSON 导出、`compare.py` 用法 |
| `benchmarks/bench_allocators.cc` | ~120 | 17 个 BM 用例（按 Range 展开） |
| `benchmarks/bench_containers.cc` | ~110 | 8 个 BM 用例 |
| `benchmarks/bench_hash_map.cc` | ~90 | 8 个 BM 用例 |
| `benchmarks/bench_strings.cc` | ~110 | 9 个 BM 用例 |

### 修改

| 文件 | 修改点 |
|------|-------|
| `memory-bank/techContext.md` | 「FetchContent 与代理」段后追加 Benchmark 启用说明；技术栈表加一行 google/benchmark v1.9.1；技术债务表 #1 标记 ✅ |
| `memory-bank/tasks.md` | 任务进度更新 |
| `memory-bank/progress.md` | Phase 完成记录 |
| `memory-bank/activeContext.md` | 阶段流转 |

---

## 11. 完成标准

- [ ] `cmake -B build -DVX_BUILD_BENCHMARKS=ON` 配置成功（FetchContent 拉取 google/benchmark v1.9.1）
- [ ] `cmake --build build` 全量构建成功（4 个 bench exe + 原 vx_foundation/test 全绿）
- [ ] 现有 890 GTest 测试 100% 通过（无回归）
- [ ] 4 个 bench exe 各自能独立跑完，至少各自打印 5 行 BM 数据
- [ ] `benchmarks/README.md` 文档化运行方式、JSON 导出、proxy 设置
- [ ] `techContext.md` 更新依赖表与技术债务表
- [ ] google/benchmark v1.9.1 CVE 检索通过（无 CRITICAL/HIGH）

---

## 12. 风险与缓解

| 风险 | 概率 | 缓解 |
|------|------|------|
| benchmark v1.9.1 与 `-Werror -Wpedantic` 冲突 | 中 | 5.3 已设方案 |
| WSL 网络代理偶发抖动导致 FetchContent 慢 | 低 | 提前 export `http_proxy`/`https_proxy`；已验证 8.3s 可拉完 |
| BM 数字在不同硬件差异大 | 高（已知） | 决策 D4 不留 baseline，仅说明这是开发机 ballpark |
| MallocAllocator 的 `RecordAlloc`/`RecordDealloc` 本身有开销污染基线 | 中 | 在 README/注释中标注：「本基准包含 GlobalMemoryStats 计数器开销」 |

---

**审批：** 用户 4 轮头脑风暴（Q1-Q4）均确认 A 方案。
