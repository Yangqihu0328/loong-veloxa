# ImageCache HashMap 化（K6 高 ROI 优化）实现计划

**目标：** 将 `vx::image::ImageCache::Load` hit 路径从 O(N) 字符串扫升级为 O(1) HashMap 查找，保持 ABI / 测试契约不变。

**架构：** 双索引数据结构 — 保留 `Vector<Entry>` 提供 handle→image O(1) 查找（保 1-based ABI）+ 新增 `HashMap<String, ImageHandle, StringHash, StringEq>` 提供 path→handle O(1) 查找。Load 命中走 HashMap，未命中走原 DecodeFromFile + push_back + HashMap::Insert 同步路径。

**技术栈：** C++20 / vx::HashMap / google/benchmark / GTest / CMake

**复杂度级别：** Level 2（4 phase / ~55-80 min plan 估时）

---

## §0 全局约束

| 维度 | 设置 |
|------|------|
| TDD 模式 | **混合模式**：现有 3 个 GTest 已是「覆盖补充」语义（行为契约不变）；新增 `ClearAndReloadDeduplicates` 用 **TDD** 模式（先写测试 → 切换 Load 路径 → 测试通过） |
| Build 类型 | Debug `build/`（ctest）+ Release `build-bench/`（bench）双 build 目录复用 TASK-09 设施 |
| 提交粒度 | 每 phase 一次 commit；P3 / P4 可能各含 1 提交 |
| 工作分支 | `feature/TASK-20260419-11-imagecache-hashmap`（已创建于 VAN，含 1 commit `e7a9162`） |
| 验证环境 | 同机与 TASK-09 baseline 比对（`benchmarks/baseline/bench_imagecache.json`） |

---

## §1 文件结构

| 操作 | 文件 | 职责 |
|------|------|------|
| 修改 | `veloxa/core/image/image_cache.h` | 加 HashMap 字段 + StringHash/StringEq 私有 functor + include `hash_map.h` |
| 修改 | `veloxa/core/image/image_cache.cc` | Load 路径切换为 HashMap.Find + Insert；Clear 同步清空 HashMap |
| 修改 | `tests/core/image/image_cache_test.cc` | 新增 `ClearAndReloadDeduplicates` 用例（D3） |
| 修改 | `benchmarks/baseline/bench_imagecache.json` | 同机复跑后重生成（K6 修复证据） |
| 修改 | `benchmarks/baseline/README.md` | Key findings 段加 K6 修复证据行 |
| 修改 | `benchmarks/README.md` | 性能数据段加 K6 修复证据行 |
| 修改 | `memory-bank/techContext.md` | 「Replay-Deepbench 性能基线」段 K6 状态更新（待优化 → 已解决） |
| 修改 | `memory-bank/tasks.md` | TASK-11 状态推进 |
| 修改 | `memory-bank/activeContext.md` | 阶段推进 |
| 修改 | `memory-bank/progress.md` | 里程碑记录 |

---

## §2 Phase 划分（4 phase）

| Phase | 估时 | 文件 | 提交主题 |
|-------|------|------|---------|
| **P1** 加 HashMap 字段（不切换路径） | 10-15 min | `image_cache.h` | `wip(TASK-20260419-11): phase-1 add HashMap field for double-index` |
| **P2** 切换 Load 路径 + 新增测试 | 20-30 min | `image_cache.cc` + `image_cache_test.cc` | `feat(image): O(1) ImageCache::Load via HashMap (TASK-20260419-11 phase-2)` |
| **P3** bench 同机复跑 + baseline 重生成 | 15-20 min | `benchmarks/baseline/bench_imagecache.json` + 2 README | `docs(bench): TASK-20260419-11 phase-3 K6 baseline + READMEs` |
| **P4** techContext + MB 收尾 | 10-15 min | techContext + activeContext + tasks + progress | `chore(build): finalize TASK-20260419-11 memory bank state (phase-4)` |

---

## Phase 1 — 加 HashMap 字段（不切换路径）

**目标：** 在 `image_cache.h` 中引入 HashMap 字段 + 私有 functor，但**不**修改 `image_cache.cc` — 即字段已存在但未被读写。验收：ctest 全过证明数据结构引入未破坏行为，HashMap 模板实例化（含 StringHash / StringEq）编译通过（这是 F2 风险的最终验证）。

### 任务 1.1：修改 `image_cache.h` [覆盖补充]

**文件：**
- 修改：`veloxa/core/image/image_cache.h`

- [ ] **步骤 1：阅读现状**

```1:30:veloxa/core/image/image_cache.h
#ifndef VELOXA_CORE_IMAGE_IMAGE_CACHE_H_
#define VELOXA_CORE_IMAGE_IMAGE_CACHE_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/string.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/image.h"

namespace vx::image {

class ImageCache {
 public:
  StatusOr<gfx::ImageHandle> Load(StringView path);
  const gfx::Image* Get(gfx::ImageHandle handle) const;
  void Clear();
  usize size() const { return images_.size(); }

 private:
  struct Entry {
    String path;
    gfx::Image image;
  };
  Vector<Entry> images_;
};

}  // namespace vx::image

#endif  // VELOXA_CORE_IMAGE_IMAGE_CACHE_H_
```

- [ ] **步骤 2：替换为以下内容（StrReplace）**

```cpp
#ifndef VELOXA_CORE_IMAGE_IMAGE_CACHE_H_
#define VELOXA_CORE_IMAGE_IMAGE_CACHE_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/hash_map.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/string.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/image.h"

namespace vx::image {

class ImageCache {
 public:
  StatusOr<gfx::ImageHandle> Load(StringView path);
  const gfx::Image* Get(gfx::ImageHandle handle) const;
  void Clear();
  usize size() const { return images_.size(); }

 private:
  struct Entry {
    String path;
    gfx::Image image;
  };

  // djb2, mirrors veloxa/core/css/property.cc:84 StringViewHash; owned String
  // key avoids dangling SSO pointers when `images_` resizes.
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

}  // namespace vx::image

#endif  // VELOXA_CORE_IMAGE_IMAGE_CACHE_H_
```

- [ ] **步骤 3：编译 + ctest 验证未破坏现有行为**

```bash
cmake --build build -j 2>&1 | tail -20
cd build && ctest --output-on-failure -j 2>&1 | tail -10
```

预期：
- 编译零错误（`HashMap<String, u32, StringHash, StringEq>` 模板实例化通过 — F2 风险消除证据）
- ctest 仍是 890/890

- [ ] **步骤 4：提交**

```bash
git add veloxa/core/image/image_cache.h
git commit -m "wip(TASK-20260419-11): phase-1 add HashMap field for double-index

P1 仅引入 path_to_handle_ HashMap 字段 + StringHash/StringEq 私有 functor，
不修改 .cc 实现 — 行为完全不变，ctest 仍 890/890。

验证目的：
- HashMap<String, u32, StringHash, StringEq> 模板实例化编译通过（F2 风险消除）
- 双索引数据结构改造与 Load 路径切换分离，便于独立 review/回滚"
```

---

## Phase 2 — 切换 Load 路径 + 新增测试

**目标：** Load hit 走 HashMap.Find；Insert 同步两个容器；Clear 同步 clear；新增 `ClearAndReloadDeduplicates` 用例。验收：现有 3 + 新 1 = 4 个 GTest 全绿。

### 任务 2.1：先写新测试（TDD RED）[TDD]

**文件：**
- 修改：`tests/core/image/image_cache_test.cc`

- [ ] **步骤 1：在 `LoadInvalidPath` 测试后追加新用例**

定位插入点：

```67:73:tests/core/image/image_cache_test.cc
TEST(ImageCacheTest, LoadInvalidPath) {
  ImageCache cache;
  auto result = cache.Load("/nonexistent.png");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(cache.size(), 0u);
}

}  // namespace vx::image
```

在 `LoadInvalidPath` 闭合 `}` 之后、`}  // namespace vx::image` 之前插入：

```cpp
TEST(ImageCacheTest, ClearAndReloadDeduplicates) {
  auto path = CreateTestPng();
  ImageCache cache;

  auto h1 = cache.Load(StringView(path.c_str()));
  ASSERT_TRUE(h1.ok());
  EXPECT_EQ(cache.size(), 1u);

  cache.Clear();
  EXPECT_EQ(cache.size(), 0u);

  auto h2 = cache.Load(StringView(path.c_str()));
  ASSERT_TRUE(h2.ok());
  EXPECT_EQ(cache.size(), 1u);
  // After Clear, the second Load must rebuild the cache from scratch
  // (the index is reset and the new handle is the first one again).
  EXPECT_EQ(h2.value(), static_cast<gfx::ImageHandle>(1));

  // Loading the same path a second time must dedup against the new entry.
  auto h3 = cache.Load(StringView(path.c_str()));
  ASSERT_TRUE(h3.ok());
  EXPECT_EQ(h3.value(), h2.value());
  EXPECT_EQ(cache.size(), 1u);

  std::remove(path.c_str());
}
```

- [ ] **步骤 2：运行测试验证 RED（含一个失败：`h3` dedup 应失败因为 P1 还没切换 Load 路径）**

```bash
cmake --build build -j --target image_cache_test 2>&1 | tail -5
cd build && ctest -R "ImageCacheTest.ClearAndReloadDeduplicates" --output-on-failure 2>&1 | tail -20
```

预期：**FAIL**（在 `h3` dedup 处 — 当前 Load O(N) 扫描没问题，但 Clear 后 vector 已清空，`h2` 是 1，`h3` 走 for-loop 仍能 dedup 命中 → 实际可能 PASS！）

> **注意**：当前 O(N) Load 实现实际上 **可能让此测试通过**，因为 Clear 只清 vector，O(N) 扫描在新 vector 上正确工作。此测试的真正威力在 P2 步骤 2.2 切换路径**且如果 Clear 漏清 HashMap 时**才显现 RED。这不违反 TDD —— 如果 RED 不出现，说明 P1 改造已覆盖所有契约，记录到 progress 即可继续。

实际预期：**PASS**（O(N) 扫描在 vector clear 后仍正确；这个测试在 P2 才是真正的回归安全网）

- [ ] **步骤 3：提交（测试本身可独立 commit）**— 暂缓，与 2.2 实现合并到一个 phase-2 commit。

### 任务 2.2：切换 `image_cache.cc` Load + Clear 路径 [TDD GREEN]

**文件：**
- 修改：`veloxa/core/image/image_cache.cc`

- [ ] **步骤 1：阅读现状**

```1:35:veloxa/core/image/image_cache.cc
#include "veloxa/core/image/image_cache.h"

#include "veloxa/core/image/image_decoder.h"

namespace vx::image {

StatusOr<gfx::ImageHandle> ImageCache::Load(StringView path) {
  for (usize i = 0; i < images_.size(); ++i) {
    if (images_[i].path.view() == path) {
      return static_cast<gfx::ImageHandle>(i + 1);
    }
  }

  auto result = DecodeFromFile(path);
  if (!result.ok()) {
    return result.status();
  }

  Entry entry;
  entry.path = String(path);
  entry.image = static_cast<gfx::Image&&>(result.value());
  images_.push_back(static_cast<Entry&&>(entry));
  return static_cast<gfx::ImageHandle>(images_.size());
}

const gfx::Image* ImageCache::Get(gfx::ImageHandle handle) const {
  if (handle == gfx::kInvalidImage || handle > images_.size()) {
    return nullptr;
  }
  return &images_[handle - 1].image;
}

void ImageCache::Clear() { images_.clear(); }

}  // namespace vx::image
```

- [ ] **步骤 2：用 StrReplace 替换为以下实现**

```cpp
#include "veloxa/core/image/image_cache.h"

#include "veloxa/core/image/image_decoder.h"

namespace vx::image {

StatusOr<gfx::ImageHandle> ImageCache::Load(StringView path) {
  // O(1) hit path via HashMap (TASK-20260419-11 K6 fix).
  String key(path);
  if (auto* existing = path_to_handle_.Find(key)) {
    return *existing;
  }

  auto result = DecodeFromFile(path);
  if (!result.ok()) {
    return result.status();
  }

  Entry entry;
  entry.path = String(path);
  entry.image = static_cast<gfx::Image&&>(result.value());
  images_.push_back(static_cast<Entry&&>(entry));
  auto handle = static_cast<gfx::ImageHandle>(images_.size());
  path_to_handle_.Insert(key, handle);
  return handle;
}

const gfx::Image* ImageCache::Get(gfx::ImageHandle handle) const {
  if (handle == gfx::kInvalidImage || handle > images_.size()) {
    return nullptr;
  }
  return &images_[handle - 1].image;
}

void ImageCache::Clear() {
  images_.clear();
  path_to_handle_.clear();
}

}  // namespace vx::image
```

> **设计要点**：
> - `String key(path)` 一次构造后**两处复用**：先 `Find`，未命中再 `Insert(key, handle)` — 避免重复构造（HashMap 内部 Insert 会再次拷贝 key 进 slot，这是不可避免的 owned-key 代价）
> - `images_.push_back` 之后再算 handle — 保 1-based ABI
> - `Get` 完全不动 — 保 O(1) ABI

- [ ] **步骤 3：编译 + 运行 4 个 ImageCacheTest 用例**

```bash
cmake --build build -j --target image_cache_test 2>&1 | tail -5
cd build && ctest -R "ImageCacheTest" --output-on-failure 2>&1 | tail -15
```

预期：**4/4 PASS**（LoadAndGet / DeduplicateSamePath / LoadInvalidPath / ClearAndReloadDeduplicates 全绿）

- [ ] **步骤 4：全量回归 ctest**

```bash
cd build && ctest -j --output-on-failure 2>&1 | tail -10
```

预期：**891/891 passed**（从 890 → 891，仅新增 ClearAndReloadDeduplicates）

- [ ] **步骤 5：Release `-Werror` 通路验证**

```bash
cmake --build build-bench -j 2>&1 | tail -10
```

预期：零 `-Werror` 失败

- [ ] **步骤 6：提交**

```bash
git add veloxa/core/image/image_cache.cc tests/core/image/image_cache_test.cc
git commit -m "feat(image): O(1) ImageCache::Load via HashMap (TASK-20260419-11 phase-2)

把 ImageCache::Load 的 hit 路径从 O(N) 字符串线性扫升级为 HashMap O(1) 查找：
- Load 先 path_to_handle_.Find(key) 命中即返回 handle
- 未命中走原 DecodeFromFile + push_back，再 path_to_handle_.Insert(key, handle)
- Clear 同步清空 vector + hashmap
- Get(handle) 完全不变（仍 images_[handle-1]）保 O(1) + 1-based ABI

新增 ClearAndReloadDeduplicates GTest 作为「双索引同步」回归安全网（D3 决策）。

验证：
- 4 个 ImageCacheTest 用例 4/4 通过（含新增）
- ctest 全量 891/891（890 + 1）
- Release -Werror 零编译失败"
```

---

## Phase 3 — bench 同机复跑 + baseline 重生成 + K6 量化判定

**目标：** 复跑 `bench_imagecache`，确认 Hit<16> < 50 ns 且 Hit<256> < 100 ns（K6 命题已解），重生成 baseline JSON 入仓，更新 README。

### 任务 3.1：复跑 bench [覆盖补充]

- [ ] **步骤 1：Release build bench 目标**

```bash
cmake --build build-bench -j --target bench_imagecache 2>&1 | tail -5
```

- [ ] **步骤 2：跑 bench 输出 JSON**

```bash
./build-bench/benchmarks/bench_imagecache --benchmark_format=json \
  > /tmp/bench_imagecache_post.json 2>&1
```

- [ ] **步骤 3：smoke 三件套自检**（writing-plans.mdc §"性能基准任务必检项 §4"）

```bash
# (a) 数字非零 + (b) items_per_second > 0
jq '.benchmarks[] | select(.items_per_second == 0)' /tmp/bench_imagecache_post.json
# (c) library_build_type=release
jq '.context.library_build_type' /tmp/bench_imagecache_post.json
```

预期：
- (a)/(b) 空输出（无任何 0 项）
- (c) `"release"`

- [ ] **步骤 4：K6 量化判定**

```bash
# Hit<16> 和 Hit<256> 的 cpu_time
jq '.benchmarks[] | select(.name | test("Load_Hit")) | {name, cpu_time}' /tmp/bench_imagecache_post.json
```

预期（与 TASK-09 baseline 对比）：

| BM | 旧 | 新 | 判定 |
|------|-----|-----|------|
| `BM_ImageCacheLoad_Hit<1>` | 10.35 ns | < 30 ns | ✅ 不退化 |
| `BM_ImageCacheLoad_Hit<16>` | 50.87 ns | **< 50 ns** | ✅ K6 部分修复 |
| `BM_ImageCacheLoad_Hit<256>` | 1151.77 ns | **< 100 ns** | ✅ K6 完全修复 |
| `BM_ImageCacheLoad_Miss` | （baseline 数字） | < baseline × 1.2 | ✅ 不退化 |
| `BM_ImageCacheGet` | （baseline 数字） | < baseline × 1.2 | ✅ 不退化 |

> **若 Hit<256> 仍 > 100 ns 但已 < 200 ns**：仍接受（已是 ~6× 改善），记录到 progress 调查 hash 函数 / 容量阶梯效应。
> **若任何 BM 退化 ≥ 1.2×**：阻塞，回 P2 排查。

- [ ] **步骤 5：覆盖入仓 baseline JSON**

```bash
cp /tmp/bench_imagecache_post.json benchmarks/baseline/bench_imagecache.json
```

- [ ] **步骤 6：更新 `benchmarks/baseline/README.md`** 加 K6 修复证据

定位「Key findings (TASK-09)」段，在末尾追加：

```markdown
**K6 (resolved by TASK-20260419-11):** `ImageCache::Load` hit path now O(1) via
HashMap. Hit<16> drops from 50.87 ns → ~XX ns (Y×↓); Hit<256> drops from
1151.77 ns → ~XX ns (Z×↓). The earlier "size=256 cache hit slower than
ReplayImageReal<16>" anomaly is gone.
```

填入实际数字。

- [ ] **步骤 7：更新 `benchmarks/README.md`** 同样加 K6 修复行

定位 ImageCache 量级行（TASK-09 K6 描述行），在其后追加：

```markdown
- **K6 (TASK-20260419-11)**: ImageCache::Load hit path is now O(1) via HashMap.
  Hit<256> = ~XX ns (down from 1151.77 ns; ~Y×↓).
```

- [ ] **步骤 8：提交**

```bash
git add benchmarks/baseline/bench_imagecache.json benchmarks/baseline/README.md benchmarks/README.md
git commit -m "docs(bench): TASK-20260419-11 phase-3 K6 baseline + READMEs

- bench_imagecache.json 同机复跑覆盖入仓（library_build_type=release ✅）
- Hit<256> 从 1151.77 ns → ~XX ns（YY×↓），K6 量化命题已解
- Hit<16> 从 50.87 ns → ~XX ns；Miss / Get 不退化
- baseline/README + benchmarks/README 加 K6 修复证据行"
```

---

## Phase 4 — techContext + MB 收尾

**目标：** 长期知识库 `techContext.md` 更新 K6 状态；activeContext / tasks / progress 推进到「构建完成」。

### 任务 4.1：更新 `memory-bank/techContext.md`

- [ ] **步骤 1：定位「Replay-Deepbench 性能基线」段 K6 描述行**

```bash
rg "K6.*Load|ImageCache.*Load.*O\(N\)" memory-bank/techContext.md
```

- [ ] **步骤 2：StrReplace 把 K6 行改为「已解决」状态**

将类似：
```
**K6（待优化）**：ImageCache::Load hit 路径 O(N) ...
```
改为：
```
**K6（TASK-20260419-11 已解决）**：ImageCache::Load hit 路径已升级为 HashMap O(1)，Hit<256> 1151.77 ns → ~XX ns（YY×↓）；详见 `archive-TASK-20260419-11.md`
```

### 任务 4.2：更新 `memory-bank/tasks.md`

- [ ] **步骤 1：定位 TASK-11 当前任务段，状态从「初始化」推进到「构建完成」**
- [ ] **步骤 2：勾选所有 10 个验收项 ✅**

### 任务 4.3：更新 `memory-bank/activeContext.md`

- [ ] **步骤 1：阶段更新为「构建完成」**

### 任务 4.4：更新 `memory-bank/progress.md`

- [ ] **步骤 1：里程碑表勾选 P1/P2/P3/P4 ✅**

### 任务 4.5：finalize commit

```bash
git add memory-bank/techContext.md memory-bank/tasks.md memory-bank/activeContext.md memory-bank/progress.md
git commit -m "chore(build): finalize TASK-20260419-11 memory bank state (phase-4)

- techContext.md: K6 状态从「待优化」改为「TASK-11 已解决」
- tasks.md: TASK-11 当前任务段所有 10 项验收勾选 ✅
- activeContext.md: 阶段从「构建中」推进到「构建完成」（待 /reflect）
- progress.md: 4 个 phase 里程碑全部勾选

下一步：使用 /reflect 进行回顾"
```

---

## §3 完成验证（writing-plans.mdc §"完成验证"）

P4 之后，输出构建完成报告前**必须**重跑：

| 验证项 | 命令 | 预期 |
|--------|------|------|
| Debug 全量 ctest | `cd build && ctest -j --output-on-failure` | **891/891 passed** |
| Release `-Werror` 全量 build | `cmake --build build-bench -j` | 零 `-Werror` |
| 13 bench 全部 exit 0 | 抽 3 个跑 `--benchmark_min_time=0.001s` smoke | 全 exit 0 |
| linter 全清 | `ReadLints` on 修改文件 | 无新增 lint |
| 计划项逐项核对 | 重读本文档 §1 文件结构表 | 全部完成 ✅ |

---

## §4 不需要依赖安全审计

- 未引入新依赖（`HashMap` 已是 vx_core 内部成熟容器）
- 未升级现有依赖
- 不触发 `npm audit` / `pip audit` 等

---

## §5 提交清单（预计 5 commits 在 feature 分支上）

| # | Phase | 主题 |
|---|-------|------|
| 0 | VAN | `e7a9162` docs(van): TASK-20260419-11 init imagecache hashmap optimization（已存在） |
| 1 | P1 | `wip(TASK-20260419-11): phase-1 add HashMap field for double-index` |
| 2 | P2 | `feat(image): O(1) ImageCache::Load via HashMap (TASK-20260419-11 phase-2)` |
| 3 | P3 | `docs(bench): TASK-20260419-11 phase-3 K6 baseline + READMEs` |
| 4 | P4 | `chore(build): finalize TASK-20260419-11 memory bank state (phase-4)` |

---

## §6 风险预案

| 风险 | 触发条件 | 预案 |
|------|---------|------|
| HashMap 模板实例化失败（F2 风险） | P1 编译报错 | 检查 String 是否需要无参 ctor、`==` operator；如必要在 `string.h` 加 `operator==(BasicString, BasicString)`，但本任务范围内只用自定义 StringEq 兜底，不应触发 |
| Clear-then-Load 测试 P1 阶段就 PASS（O(N) 扫描已正确处理 vector clear） | 实测 | 接受。该测试在 P2 才是真正的回归安全网（验证 Clear 同步清 HashMap） |
| Hit<256> 改善小于预期（如仅 5×↓） | bench 数字不达 < 100 ns 但 < 200 ns | 接受（已是巨大改善），progress 记录 + 排查容量阶梯/hash 函数 |
| Hit<256> 改善达不到 < 200 ns | 严重退化 | 阻塞，回 P2 检查 HashMap.Find 实际行为；可能是 std::hash<usize> 路径未走自定义 hash |
| Miss 退化 ≥ 1.2× | bench 实测 | 检查是否 String(path) 拷贝代价过高，考虑改用 in-place `path_to_handle_.Find(String(path))` 临时构造（已经如此） |
| Get 退化 | 不应发生（Get 实现完全不变） | 若实测有退化，怀疑 inlining/cache 行为变化，记录到 progress 但接受（Get 不在 K6 范围） |

---

## 计划完成。准备执行？使用 `/build` 命令进入构建阶段。
