# 技术债务子集 — 设计规格

**任务 ID：** TASK-20260413-02  
**日期：** 2026-04-13  
**状态：** 草案（请审查；批准前不进入 `/build`）

---

## 1. 范围锁定（`techContext` 条目）

本迭代**仅**处理以下两项及直接派生的代码清理：

| 编号 | 条目 | 验收标准 |
|------|------|----------|
| **#41** | HashMap 缺少 `const_iterator` / `cbegin` / `cend` | `HashMap` 提供 `const_iterator`、`begin() const` / `end() const`、`cbegin()` / `cend()`；`const HashMap` 上可范围 for；`hash_map_test` 新增覆盖 |
| **#39** | 像素测试缺少标准化工具 | 新增 `tests/test_pixel_utils.h`，提供 `PixelR/G/B/A(u32 rgba)`（与 `techContext` RGBA32 约定一致）；**至少 2** 个现有测试文件改为使用该头文件 |

**明确不做：** 技术债务清单中其余条目（parser 拆分、margin collapsing、Benchmark 等）留待后续 TASK。

---

## 2. 方案对比

### 2.1 HashMap 常量迭代

| 方案 | 说明 | 取舍 |
|------|------|------|
| **A. 嵌套 `ConstIterator` 类** | 与现有 `Iterator` 平行，`*`/`->` 返回 `const Slot&` | **推荐**：与当前风格一致，行为清晰 |
| **B. 模板化单一 Iterator** | `Iterator<SlotQual>` | 复杂度高，本任务不采用 |

### 2.2 `TransitionManager::HasActive`

| 方案 | 说明 | 取舍 |
|------|------|------|
| **A. 保留 `active_count_`** | 零行为变更 | 不消除 #41 的根因 |
| **B. 删除计数器，用 `const` 遍历判断是否存在 `!completed`** | 依赖 #41 的 `const` 迭代 | **推荐**：与 `archive-TASK-20260405-13` 中「计数器为权宜」叙述对齐；`Tick`/`OnStyleChange` 删除所有 `active_count_` 维护 |

### 2.3 像素工具

| 方案 | 说明 | 取舍 |
|------|------|------|
| **A. 头文件内联 `PixelR`…** | 测试专用命名空间 `test` 或匿名 | **推荐**：零链接成本，与现有 `tests/` 风格兼容 |

---

## 3. 安全与正确性

- **无**新增不可信输入面；HashMap 迭代器不改变表结构语义。
- `HasActive` 改为扫描后须与现有 **`transition_test` / `transition_integration_test`** 行为一致（同一验收）。

---

## 4. 审查

- [ ] 同意范围仅为 **#41 + #39**（及 `active_count_` 移除）。  
- [ ] 同意 `PixelR/G/B/A` 按 **R[0:7], G[8:15], B[16:23], A[24:31]** 定义。

**批准：** 回复「批准设计」或修订范围。

---

## 5. 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | 2026-04-13 | `/plan` 初稿 |
