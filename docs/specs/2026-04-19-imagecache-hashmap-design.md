# 设计规格：ImageCache::Load HashMap 化（K6 高 ROI 优化）

**任务 ID：** TASK-20260419-11
**日期：** 2026-04-19
**复杂度级别：** Level 2
**来源：** TASK-20260419-09 K6 量化拆出（`bench_imagecache.json` Hit<256> 1151.77 ns > `ReplayImageReal<16>` 595 ns）
**预期工作量：** ~1-2h（4 phase / ~55-80 min plan 估时；按 TASK-09 4× 高估经验可能 30-45 min）

---

## 1. 目标

将 `vx::image::ImageCache::Load(StringView path)` 的 hit 路径从 O(N) 字符串线性扫升级为 O(1) HashMap 查找，**同时保持现有 ABI / 测试契约不变**：

| 接口 | 现状 | 改造后 |
|------|------|--------|
| `Load(StringView path) -> StatusOr<ImageHandle>` | hit O(N)（line 8-12 for-loop string compare） | **hit O(1)（HashMap.Find）** |
| `Get(ImageHandle handle) -> const Image*` | O(1)（`images_[handle-1]`） | 完全不变（仍 O(1)） |
| `Clear()` | O(N)（vector clear） | O(N)（vector clear + hashmap clear） |
| `size() const` | O(1) | 完全不变 |
| `gfx::ImageHandle` 语义 | 1-based vector 下标 | **保持** 1-based vector 下标（关键 ABI） |
| `DeduplicateSamePath` 测试契约 | 同 path Load 返回相同 handle | **保持** |

### 量化目标（与 TASK-09 baseline 对比）

| BM | 当前（TASK-09 baseline，单机） | 目标 | 改善 |
|------|------|------|------|
| `BM_ImageCacheLoad_Hit<1>` | 10.35 ns | ~10-30 ns | 不退化 |
| `BM_ImageCacheLoad_Hit<16>` | 50.87 ns | ~10-30 ns | **~2-5×↓** |
| `BM_ImageCacheLoad_Hit<256>` | 1151.77 ns | ~10-30 ns | **~38-115×↓** |
| `BM_ImageCacheLoad_Miss` | （不变，由 DecodeFromFile 主导） | 不退化 | — |
| `BM_ImageCacheGet` | （不变，O(1) 数组查表） | 不退化 | — |

---

## 2. 架构（双索引）

VAN 阶段 F1 grep 确认 `gfx::ImageHandle = u32` 是 **1-based vector 下标**（`Get(handle) = images_[handle-1]`）— 这是 ABI 契约，不能动。因此**纯 HashMap 替换不可行**，必须采用**双索引**：

```cpp
class ImageCache {
 public:
  StatusOr<gfx::ImageHandle> Load(StringView path);  // 接口不变
  const gfx::Image* Get(gfx::ImageHandle handle) const;  // 接口不变 + 实现不变
  void Clear();
  usize size() const { return images_.size(); }

 private:
  struct Entry { String path; gfx::Image image; };

  // 主存储：handle -> Entry，handle = index + 1（保 ABI）
  Vector<Entry> images_;

  // 反向索引：path -> handle，O(1) hit 路径（K6 修复目标）
  struct StringHash {
    usize operator()(const String& s) const {
      // djb2，复刻自 veloxa/core/css/property.cc:84-92 StringViewHash
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
  HashMap<String, gfx::ImageHandle, StringHash, StringEq> path_to_handle_;
};
```

**为什么不用 `StringView` key**（VAN F2 验证）：`BasicString` 有 SSO（短串内联到对象内部，`string.h:42-46`），vector resize 时 `Entry` 移动 → 指向 entry path 的 StringView 悬挂。owned `String` key 多一次拷贝（~25 ns），代价远低于安全收益。

**为什么不用 `std::equal_to<String>`**（VAN F2）：`string.h:178/190` 仅有 `operator==(BasicString, StringView)` 跨类型版本，没有 `operator==(BasicString, BasicString)` 直接版本。`std::equal_to<String>::operator()` 实例化时需要 `String == String`，可能编译失败（依赖隐式 String→StringView 转换是否能解析）。**自定义 `StringEq` 兜底**避免这一类型推导风险。

---

## 3. 注入点核对表（writing-plans.mdc §"管线注入点代码级可行性验证"）

| 改造点 | 实际代码 | 数据可访问性 | 可写性 | 结论 |
|--------|---------|------------|--------|------|
| `Load` hit 路径 | `image_cache.cc:7-12`（for-loop） | path 已传入 | 函数体可任意改 | ✅ 直接替换为 `path_to_handle_.Find(String(path))` |
| `Load` insert 路径 | `image_cache.cc:19-23`（push_back） | entry.path / handle 都已构造 | 函数体可任意改 | ✅ push_back 后追加 `path_to_handle_.Insert(entry.path, handle)`，**注意**：必须在 vector entry 构造后用 entry.path 作为 key（不是入参 path），保持 String 与 entry path 一致 |
| `Get` 路径 | `image_cache.cc:26-31` | 不变 | 不变 | ✅ 完全不动 |
| `Clear` 路径 | `image_cache.cc:33` | 完全 owner | 函数体可任意改 | ✅ 加一行 `path_to_handle_.clear()` |

---

## 4. 边界输入清单（writing-plans.mdc §"边界输入清单"）

| 类别 | 示例 | 预期行为 | 测试覆盖 |
|------|------|---------|---------|
| 默认路径 | `/tmp/vx_cache_test_<pid>.png` | hit / miss 行为按现状 | `LoadAndGet`（已有） |
| Dedup 同 path | 同一 path 连续 Load 2 次 | 第 2 次返回相同 handle，size = 1 | `DeduplicateSamePath`（已有） |
| 不存在路径 | `/nonexistent.png` | Load 失败，size = 0，HashMap 未 insert | `LoadInvalidPath`（已有） |
| **Clear 后重新 Load**（D3 新加） | Load → Clear → 同 path Load | Clear 后 size = 0；重新 Load 返回 handle = 1（不是旧 handle）；再 Load 同 path 仍 dedup | **`ClearAndReloadDeduplicates`（新加）** |
| 空字符串 path | `Load("")` | 由 DecodeFromFile 决定（不存在文件 → 错误） | 现有 `LoadInvalidPath` 已覆盖错误返回路径，不必额外加 |
| 超长 path | 1KB 字符串 | DecodeFromFile 失败（文件不存在）；HashMap key 拷贝多花 ~µs | 不在本任务关注范围 |

---

## 5. 调用链端到端验证（writing-plans.mdc §"调用链端到端验证"）

仅 `Load` 函数体内部行为变化，**接口签名不变**，无需透传。

```bash
# 验证调用方未引入新依赖
$ rg "image_cache" --type cpp | grep -v "_test.cc" | grep -v "veloxa/core/image/image_cache"
veloxa/core/application.h:9:#include "veloxa/core/image/image_cache.h"
veloxa/core/application.h:67:  image::ImageCache image_cache_;
veloxa/core/application.cc:118:  cfg.layout_context.image_cache = &image_cache_;
veloxa/core/application.cc:119:  cfg.image_cache = &image_cache_;
benchmarks/bench_imagecache.cc:...  # bench 只用公开 API
```

唯一调用方 `application.cc` 仅持有指针 + 引用传递，无 handle 数值递增依赖 → **回归风险接近零**。

---

## 6. CMake 链接方向（writing-plans.mdc §"CMake 链接方向约束分析"）

无变化：

- `image_cache.h` 已经 include `veloxa/foundation/containers/vector.h` + `veloxa/foundation/strings/string.h`
- 新加 `veloxa/foundation/containers/hash_map.h` 是同 module（`vx_core` PUBLIC headers），无新链接依赖
- 测试 `image_cache_test` 已经链 `vx_core`，无变化
- bench `bench_imagecache` 已经链 `vx_core PNG::PNG`，无变化

---

## 7. 关键决策汇总（D1-D4，plan 头脑风暴用户确认）

| # | 维度 | 选择 | 理由 |
|---|------|------|------|
| **D1** | HashMap key 类型 | **owned `String`** + 自定义 `StringHash` + `StringEq` | VAN F2 grep：BasicString SSO 内联 + Vector resize 会移动 entry → StringView 悬挂；`std::equal_to<String>` 类型推导不确定 → 自定义兜底 |
| **D2** | HashMap 初始容量 | 沿用 `kDefaultCapacity = 16`，不 reserve | 生产场景 ImageCache 不会频繁增长；TASK-09 K6 baseline 极端 256 entry 也只是 ~4 次 rehash（~10 µs 总代价），分摊到 dedup 命中收益后净收益巨大；避免为小规模场景预付费 |
| **D3** | 是否新加边界测试 | **加 `ClearAndReloadDeduplicates`**（~5 行 GTest） | 双索引最易出 bug 的点 = `Clear()` 漏清 HashMap → Clear 后 Load 同 path 错误命中旧 handle；这个测试是「双索引同步」的安全网 |
| **D4** | Phase 划分 | **4 phase**（细粒度提交）：P1 仅加字段 / P2 切 Load 路径 / P3 bench 复跑 / P4 文档 | Incremental 改造模式：每步可独立 review + 独立回滚；P1 引入字段不影响行为，是「数据结构改造与行为切换分离」的安全模式 |

---

## 8. 验收标准（10 项）

1. ✅ 双索引数据结构（`Vector<Entry>` + `HashMap<String, ImageHandle, StringHash, StringEq>`）实现完成，编译通过
2. ✅ `Load` hit 路径走 `HashMap::Find`（O(1)），O(N) for-loop 被消除
3. ✅ `Load` insert 路径同步插入 `Vector` 与 `HashMap`，handle 仍是 1-based vector 下标（`gfx::ImageHandle = u32`）
4. ✅ `Get(handle)` 实现完全不变（仍 `&images_[handle - 1].image`），O(1)
5. ✅ `Clear()` 同步清空两个容器
6. ✅ 现有 3 个 GTest 全绿（`LoadAndGet` / `DeduplicateSamePath` / `LoadInvalidPath`）
7. ✅ 新增 `ClearAndReloadDeduplicates` GTest 通过
8. ✅ ctest 全量回归 890/890 → **891/891**（仅 +1 用例，无回归）
9. ✅ Release `-Werror` 通路零编译失败
10. ✅ `bench_imagecache` 同机复跑：**Hit<16> < 50 ns** 且 **Hit<256> < 100 ns**（K6 量化命题"size=256 时 1162 ns"已解）；Miss / Get 不退化；baseline JSON 重生成入仓

---

## 9. 风险与对策

| # | 风险 | 概率 | 对策 |
|---|------|------|------|
| 1 | `std::equal_to<String>` 实例化失败编译 | 已规避 | D1 已用自定义 `StringEq`，不依赖 `std::equal_to` |
| 2 | Vector resize 时 String 移动导致 HashMap key 与 entry 不一致 | 已规避 | HashMap 持有的是**自己的** String 副本（owned key），与 entry.path 是两个独立对象 |
| 3 | Clear 后 HashMap 漏清 → 后续 Load 错误命中旧 handle | **D3 新测试覆盖** | `ClearAndReloadDeduplicates` 是回归安全网 |
| 4 | bench 复跑数字偏差太大，无法判定 K6 已解 | 低 | TASK-09 baseline 数据来自同机；bench 阈值（< 50 / 100 ns）远低于当前（50 / 1152 ns），偏差容忍度大 |
| 5 | HashMap rehash 异常导致状态半新半旧 | 低 | HashMap 是成熟容器（已被 GlyphCache / PropertyMap 长期使用）；improbable. 若发生 → ctest dedup 测试会捕获 |
| 6 | 修改触发 Release IPA 误报（如 TASK-04/07 同类） | 低 | 仅新增私有字段 + 修改 4 行函数体，无模板特化 / 无运行时尺寸 memcpy；与历史误报模式不同 |

---

## 10. 不需要 `/creative` 阶段

理由：
- 数据结构空白已由 D1（key 类型 + Hash/Eq 选择）完整闭合
- 算法空白已由 §2 双索引方案明确
- 接口空白：无（Load/Get/Clear 全部接口不变）
- UI 空白：无（库代码）
- Phase 划分已由 D4 确定

---

## 11. 与 TASK-09 / TASK-12 的关系

- **TASK-09 K6 → 本任务（TASK-11）**：本任务是 K6 直接修复
- **TASK-09 K7 → TASK-12（候选）**：与本任务**完全独立**，不会冲突；TASK-12 触发条件是「真路径默认化提上日程」，目前未触发
- **TASK-09 K2/K3 + VAN → TASK-10（候选）**：研究类，与本任务**完全独立**

本任务完成后：
- K6 关闭，`techContext.md`「Replay-Deepbench 性能基线」段需更新（K6 状态从"待优化"→"已解决"）
- `activeContext.md` 候选区 TASK-11 划线（已立项 → 已完成）
