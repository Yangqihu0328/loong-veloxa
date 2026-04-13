# 技术债务子集（#41 + #39）实现计划

**目标：** 为 `HashMap` 增加 `const_iterator` / `cbegin` / `cend`，移除 `TransitionManager` 中因缺少 const 遍历而引入的 `active_count_`；新增 `tests/test_pixel_utils.h` 并在至少两个测试中复用，关闭 `techContext` 对应条目。

**架构：** `ConstIterator` 与现有 `Iterator` 对称；`HasActive() const` 改为 O(n) 扫描 `!tr.completed`；像素工具为头文件内联函数，语义与 `gfx::Color::ToRGBA32()` / `techContext` RGBA32 一致。

**技术栈：** C++17、GTest。

**复杂度级别：** Level 2（范围已锁定；若中途扩大范围须返回 `/plan`）

**关联设计：** `docs/specs/2026-04-13-tech-debt-subset-design.md`

---

## 文件结构

| 路径 | 职责 |
|------|------|
| `veloxa/foundation/containers/hash_map.h` | `[共享头]` 增加 `ConstIterator`、`const_iterator`、`begin() const`、`end() const`、`cbegin()`、`cend()` |
| `tests/foundation/containers/hash_map_test.cc` | const 迭代与 `cbegin`/`cend` 测试 |
| `veloxa/core/css/transition.h` | 删除 `active_count_` 成员 |
| `veloxa/core/css/transition.cc` | 删除计数维护；`HasActive` 扫描 |
| `tests/test_pixel_utils.h` | **新建** — `PixelR`/`PixelG`/`PixelB`/`PixelA` |
| `tests/api/api_integration_test.cc` | 删除本地 Pixel 内联，改为 `#include` 公共头 |
| `tests/platform/memory_surface_test.cc` | 引入公共头，至少一处断言改用 `PixelR`… |
| `memory-bank/systemPatterns.md` | 删除或改写「active_count_ 计数器替代 const 遍历」过时段落 |
| `memory-bank/techContext.md` | 将 #39、#41 标为已解决或附 ✅ 注记（与实现一致） |

**不修改：** `CMakeLists.txt`（无新目标）；不在本计划内全仓替换所有像素断言。

---

## 技术验证

- `TransitionManager` 已有 `transition_test.cc` / `transition_integration_test.cc` 覆盖 `HasActive` — 行为须 **保持绿色**。  
- `HashMap` 仅头文件 — 修改后全量重编 `vx_foundation` 消费者。

---

## 任务列表

### 任务 1：`HashMap` 常量迭代器 [TDD]

**文件：**
- 修改：`veloxa/foundation/containers/hash_map.h` `[共享头]`
- 修改：`tests/foundation/containers/hash_map_test.cc`

**步骤：**

- [ ] **步骤 1：编写失败测试**（在 `HashMapTest` 末尾增加）：

```cpp
TEST(HashMapTest, ConstIteration) {
  HashMap<int, int> m;
  m.Insert(1, 10);
  m.Insert(2, 20);
  const HashMap<int, int>& cm = m;
  int sum = 0;
  for (const auto& slot : cm) {
    sum += slot.key + slot.value;
  }
  EXPECT_EQ(sum, 33);  // (1+10) + (2+20)
  int c = 0;
  for (auto it = m.cbegin(); it != m.cend(); ++it) {
    ++c;
    EXPECT_TRUE(it->key == 1 || it->key == 2);
  }
  EXPECT_EQ(c, 2);
}
```

- [ ] **步骤 2：** `cmake --build build --target hash_map_test && ./build/tests/hash_map_test --gtest_filter=HashMapTest.ConstIteration`  
  **预期：** FAIL（编译失败或链接失败均可接受为 RED）。

- [ ] **步骤 3：实现** — 在 `Iterator` 定义之后、`using iterator = Iterator;` 之前插入 `ConstIterator` 类（与 `Iterator` 相同逻辑，`const u8* ctrl_`、`const Slot* slots_`，`operator*` 返回 `const Slot&`）。在 `begin()`/`end()` 之后增加：

```cpp
  using const_iterator = ConstIterator;

  const_iterator begin() const {
    return const_iterator(ctrl_, slots_, 0, capacity_);
  }
  const_iterator end() const {
    return const_iterator(ctrl_, slots_, capacity_, capacity_);
  }
  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }
```

（`ConstIterator` 类体从 `Iterator` 复制并改 `const` 限定，私有 `AdvancePastEmptyOrDeleted` 同步。）

- [ ] **步骤 4：** 运行 `hash_map_test` → **PASS**。

- [ ] **步骤 5：** `git commit -am "feat(foundation): HashMap const_iterator and cbegin/cend"`

---

### 任务 2：移除 `TransitionManager::active_count_` [TDD] `[影响前序测试]`

**文件：**
- 修改：`veloxa/core/css/transition.h`
- 修改：`veloxa/core/css/transition.cc`

**说明：** 不改变对外 API；依赖现有 `tests/core/css/transition_test.cc` 与 `tests/core/transition_integration_test.cc` 为回归网。

**步骤：**

- [ ] **步骤 1：** 删除 `transition.h` 中 `usize active_count_ = 0;`。

- [ ] **步骤 2：** 在 `transition.cc` 中删除所有 `active_count_` 的自增/自减。

- [ ] **步骤 3：** 将 `HasActive` 实现为：

```cpp
bool TransitionManager::HasActive() const {
  for (const auto& slot : transitions_) {
    for (const auto& tr : slot.value) {
      if (!tr.completed) {
        return true;
      }
    }
  }
  return false;
}
```

- [ ] **步骤 4：** `Clear()` 中删除 `active_count_ = 0;`（`transitions_ = {}` 已足够）。

- [ ] **步骤 5：** `cmake --build build --target transition_test transition_integration_test && ctest -R transition`  
  **预期：** PASS。

- [ ] **步骤 6：** `git commit -am "refactor(css): derive HasActive without active_count_"`

---

### 任务 3：`tests/test_pixel_utils.h` 与两处迁移 [TDD]

**文件：**
- 创建：`tests/test_pixel_utils.h`
- 修改：`tests/api/api_integration_test.cc`
- 修改：`tests/platform/memory_surface_test.cc`

**步骤：**

- [ ] **步骤 1：** 创建 `tests/test_pixel_utils.h`：

```cpp
#ifndef VELOXA_TESTS_TEST_PIXEL_UTILS_H_
#define VELOXA_TESTS_TEST_PIXEL_UTILS_H_

#include "veloxa/foundation/base/types.h"

namespace vx {
namespace test {

// RGBA32 与 gfx::ToRGBA32 / SoftwareCanvas 一致：R 低字节 … A 高字节
inline u8 PixelR(u32 rgba) { return static_cast<u8>(rgba & 0xFFu); }
inline u8 PixelG(u32 rgba) { return static_cast<u8>((rgba >> 8) & 0xFFu); }
inline u8 PixelB(u32 rgba) { return static_cast<u8>((rgba >> 16) & 0xFFu); }
inline u8 PixelA(u32 rgba) { return static_cast<u8>((rgba >> 24) & 0xFFu); }

}  // namespace test
}  // namespace vx

#endif  // VELOXA_TESTS_TEST_PIXEL_UTILS_H_
```

- [ ] **步骤 2：** `api_integration_test.cc`：删除文件顶部 `PixelR/G/B` 内联，改为  
  `#include "tests/test_pixel_utils.h"`，断言改为 `vx::test::PixelR(center)` 等。

- [ ] **步骤 3：** `memory_surface_test.cc`：  
  `#include "tests/test_pixel_utils.h"`，在 `LockReturnsValidPointer` 中增加：  
  `EXPECT_EQ(vx::test::PixelR(pixels[0]), 255);` 等（与现有 `0xFF0000FF` 一致），保留对整字 `EXPECT_EQ` 若冗余可二选一，**至少**一条通道级断言。

- [ ] **步骤 4：** 构建 `api_integration_test`、`memory_surface_test` 并运行 → PASS。

- [ ] **步骤 5：** `git commit -am "test: add test_pixel_utils and reuse in api/memory_surface"`

---

### 任务 4：文档与知识库收尾

- [ ] **步骤 1：** 更新 `memory-bank/systemPatterns.md` — 删除或改写「### active_count_ 计数器替代 const 遍历模式」段落，改为「HashMap 提供 const 迭代后，`HasActive` 可直接扫描」。  
- [ ] **步骤 2：** 更新 `memory-bank/techContext.md` 技术债务清单 — #39、#41 打 ✅ 或附「TASK-20260413-02」简短说明。  
- [ ] **步骤 3：** 全量 `ctest`（或 `cmake --build build && ctest`）。  
- [ ] **步骤 4：** `git commit -am "docs(memory-bank): mark tech debt #39 #41 addressed"`

---

## 需要 `/creative` 的组件

**无** — 标准容器迭代器与测试工具提取，无产品级分叉决策。

---

## 执行交接

计划保存至 `docs/plans/2026-04-13-tech-debt-subset.md`。设计规格请审查批准后执行 **`/build`**（建议分支 `feature/TASK-20260413-02-tech-debt`）。
