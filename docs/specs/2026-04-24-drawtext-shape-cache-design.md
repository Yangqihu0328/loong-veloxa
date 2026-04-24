# TASK-20260424-04 设计规格：DrawText hb_shape 结果缓存

**任务：** TASK-20260424-04 — SoftwareCanvas::DrawText 真路径 warm 残余优化（D 纯收尾模式）
**日期：** 2026-04-24
**阶段：** 规划中（/plan 头脑风暴产出）
**批准：** 用户批准（Q1 = 方案 B / Q2 = K2 / Q3 = S2 / Q4 = C1 / Q5 = B1）

---

## 1. 目标与范围

### 1.1 性能目标（D 纯收尾模式）

| BM | baseline（TASK-24-03 归档值）| 目标 | 倍率 |
|---|---:|---:|---|
| `BM_DrawTextReal_Warm_Medium` | 3499 ns | **< 3200 ns** | -8.5% / -299 ns |
| `BM_DrawTextReal_Warm_Short` | 677 ns | 不回归（≤ 744 ns = 677 × 1.1）| 兜底 |
| `BM_DrawTextReal_Warm_Long` | 10573 ns | 不回归（≤ 11630 ns = 10573 × 1.1）| 兜底 |
| `BM_DrawTextReal_Cold_Medium` | 28338 ns | 不回归（≤ 31172 ns = 28338 × 1.1）| 兜底 |
| `BM_DrawTextReal_Warm_TextVarying_RoundRobin`（**新**）| N/A → 先测**无 cache** pre-baseline | ≤ **pre-baseline +5% 或 +50 ns**（取严）| 新 cache-miss 护栏 |
| `BM_DrawTextReal_Warm_TextVarying_AllMiss`（**新 / 参考**）| N/A → 先测**无 cache** pre-baseline | 入 baseline CSV，不计门槛 | 最坏边界记录 |

### 1.2 范围

**In scope：**
- `SoftwareCanvas::DrawText` 真路径的 `hb_shape` 调用结果缓存（candidate (c)）
- 新增 `ShapeCache` 组件 + FontManager 集成
- 2 个新 BM（RoundRobin 门槛 + AllMiss 参考）
- 单元 + 集成 + 反向探针测试

**Out of scope（本 task 不做）：**
- `FreeTypeTextShaper::Measure()` 的 `hb_shape`（独立路径，不在 DrawText warm bench 目标窗口内）
- candidate (a) SIMD 块级 zero-skip fast path（VAN Q1 方案 B 决策放弃 — 收益接近 0）
- candidate (b) GlyphCache row_ptr 数组（TASK-24-03 已优化过，剩余空间 ≤ 20 ns）
- 跨进程 / 跨 FontManager 共享 cache
- LRU 淘汰（YAGNI；FIFO 128 足够）
- 线程安全（FontManager 非线程共享约定由 techContext.md 背书）

---

## 2. 架构

```
DrawText(canvas, text, font, px, x, y, r, g, b, a)
   │
   ├─► GlyphCache::GetOrLoad(...)                  // 已优化（TASK-24-03）
   │
   ├─► FontManager::ShapeOrLookup(font, px, text)  ◄── 新增入口
   │        │
   │        ├── key = {font_handle, pixel_size, xxhash_u64(text), text.size()}
   │        │
   │        ├── ShapeCache::LookupOrNull(key)
   │        │      ├── hit   → return cached ShapedRun*
   │        │      └── miss  → fall through
   │        │
   │        └── hb_buffer_reset + add_utf8 + hb_shape_full
   │                → 拷贝 glyph_info + glyph_position 到 Vector<ShapedGlyph>
   │                → FIFO insert（容量满覆盖 head_）
   │                → return ShapedRun*
   │
   └─► Per-glyph blit loop（已优化 SIMD，保留不变）
```

### 2.1 新增组件

| 组件 | 路径 | 职责 |
|---|---|---|
| `ShapedGlyph` | `veloxa/text/shape_cache.h`（新建） | `u32 glyph_id; f32 x_off, y_off, x_adv;` 一次 shape 后的单 glyph 结果 |
| `ShapedRun` | 同上 | `Vector<ShapedGlyph> glyphs;` 一次 shape 调用的完整结果 |
| `ShapeCacheKey` | 同上 | `FontHandle font; u32 pixel_size; u64 text_fp; u32 text_len;` |
| `ShapeCache` | `veloxa/text/shape_cache.h/.cc`（新建） | 固定 128 entry FIFO ring + lookup/insert/clear |
| `vx::HashBytesU64` | `veloxa/foundation/base/hash.h`（新建） | FNV-1a u64 byte-hash（10 行 constexpr，inline） |
| `FontManager::ShapeOrLookup(...)` | `veloxa/text/font_manager.{h,cc}` | 新 public API；持有 `ShapeCache` 成员 |

### 2.2 修改点

| 文件 | 改动摘要 |
|---|---|
| `veloxa/graphics/software/software_canvas.cc` | L216-231 段改为 `font_manager_->ShapeOrLookup(...)`；移除 `hb_buffer_reset/add_utf8/hb_shape`；改为遍历 `ShapedRun.glyphs` |
| `veloxa/text/font_manager.h` | + `ShapeOrLookup` + 持有 `ShapeCache shape_cache_;` 成员 |
| `veloxa/text/font_manager.cc` | 实现 `ShapeOrLookup`（含 hb_buffer + hb_shape 调用） |
| `benchmarks/bench_drawtext.cc` | + 2 BM（RoundRobin / AllMiss），以及 256 text pool 初始化 |
| `veloxa/text/CMakeLists.txt` | + `shape_cache.h/.cc` sources【共享文件】 |
| `veloxa/foundation/base/CMakeLists.txt` | + `hash.h`（header only 可能无需改）【共享文件，检查】 |

---

## 3. 数据结构

### 3.1 ShapedGlyph + ShapedRun

```cpp
// veloxa/text/shape_cache.h
namespace vx::text {

struct ShapedGlyph {
  u32 glyph_id;
  f32 x_offset;
  f32 y_offset;
  f32 x_advance;
};

struct ShapedRun {
  // 本字段在 cache entry 内长寿命存储；DrawText 以 const ShapedRun* 使用，
  // 不可跨 FontManager 生命周期持有（见 §6.2 UAF 缓解）。
  Vector<ShapedGlyph> glyphs;
};

}  // namespace vx::text
```

### 3.2 ShapeCacheKey + Entry

```cpp
struct ShapeCacheKey {
  FontHandle font;
  u32 pixel_size;
  u64 text_fingerprint;
  u32 text_len;  // 碰撞降级护栏
};

struct ShapeCacheEntry {
  ShapeCacheKey key;
  ShapedRun run;
  bool occupied;  // 初始 false；Insert 后 true
};
```

### 3.3 ShapeCache

```cpp
class ShapeCache {
 public:
  static constexpr usize kCapacity = 128;
  ShapeCache();
  // hot path: O(N=128) 线性扫描；cache-line 预取友好；
  // 比 HashMap 在 N=128 下更快（单次 lookup ≈ 50-150 cycles）
  const ShapedRun* LookupOrNull(const ShapeCacheKey& key) const;
  // FIFO insert：未满追加到尾；已满覆盖 head_ 并 head_++
  ShapedRun* Insert(ShapeCacheKey key, ShapedRun run);
  void Clear();  // Font reload / unload 调用；保留容量预分配
  usize size() const { return count_; }

 private:
  Vector<ShapeCacheEntry> entries_;  // size == kCapacity 预分配
  usize head_ = 0;                   // 下一个被覆盖的 slot
  usize count_ = 0;                  // 当前有效条目数
};
```

### 3.4 HashBytesU64（FNV-1a u64）

```cpp
// veloxa/foundation/base/hash.h
namespace vx {
inline u64 HashBytesU64(const u8* data, usize len) {
  u64 h = 0xCBF29CE484222325ULL;  // FNV offset basis
  for (usize i = 0; i < len; ++i) {
    h ^= static_cast<u64>(data[i]);
    h *= 0x100000001B3ULL;  // FNV prime
  }
  return h;
}
}  // namespace vx
```

**理由：** FNV-1a 单次乘加，19 char 约 20 cycles ≈ 6 ns；远低于一次 `hb_shape` 成本 (~2000 ns)；分布足够 UI 场景；零依赖 / 单文件 header。

---

## 4. 公开 API

### 4.1 FontManager 新增

```cpp
class FontManager {
 public:
  // ... 已有成员 ...

  // Shape 文本（查缓存优先；miss 时调用 hb_shape_full 并缓存）。
  // 返回的 ShapedRun* 在下一次同 FontManager 上的 ShapeOrLookup / Clear /
  // FontManager 析构之前有效；调用方不应跨 DrawText 调用持有。
  //
  // 返回 nullptr：font 无效、hb_font 获取失败、text 为空。
  const ShapedRun* ShapeOrLookup(FontHandle font, u32 pixel_size,
                                  StringView text);

  // 测试辅助：强制清空 cache（font reload / 单元测试用）。
  void ClearShapeCache();

 private:
  // ... 已有字段 ...
  ShapeCache shape_cache_;
};
```

### 4.2 DrawText 改造片段（示意）

```cpp
// software_canvas.cc L216-231 前后对比

// Before (TASK-24-03):
hb_buffer_t* buf = AcquireThreadLocalHbBuffer();
if (!buf) { DrawTextFallback(...); return; }
hb_buffer_reset(buf);
hb_buffer_add_utf8(buf, text.data(), text.size(), 0, -1);
hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
hb_buffer_guess_segment_properties(buf);
hb_shape(hb_font, buf, nullptr, 0);
u32 glyph_count = 0;
hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
// ... per-glyph loop uses glyph_info[i].codepoint + glyph_pos[i].{x_offset, y_offset, x_advance}

// After (TASK-20260424-04):
const ShapedRun* run = font_manager_->ShapeOrLookup(font, pixel_size, text);
if (!run) { DrawTextFallback(...); return; }
const u32 glyph_count = static_cast<u32>(run->glyphs.size());
// ... per-glyph loop uses run->glyphs[i].{glyph_id, x_offset, y_offset, x_advance}
```

---

## 5. 关键行为

### 5.1 Hit 路径

1. 计算 `fp = HashBytesU64(text.data(), text.size())` — O(len) ≈ 6 ns (19 char)
2. 遍历 128 `entries_` 中 occupied 者（count_ ≤ 128）— 比较 `(font, pixel_size, fp, text_len)` 4 字段 — 平均 64 次 branch，**~100 cycles ≈ 30 ns worst case**
3. Hit → 返回 `&entry.run` — 0 分配
4. DrawText blit loop 直接用 `run->glyphs[]`

**预估 warm hit 路径节省（vs TASK-24-03 baseline）：**
- 节省：hb_buffer_reset (~100 ns) + add_utf8 (~200 ns) + set_direction/script/guess (~50 ns) + **hb_shape_full (~1800 ns)** = **~2150 ns**
- 引入开销：HashBytesU64 ~6 ns + 线性扫描 ~30 ns = ~36 ns
- **净收益：~ -2114 ns**
- 即使保守 50% 扣减（缓存结构 overhead、Vector 拷贝、cache miss 率非 100% 等），**净收益 ≥ -1000 ns**
- Medium baseline 3499 ns → 目标 < 3200 ns 差 299 ns —— **保守 3× 冗余达标空间**

### 5.2 Miss 路径

1. 计算 fingerprint + 线性扫描都完成（~36 ns）
2. 未命中 → 走原 hb_buffer + hb_shape_full 逻辑（~2150 ns）
3. 拷贝 glyph_info + glyph_pos 到新 `ShapedRun{Vector<ShapedGlyph>{glyph_count reserved}}`（glyph_count ~20 → ~100 ns alloc + copy）
4. `Insert(key, run)` — FIFO 覆盖或追加（~50 ns move）

**miss 路径新增开销：~200 ns**（fingerprint + 扫描 + 拷贝 + insert）
- 相对 baseline 3499 ns → **+5.7%**
- 门槛 B1 RoundRobin（50% hit）下：50% × 2150 节省 - 100% × 200 开销 = **-875 ns 净收益**（远好于 +5% 门槛）
- 最坏 B3 AllMiss 下：0% × 2150 - 100% × 200 = **+200 ns = +5.7%**
  - → 接近 +50 ns 门槛上限；**可能触发门槛事件，需在 /build Phase 3 验证**
  - 若 B3 超 +50 ns：方案 = 在 ShapeOrLookup 入口加**短文本跳过**（e.g. text.size() ≤ 2 直接 hb_shape 不入 cache；理由：short text 本身 < 700 ns，cache 收益被 overhead 吞掉）

### 5.3 Clear 路径

- 调用时机：
  - `FontManager::UnloadFont()` / 析构（当前无 UnloadFont，析构时自动）
  - `ShapeOrLookup` 内部**若 pixel_size ≠ cached ft_pixel_size 之前** → 无需 Clear（key 包含 pixel_size，自然 miss）
- 实现：`entries_[i].occupied = false; count_ = 0; head_ = 0;`（不释放 Vector 底层，保留预分配）

---

## 6. 错误处理 + 安全考量

### 6.1 失败降级矩阵

| 失败点 | 行为 |
|---|---|
| `font == kInvalidFont` | `ShapeOrLookup` 返回 nullptr → DrawText 走 fallback |
| `hb_font` 获取失败 | 同上 |
| `text.empty()` | 返回 nullptr（调用方已提前排查）|
| `hb_shape_full` 返回空 glyph | **不入缓存**（避免污染）；返回本次 run 的引用必须保证生命周期 → **直接返回 nullptr 走 fallback** |
| `Vector::reserve` OOM | 当前 Vector 实现不返回失败（techContext 确认）→ 不处理 |
| Cache entry 冲突（同 key 重复 Insert）| 简单覆盖（不做 dedup），依 FIFO 自然老化 |

### 6.2 安全面（轻量威胁建模 — 本任务已标 ❌ 非安全相关，此处仅核对）

| 威胁 | 风险 | 缓解 |
|---|---|---|
| **DoS 内存膨胀** | 攻击者构造大量不同 text → cache 无限增长 | **C1 FIFO 128 上限**，内存天花板 ~40 KB / FontManager（Vector<ShapedGlyph> × 128 ≈ 128 × (20 × 20 bytes) = 51 KB worst，典型 ~15-30 KB）|
| **Hash 碰撞侧信道 / 误命中** | 攻击者构造 fingerprint 相同但 text 不同的输入 → 返回错误 glyph | **双字段比较（fp + text_len）**；u64 × u32 组合碰撞概率 ~ 2^-96，实际不可达 |
| **UAF / 悬垂引用** | `DrawText` 持有 `const ShapedRun*`，但 cache Insert 导致 Vector realloc → 指针失效 | **`entries_` 预分配 kCapacity，size() 恒定不 realloc**；插入仅修改 entries_[i].{key, run, occupied}；返回指针始终指 entries_[i].run，稳定 |
| **Font 重载不一致** | FontHandle 复用但指向新 font → 老 cache 返回旧 shape | **当前 FontManager 无 reload 路径**（LoadFont 追加不复用）；新增 `ClearShapeCache()` 为未来 reload 预留 |
| **Key 构造溢出** | text.size() > u32_max → text_len 截断误命中 | text.size() 当前 `usize`（64-bit），`static_cast<u32>` 截断；加 early return `if (text.size() > 0xFFFF) return nullptr` 保守护栏 |

---

## 7. 测试策略（TDD）

### 7.1 单元测试（新建 `test/text/shape_cache_test.cc`）

| # | 测试名 | 类型 | 目的 |
|:-:|---|:-:|---|
| T1 | `ShapeCache_LookupEmpty_ReturnsNull` | RED→GREEN | 初始化空 cache lookup 返回 null |
| T2 | `ShapeCache_InsertThenLookup_ReturnsSame` | GREEN | 基础 insert/lookup 契约 |
| T3 | `ShapeCache_InsertBeyondCapacity_FIFOEvict` | RED→GREEN | 第 129 条目覆盖第 1 条目，第 1 lookup miss |
| T4 | `ShapeCache_KeyMismatch_Font_Miss` | GREEN | 同 text/px 不同 font → miss |
| T5 | `ShapeCache_KeyMismatch_PixelSize_Miss` | GREEN | 同 text/font 不同 px → miss |
| T6 | `ShapeCache_FingerprintEqLenMismatch_Miss` | RED→GREEN | 手动构造 fp 冲突但 len 不同 → miss（碰撞降级护栏）|
| T7 | `ShapeCache_Clear_EmptiesButKeepsCapacity` | GREEN | Clear 后 size==0，再 Insert 不 realloc |
| T8 | `HashBytesU64_EmptyInput_Basis` | GREEN | FNV basis 恒等性 |
| T9 | `HashBytesU64_DifferentByte_DifferentHash` | GREEN | avalanche 基础（不求强，仅求非退化）|

### 7.2 集成测试（新建 `test/graphics/drawtext_shape_cache_test.cc`）

| # | 测试名 | 目的 |
|:-:|---|---|
| I1 | `DrawText_SameTextTwice_PixelByPixelIdentical` | 第二次（hit 路径）与第一次（miss 路径）渲染 pixel-byte-identical |
| I2 | `DrawText_AfterClearShapeCache_Identical` | Clear 后再绘制与首次 miss 结果 identical |
| I3 | `DrawText_MultipleTextsInterleaved_Correct` | text_a → text_b → text_a 交错，结果稳定 |

### 7.3 反向探针（RED 守护，写入集成测试文件）

| # | 名称 | 机制 |
|:-:|---|---|
| R1 | `DrawText_CacheDisabled_PixelIdentical` | 编译宏 `VX_SHAPE_CACHE_DISABLE=1` 下绕过 ShapeOrLookup 直接 hb_shape，结果与 cache-enabled 完全 identical |
| R2 | `ShapeCache_DeliberateForcedCollision_StillMiss` | 构造两组 (text_a, text_b) 使 HashBytesU64 返回相同值但 size 不同（用已知 FNV-1a 碰撞）→ 验证 text_len 护栏生效 |

### 7.4 Benchmark

| BM | 类型 | 门槛 |
|---|---|---|
| `BM_DrawTextReal_Warm_Medium` | 已有 | **< 3200 ns**（主目标）|
| `BM_DrawTextReal_Warm_Short` / `_Long` | 已有 | ≤ baseline × 1.1（兜底）|
| `BM_DrawTextReal_Cold_Medium` | 已有 | ≤ baseline × 1.1（兜底）|
| **`BM_DrawTextReal_Warm_TextVarying_RoundRobin`（新）** | 新增 | 相对 pre-baseline（无 cache 版）**≤ +5% 或 +50 ns（取严）** |
| **`BM_DrawTextReal_Warm_TextVarying_AllMiss`（新）** | 新增 / **参考** | 入 baseline CSV，不计门槛 |

### 7.5 BM_TextVarying 设计

```cpp
// benchmarks/bench_drawtext.cc
static constexpr vx::usize kVaryingPoolSize = 256;  // 超过 cache 容量 128 → 50% hit

// 256 条确定性生成的 19-char text（hash-seeded 避免随机波动）
struct VaryingPool {
  char buffer[kVaryingPoolSize][32];
  VaryingPool() {
    for (vx::usize i = 0; i < kVaryingPoolSize; ++i) {
      std::snprintf(buffer[i], 32, "The lazy dog var %03zu", i);
      // 20 chars exactly — matches Medium class
    }
  }
};
const VaryingPool& GetPool() { static VaryingPool p; return p; }

static void BM_DrawTextReal_Warm_TextVarying_RoundRobin(benchmark::State& state) {
  auto& fm = SharedFM();
  const auto& pool = GetPool();
  // ... Canvas setup (warm glyph cache of all 256 texts first via warmup loop)
  vx::usize idx = 0;
  for (auto _ : state) {
    canvas.DrawText(vx::StringView(pool.buffer[idx % kVaryingPoolSize]), ...);
    idx = (idx + 1) % kVaryingPoolSize;
  }
}

static void BM_DrawTextReal_Warm_TextVarying_AllMiss(benchmark::State& state) {
  // 每 iter 构造全新 text（nounce 拼接），100% miss
  auto& fm = SharedFM();
  vx::usize idx = 0;
  char buf[32];
  for (auto _ : state) {
    std::snprintf(buf, 32, "unique miss %015zu", idx++);
    canvas.DrawText(vx::StringView(buf), ...);
  }
}
```

**pre-baseline 采集协议：** 在 /build Phase 1（新 BM 骨架落地，但 ShapeCache 未接入）先跑一次收集 2 新 BM 的 **pre-baseline**，然后 Phase 4（cache 接入完成）再跑一次对比。WSL2 稳态协议必须启用（TASK-24-03 §7）。

---

## 8. 分阶段实施骨架（给 /plan 写入实现计划用）

| Phase | 内容 | Gate |
|:-:|---|---|
| **P1** | `HashBytesU64` header + FNV 单元测试（T8, T9）| Test PASS |
| **P2** | `ShapeCache` header/cc + 单元测试（T1-T7）| Test PASS |
| **P3** | `FontManager::ShapeOrLookup` API + SoftwareCanvas 接入（I1, I2, I3, R1, R2）| Test PASS + Release -O3 0 warn |
| **P4** | 新 2 BM（RoundRobin + AllMiss）骨架 + **pre-baseline 采集** | baseline CSV 生成 |
| **P5** | 完整 bench 验证（WSL2 稳态协议）+ 门槛判决 | 门槛达成或降级决策 |
| **P6** | baseline/bench_drawtext.json + README 历史行刷新 | 归档就绪 |

---

## 9. 不变量 / 契约

1. **Pixel-identical**：cache enable / disable 下 DrawText 输出完全相同（字节级）
2. **稳定指针**：`ShapeCache::entries_` 预分配后大小恒定，返回指针生命周期直到 `Clear()` / FontManager 析构
3. **无 heap 增长热路径**：hit 路径 0 分配；miss 路径仅 `Vector<ShapedGlyph>::reserve + assignment`（reserve size 已知）
4. **D 纯收尾模式**：若 Phase 5 实测 Medium ≥ 3200 ns 但 < 3499 ns（有进步但未达阈值）→ **可归档为"部分达成"**（D 模式下接受），不追加 Phase 7 继续优化

---

## 10. 接受条件（Acceptance Criteria）

1. `BM_DrawTextReal_Warm_Medium_mean` < 3200 ns（WSL2 稳态协议下）
2. `BM_DrawTextReal_Warm_Short/_Long` / `_Cold_Medium` 均 ≤ baseline × 1.1
3. `BM_DrawTextReal_Warm_TextVarying_RoundRobin` ≤ pre-baseline + max(5%, 50 ns)
4. ctest 全量 PASS（当前 59+ tests → 预计 +11-12 new：9 unit + 3 integration + 2 RED probe）
5. Release `-O3 -Werror` 0 err/warn
6. baseline/bench_drawtext.json 刷新 + README 追加 TASK-20260424-04 行
7. 反向探针 R1 + R2 均 PASS（守护 cache 正确性）

---

## 11. 参考

- TASK-20260424-03 归档：`memory-bank/archive/archive-TASK-20260424-03.md` §9.2
- TASK-20260424-03 设计：`docs/specs/2026-04-24-drawtext-warm-opt-design.md`（同任务链 K7 resolved 基线）
- systemPatterns.md：WSL2 bench 稳态协议、异构工作负载 SIMD 阈值 dispatch、编译器已做优化识别反模式
- writing-plans.mdc §7（WSL2）/ §8（编译器反模式）—— 本任务适用 §7
