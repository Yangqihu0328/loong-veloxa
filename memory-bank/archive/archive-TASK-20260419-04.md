# 归档：修复 `enum_serialization.cc` Release `-Warray-bounds` 误报

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-04
**复杂度级别：** Level 1（小修小补 / 单文件 / 修复路径明确）
**状态：** ✅ 已完成

## 任务概述

`veloxa/core/css/enum_serialization.cc:63` 在 GCC Release `-O2` + 项目级 `-Werror=array-bounds` 下编译失败：

```
error: array subscript ‘const char* const [5][0]’ is partly outside array bounds of
       ‘const char* const [2]’ [-Werror=array-bounds]
   63 |   const char* s = table[v];
```

阻塞所有依赖 `vx_core` 的 Release 构建路径，直接卡住 TASK-20260419-03 Phase 1（CSS bench 链接 `vx_core`）。

**目标：** GCC Release `-O2 -Werror` 下 `vx_core` 干净编译；不影响 Debug、不削弱告警、保持 `EnumValueToCssString()` 行为契约；解锁 TASK-03。

## 技术方案

**采用方案：C — 去模板化**（用户裁定，根因消除）

**根因：** `template<usize N> Lookup(const char* const (&table)[N], u16 v)` 在 13 个 `case` 中按 5 种不同 `N` 内联出 5+ 个 clone。GCC IPA 跨 clone 做值域传播时，将「某 case 实际类型为 `[5]` 的访问」错误地匹配到「另一 case 类型为 `[2]/[4]` 的 table」，未识别 `if (v >= N) return` 已先行守住。Debug `-O0` 不做该层优化故不触发；TASK-02 仅链 `vx_foundation` 故潜伏。

**修复：** 删除模板，改非模板辅助函数 + 宏 wrapper。

```cpp
StringView LookupImpl(const char* const* table, std::size_t n, u16 v) {
  if (v >= n) return StringView();
  const char* s = table[v];
  if (!s) return StringView();
  return StringView(s, std::strlen(s));
}
#define VX_LOOKUP(arr, v) LookupImpl((arr), std::size(arr), (v))
// ... 13 处调用：return VX_LOOKUP(kDisplay, enum_value); ...
#undef VX_LOOKUP
```

**关键设计：** 宏用 `std::size(arr)` 自动派生长度，调用点写法跟模板版一样简洁，arr 与 size 不可能脱节 — 化解了 C 方案的传统反对意见（"显式传 size 易失配"）。宏 TU 内 `#define`/`#undef` 严格限定，不通过 header 泄漏。

**未采用方案：**
- A) 文件局部 `#pragma GCC diagnostic ignored "-Warray-bounds"`：警告抑制非根因消除；未来 GCC 修复后需手动清理
- B) CMake `set_source_files_properties(... PROPERTIES COMPILE_OPTIONS "-Wno-array-bounds")`：信息散到构建系统层；新人不易找

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `veloxa/core/css/enum_serialization.cc` | +37/-16；模板 `Lookup<N>` → 非模板 `LookupImpl` + `VX_LOOKUP` 宏；13 处调用替换；详尽注释记录 GCC IPA 误报史 |

### 关键决策

1. **方案 C 而非 A/B：** 用户优先选择根因消除而非警告抑制。事后看 +37/-16 行未超 Level 1 budget。
2. **宏 + 非模板 vs 纯函数：** 用 `VX_LOOKUP(arr,v) = LookupImpl(arr, std::size(arr), v)` 的宏化设计，保留模板版调用点的简洁与防失配优势，同时彻底消除 IPA clone 触发条件。
3. **TU 限定的宏：** `#define` 在匿名 namespace 内、`#undef` 在 `EnumValueToCssString` 之后、`namespace vx::css` 末尾前；宏不通过 header 泄漏到其他 TU。

### 安全决策

本任务不涉及安全变更（纯编译告警修复 / 重构语义等价；无外部输入 / 认证 / 网络 / 部署变化；无新依赖故无 CVE 审计）。

## 测试覆盖

复用现有 GTest（不新增测试）：

| 验证 | 命令 | 结果 |
|------|------|------|
| Debug 全套件回归 | `ctest --test-dir build` | **890/890 通过**（1.59s） |
| Release `vx_core` 编译 | `cmake --build build-bench --target vx_core -j` | **干净**（`-Werror=array-bounds` 不再触发） |
| Release 单测语义等价 | `ctest --test-dir build-bench -R EnumSerialization` | **17/17 通过**（含 `OutOfRangeReturnsEmpty` / `BorderStyleCoversAllFourSides` 等关键边界，`-O2` 下语义等价确认） |
| Lint | `read_lints` | **干净** |
| 依赖审计 | N/A | 本任务无新增/更新依赖 |

## 经验教训

1. **`template<usize N>` 取数组引用是 GCC IPA `-Warray-bounds` 误报的隐藏陷阱：** 当 N ≥ 3 个不同长度且开 `-O2 -Werror=array-bounds` 时，IPA cross-clone 值域传播会失误，触发硬编译失败。Debug `-O0` 完全不触发。
2. **「数组+长度对的现代 C++ 习惯」：** 宏 `VX_LOOKUP(arr,v) = LookupImpl(arr, std::size(arr), v)` 替代模板取引用，可获得相同的"调用点简洁 + 防 arr/size 失配"双优势，且无 IPA clone 触发器。已沉淀为 `systemPatterns.md` 可复用模式。
3. **「前置依赖/环境/API 能力未验证」反复模式（8+ 次）再次命中：** TASK-01 引入 `Lookup<N>` 时仅 Debug 验证；TASK-02 仅链 `vx_foundation` 故潜伏；TASK-03 Phase 1 链 `vx_core` 才暴露。与 TASK-02 反思 #1 P1 同源但更广（不限基准）。
4. **VAN 推 A、用户裁定 C，事后看 Level 1 budget 评估保守：** Level 1 边界判断 ≤ 5 行 vs ≤ 30 行的差距其实在宏化设计后被压到 +37/-16，实际工作量 < 30 min，仍稳在 Level 1。

## 改进建议落实状态

| # | 建议 | 优先级 | 状态 |
|---|------|------:|------|
| 1 | 引入 `template<usize N>` / CRTP / SFINAE 等模板特化技巧的 PR 必须验证 Release `-O2 -Werror` 通路 | P1（升级，反复出现） | ✅ 已写入 `activeContext.md`「待处理事项 → 长期项 P1（新升级）」；下次同类 PR 前固化到 `writing-plans.mdc` 「Release 通路验证」段 |
| 2 | 「数组+长度对的现代 C++ 习惯：宏化非模板查表」沉淀为可复用模式 | P2 | ✅ 已写入 `systemPatterns.md` 「来自 TASK-20260419-04 GCC IPA `-Warray-bounds` 修复」段 |

## 解锁价值

合并到 main 后，TASK-20260419-03 Phase 1 立即可续接：

```bash
git checkout feature/TASK-20260419-03-css-benchmarks
git rebase main
cmake --build build-bench --target bench_css_tokenizer bench_css_parser bench_css_property_lookup -j
# 验证 3 smoke BM 跑通后进入 Phase 2
```

## 参考文档

- 源文件：`veloxa/core/css/enum_serialization.cc`（含详尽注释）
- 现有测试：`tests/core/css/enum_serialization_test.cc`（166 行 / 60 处 `EnumValueToCssString` 断言，作回归基线）
- 模式沉淀：`memory-bank/systemPatterns.md` 「来自 TASK-20260419-04 GCC IPA `-Warray-bounds` 修复 → 数组+长度对的现代 C++ 习惯：宏化非模板查表」
- P1 待处理事项：`memory-bank/activeContext.md` 「待处理事项 → 长期项」
- 回顾内容：`memory-bank/progress.md` 「2026-04-19 REFLECT」段（Level 1 简要回顾，无独立 reflection 文件）
- 阻塞下游：TASK-20260419-03 Phase 1（CSS 性能基准）

## 提交记录（feature 分支 ahead-of-main 4 commits）

| Commit | 类型 | 说明 |
|--------|------|------|
| `a368217` | chore(mb) | VAN 阶段 — 立项 + 候选方案分析 |
| `eebcf07` | fix(css) | 主修复 — 去模板化 `Lookup<N>` |
| `d279cf0` | chore(mb) | BUILD 完成态 MB 更新 |
| `cd72041` | docs(reflect) | Level 1 简要回顾 + 模式沉淀 |
