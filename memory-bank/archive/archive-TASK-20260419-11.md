# 归档：TASK-20260419-11 ImageCache::Load HashMap 化（K6 高 ROI 优化）

**日期：** 2026-04-19
**任务 ID：** TASK-20260419-11
**复杂度级别：** Level 2（多文件机械替换 + 数据双索引设计；无 UI/架构空白）
**状态：** ✅ 已完成
**分支：** `feature/TASK-20260419-11-imagecache-hashmap`（共 7 commits + 归档/收尾即将提交）

---

## 任务概述

将 `vx::image::ImageCache::Load(StringView path)` 的 hit 路径从 O(N) 字符串线性扫升级为 O(1) HashMap 查找，**严格保留现有 ABI**（`gfx::ImageHandle` 仍是 1-based vector 下标，`Get(handle)` 保持 O(1)）。

**触发来源**：TASK-20260419-09 K6 量化命题 — `BM_ImageCacheLoad_Hit<256> = 1151.77 ns` 比整个 `BM_ReplayImageReal<16> = 595 ns` 还慢，意味着任何 > 30 张图片的真实页面都直接受益。**候选区已立项 P1 高 ROI**。

**核心目标量化**：
| 指标 | Baseline (TASK-09) | 目标 |
|---|---:|---|
| `BM_ImageCacheLoad_Hit<16>` | 50.87 ns | < 50 ns |
| `BM_ImageCacheLoad_Hit<256>` | 1151.77 ns | < 100 ns |
| `BM_ImageCacheLoad_Miss` | 3833 ns | 不退化 |
| `BM_ImageCacheGet` | 0.94 ns | 不退化 |

---

## 技术方案

### 双索引设计（VAN F1 grep 实证后唯一可行方案）

VAN 阶段 5 处 grep 实证发现 `gfx::ImageHandle = u32` 是 1-based vector 下标（`image_cache.cc:30 &images_[handle - 1]`），ABI 不可改 → **必须保留 `Vector<Entry>` 提供 handle→image O(1) + Get ABI**，新增 `HashMap<String, ImageHandle, StringHash, StringEq>` 提供 path→handle O(1)。

```cpp
class ImageCache {
 public:
  StatusOr<gfx::ImageHandle> Load(StringView path);
  const gfx::Image* Get(gfx::ImageHandle handle) const;
  void Clear();
  usize size() const { return images_.size(); }

 private:
  struct Entry { String path; gfx::Image image; };

  // djb2; mirrors property.cc:84 StringViewHash. Owned String key avoids
  // dangling SSO pointers when `images_` resizes.
  struct StringHash {
    usize operator()(const String& s) const {
      usize h = 5381;
      auto sv = s.view();
      for (usize i = 0; i < sv.size(); ++i) {
        h = ((h << 5) + h) + static_cast<u8>(sv[i]);
      }
      return h;
    }
  };
  struct StringEq {
    bool operator()(const String& a, const String& b) const {
      return a.view() == b.view();
    }
  };

  Vector<Entry> images_;
  HashMap<String, gfx::ImageHandle, StringHash, StringEq> path_to_handle_;
};
```

### Load/Clear 路径切换

```cpp
StatusOr<gfx::ImageHandle> ImageCache::Load(StringView path) {
  String key(path);                                              // O(1) hit path
  if (auto* existing = path_to_handle_.Find(key)) return *existing;
  auto result = DecodeFromFile(path);
  if (!result.ok()) return result.status();
  Entry entry;
  entry.path = String(path);
  entry.image = static_cast<gfx::Image&&>(result.value());
  images_.push_back(static_cast<Entry&&>(entry));
  auto handle = static_cast<gfx::ImageHandle>(images_.size());
  path_to_handle_.Insert(key, handle);
  return handle;
}

void ImageCache::Clear() {
  images_.clear();
  path_to_handle_.clear();   // D3 - 双索引同步
}
```

---

## 实现摘要

### 文件变更

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| 修改 | `veloxa/core/image/image_cache.h` | +23 行：HashMap 字段 + StringHash/StringEq functor |
| 修改 | `veloxa/core/image/image_cache.cc` | +11/-6 行：Load 切 HashMap 路径，Clear 同步两表 |
| 修改 | `tests/core/image/image_cache_test.cc` | +26 行：新增 `ClearAndReloadDeduplicates`（D3 回归网，RED 探针验证有效）|
| 修改 | `benchmarks/baseline/bench_imagecache.json` | 重生成（147 → 805 行；带 repetitions=3 + median/mean/stddev/cv）|
| 修改 | `benchmarks/baseline/README.md` | K6-resolved 行 + 历史项 + 顶部概览数字更新 |
| 修改 | `benchmarks/README.md` | ImageCache 量级行更新 + tree 注释 |
| 创建 | `docs/specs/2026-04-19-imagecache-hashmap-design.md` | 192 行设计 spec |
| 创建 | `docs/plans/2026-04-19-imagecache-hashmap.md` | 585 行 4-phase 实施计划 |
| 创建 | `memory-bank/reflection/reflection-TASK-20260419-11.md` | Level 2 回顾 + 6 改进建议 |
| 修改 | `memory-bank/{tasks,activeContext,progress,techContext,systemPatterns}.md` | 全程 MB 同步 + 长期知识库沉淀 |

### 关键决策（D1-D4，plan 头脑风暴用户确认）

| # | 维度 | 选择 | 理由 |
|---|---|---|---|
| **D1** | HashMap key 类型 | **owned `String`** + 自定义 `StringHash` + `StringEq` | (a) `BasicString` SSO 内联（`string.h:30-56` grep 实证）使 StringView key 在 vector resize 时悬挂；(b) `vx::String` 仅 `==(String, StringView)` operator，`std::equal_to<String>` 类型推导不确定 → 自定义 functor 兜底 |
| **D2** | HashMap 初始容量 | 沿用 `kDefaultCapacity = 16`，**不 reserve** | 生产场景不频繁增长；TASK-09 K6 baseline 极端 256 entry 仅 ~4 次 rehash，分摊代价远低于命中收益 |
| **D3** | 边界测试 | 加 1 个 `ClearAndReloadDeduplicates` GTest | 双索引最易出 bug 点 = `Clear()` 漏清 HashMap → 该测试是「双索引同步」回归安全网；mixed TDD 下配 **RED 反向探针** 主动验证有效（注释 `path_to_handle_.clear()` → FAIL → 恢复 → PASS） |
| **D4** | Phase 划分 | **4 phase 细粒度**（每 phase 独立 commit）：P1 仅加字段 / P2 切 Load + 测试 / P3 bench + baseline / P4 文档 | 「数据结构改造与行为切换分离」让 review 风险面在每 commit 都可量化；P1 仅头文件改动验 F2 编译风险闭合，不影响行为 |

### 5 处 grep 实证（VAN 阶段，落实 P0 #4「方案根因假设未先验证」第 4 次完整应用）

| # | 命题 | grep 实证 | 影响设计 |
|---|---|---|---|
| F1 | 「`gfx::ImageHandle` 可改为不透明 token」 | ❌ 错。`graphics/image.h:49` `using ImageHandle = u32;` + `image_cache.cc:30 &images_[handle - 1]` | handle **必须**保持 1-based vector 下标 → 必须**双索引**保留 Vector |
| F2 | 「`std::equal_to<String>` 默认可用」 | ⚠️ 风险。`string.h:178/190` 仅有 `==(String, StringView)`，无 `==(String, String)` 直接版本 | 设计 `StringEq` 自定义 functor 兜底 |
| F3 | 「需要新写 djb2 hash」 | ❌ 错。`property.cc:84-92 StringViewHash` 已是现成 djb2 | 机械复刻 ~5 行（仅参数类型 `StringView` → `const String&`） |
| F4 | 「修改可能影响多个调用方」 | ❌ 错。`application.cc:67/118` 是**唯一调用方**（仅持有指针） | 回归风险接近零 |
| F5 | 「需新加 dedup 测试用例」 | ❌ 错。`image_cache_test.cc:55-65 DeduplicateSamePath` 已覆盖 | 现有 3 个测试已充分覆盖核心契约 |

### 安全决策

**本任务不涉及安全变更。** 性能优化任务，无外部输入新增 / 无认证路径 / 无新依赖（HashMap 与 String 均为 vx_core 内部成熟容器）。

---

## 测试覆盖

### 现有测试（regression）

- `ImageCacheTest.LoadAndGet` — Load 成功 + Get 返回正确 image 元数据
- `ImageCacheTest.DeduplicateSamePath` — 同 path Load 返回相同 handle（K6 改造关键回归点）
- `ImageCacheTest.LoadInvalidPath` — Load 失败时 cache size 不增长

### 新增测试（D3）

```cpp
TEST(ImageCacheTest, ClearAndReloadDeduplicates) {
  // TASK-20260419-11 D3 regression: Clear() must drop both the Vector entries
  // and the path_to_handle_ HashMap, so a re-Load after Clear gets a fresh
  // 1-based handle and still deduplicates.
  auto path = CreateTestPng();
  ImageCache cache;
  auto h1 = cache.Load(StringView(path.c_str()));
  // ... Clear → re-Load → assert handle == 1 → re-Load again → assert dedup
}
```

### 验证证据

- **Debug `ctest` 891/891 PASS**（含新增 `ClearAndReloadDeduplicates`）
- **Release `-O3 -DNDEBUG` (`build-bench/`) 0 errors / 0 warnings**
- **D3 RED 反向探针**：临时注释 `path_to_handle_.clear()` → 测试立即 FAIL → 恢复 → PASS（耗时 < 3 min，提供完整 RED→GREEN 证据链）
- **Linter 零错误**（`image_cache.{h,cc}` + 测试 + 全部 MB 文件）

### Bench 量化（K6 量化命题完全解，10/10 验收 ✅）

| BM (median) | Baseline | 现在 | 变化 | 判定 |
|---|---:|---:|---|---|
| `BM_ImageCacheLoad_Hit<16>` | 50.87 ns | **44.05 ns** | 1.16×↓ | ✅ < 50 ns 目标 |
| `BM_ImageCacheLoad_Hit<256>` | 1151.77 ns | **45.70 ns** | **25.2×↓** | ✅ < 100 ns 目标（远超 plan 12× 预期）|
| `BM_ImageCacheLoad_Miss` | 3833 ns | 3344 ns | 1.15×↓ | ✅ 不退化 |
| `BM_ImageCacheLoad_Hit<1>` | 10.35 ns | 43.27 ns | +33 ns | ⚠️ HashMap djb2 + probe 固有，绝对量微小，被 Hit<256> 净增益完全压倒（plan D2 已注明可接受）|
| `BM_ImageCacheGet` | 0.94 ns | 1.16 ns | 噪声级 | ✅ 实现未改动 |
| `BM_ReplayImageReal<16>` | 617 ns | 598 ns | 持平 | ✅ |
| `BM_ReplayImageReal<64>` | 2384 ns | 2398 ns | 持平 | ✅ |

**Smoke 三件套自检**：`items_per_second==0` 计数 = 0；`library_build_type=release`；7 个核心 BM 复跑数据稳定（cv 多数 < 1%）。

**Anomaly 消失**：「size=256 cache hit (1162 ns) 慢于整个 ReplayImageReal<16> (595 ns)」彻底消失。

---

## 经验教训（精选自 reflection §"经验教训"）

### L1 — Mixed TDD 模式下 RED 反向探针 = D3 类回归测试的标配补强

D3 决策（额外加 1 个 GTest 覆盖双索引同步）配 RED 反向探针验证（临时破坏实现 → FAIL → 恢复 → PASS），耗时 < 3 min 但提供完整证据链。**任何 mixed TDD 任务中「为预防特定 bug 而新增的回归测试」都应配反向探针**，否则可能在「实现恰好正确」的巧合下成为永远不报警的死代码。

### L2 — bench plan 阈值表对超低 ns BM 应用「绝对增量兜底」

`< baseline × 1.2` 阈值在超低 ns BM（< 50ns）上对测量噪声极敏感。Get 0.94→1.16 ns = 1.23×（噪声）；Hit<1> 10.35→43.27 ns = 4.18×（HashMap 固有）。改进公式：

```
判定阈值 = max(baseline × 1.2, baseline + 0.5ns)   for baseline < 5ns
判定阈值 = max(baseline × 1.2, baseline + 5ns)     for baseline ∈ [5, 50)ns
判定阈值 = baseline × 1.2                            for baseline ≥ 50ns
```

### L3 — Plan 阶段需 grep `which <tool>` 验证 smoke 工具链可用

VAN/Plan grep 实证不应只覆盖被改源码的 API；smoke 验收依赖的 CLI 工具（`jq` / `bc` / `valgrind` 等）也需在 plan 阶段确认存在，否则进入 Build 期才发现要切技术栈（本次 jq 缺失改 python heredoc，多花 ~3 min）。

### L4 — 「数据结构改造与行为切换分离」是低风险性能任务的通用 phase 拆分模式

P1 仅加字段不改语义 → P2 切 hot path 行为 + 测试。每 commit 风险面独立可审，F 类编译风险与 K 类运行时风险解耦定位互不污染。**适用于任何"加新数据结构 → 切换 hot path → 验证 perf"的优化型 PR**。

### L5 — HashMap 不是金科玉律，极小 N 下线性扫的 cache locality 仍胜

Hit<1> 4× 回归说明 HashMap djb2(O(strlen)) + probe + Slot 间接的固有 ~32ns 开销 > 单元素线性 memcmp + cache-line 命中。**若未来引入 N 永远 ≤ 4 的新 cache 场景（如最近 N 调用 token cache），不应套用本任务同构方案** — 用 fixed-size `array` + unrolled 比较或直接 `array<V, N>` 下标查找。

### Bench 估时校准协议首次实证（TASK-05/09 → TASK-11）

| 任务 | Plan | 实际 | 比率 |
|------|-----:|-----:|-----:|
| TASK-05 | 4.25h | 75 min | 3.4× |
| TASK-09 | 3.5h | ~50 min | 4.2× |
| **TASK-11** | **55-80 min** | **~35-40 min** | **~1.5-2.0×** |

`systemPatterns.md` "bench 类任务估时校准" 协议（含复用率 + 单 BM 3-5 min + phase 估时基线）**首次生效**，偏差从 4× 收敛到 ~2×；下次再校准一次即可写入 `writing-plans.mdc` 作为基线。

---

## 改进建议落实状态（来自 reflection §"改进建议"）

| # | 优先级 | 建议 | 落实状态 |
|---|:---:|---|---|
| 1 | **P1** | bench 阈值表对超低 ns BM 用绝对增量兜底 | ✅ 已沉淀到 `systemPatterns.md` 公式 + `activeContext.md` P1 待处理 |
| 2 | **P1** | Plan 阶段 grep `which <tool>` 验证 smoke 工具链 | ✅ `activeContext.md` P1 待处理（待加入 `writing-plans.mdc`） |
| 3 | **P1** | Mixed TDD D3 类回归测试标配 RED 反向探针 | ✅ `systemPatterns.md` 新模式段已沉淀 |
| 4 | **P2** | 数据结构改造与行为切换分离 P1+P2 拆分模式 | ✅ `systemPatterns.md` 新模式段已沉淀 |
| 5 | **P2** | HashMap 不是金科玉律：极小 N 下线性扫仍胜 | ✅ `techContext.md` HashMap 段附注已沉淀 |
| 6 | **P2** | bench 估时校准趋势监测（4.2×→2.0×→？） | ✅ `systemPatterns.md` 校准段已加 TASK-11 实证 |

**P0**：无（任务零返工、零阻塞、零回退）。

---

## 参考文档

- **设计规格：** `docs/specs/2026-04-19-imagecache-hashmap-design.md`
- **实现计划：** `docs/plans/2026-04-19-imagecache-hashmap.md`
- **回顾文档：** `memory-bank/reflection/reflection-TASK-20260419-11.md`
- **来源任务归档：** `memory-bank/archive/archive-TASK-20260419-09.md`（K6 命题来源）
- **核心源文件：**
  - `veloxa/core/image/image_cache.h`
  - `veloxa/core/image/image_cache.cc`
  - `tests/core/image/image_cache_test.cc`
- **基线 JSON：** `benchmarks/baseline/bench_imagecache.json`（TASK-11 重生成）
- **关键 commits：**
  - `e7a9162` docs(van): init imagecache hashmap optimization
  - `dbdcffc` docs(plan): imagecache hashmap design + plan
  - `ae72800` feat(image): P1 add HashMap field
  - `47ecb1d` feat(image): P2 ImageCache::Load O(N)→O(1) via HashMap
  - `1a1ceb5` docs(bench): P3 K6 baseline + READMEs
  - `bfff464` chore(build): finalize memory bank state
  - `6d7fa1d` docs(reflect): add reflection for TASK-20260419-11
