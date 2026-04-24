# SoftwareCanvas::DrawText 真路径 warm 优化 — 设计规格

**日期：** 2026-04-24
**任务：** TASK-20260424-03
**来源：** TASK-20260419-09 K7 发现（`memory-bank/archive/archive-TASK-20260419-09.md`），候选 TASK-20260419-12 P2 触发型已激活
**复杂度：** Level 2-3（优化类；多候选 + 设计决策；修改 3-5 文件）
**安全相关：** ❌ 否

---

## 1. 目的与成功标准

### 1.1 目的

把 `SoftwareCanvas::DrawText` 真路径（FreeType + HarfBuzz + GlyphCache）的 **warm Medium** 耗时从 **5807 ns → < 3000 ns**（≥ 1.94× 加速），使其在**所有文本长度**下优于 fallback（fallback Medium = 3647 ns），为后续**默认真路径化**（新任务的前置条件）扫清性能债。

### 1.2 成功标准（Binary 验收）

| # | 判据 | 阈值（刚性）|
|:-:|---|---:|
| S1 | `BM_DrawTextReal_Warm_Medium`（19 char）| **< 3000 ns** |
| S2 | `BM_DrawTextReal_Warm_Short`（2 char）| ≤ 1166 ns（968×1.2 + 5 兜底，TASK-11 #1 绝对增量）|
| S3 | `BM_DrawTextReal_Warm_Long`（124 char）| ≤ 18537 ns（16852×1.1）|
| S4 | `BM_DrawTextReal_Cold_Medium` | ≤ 60677 ns（52763×1.15，冷路径允许小退化）|
| S5 | `BM_DrawTextFallback_{Short,Medium}` | ≤ baseline × 1.15 |
| S6 | `BM_ReplayTextHeavyReal`（913 µs）| ≤ 913 µs × 1.0（改善或持平）|
| S7 | `bench_layout_*` / `bench_render_*` / `bench_imagecache` | 无显著退化（10% 内）|
| S8 | ctest 全量 | PASS（predicted 892+）|
| S9 | Release `-O3 -Werror` 全量 rebuild | 0 err/warn |
| S10 | 新增 `DrawTextPixelBlendPrecision` GTest | PASS + RED 反向探针验证有效 |
| S11 | `benchmarks/baseline/bench_drawtext.json` 刷新 + `baseline/README.md` K7 标 resolved | ✅ |
| S12 | `techContext.md` Replay-Deepbench 段 K7 补 resolved + 新 warm 数据表 | ✅ |

---

## 2. 用户决策（头脑风暴产出）

| # | 维度 | 选择 | 理由 |
|:-:|---|---|---|
| **D1** | 优化实施策略 | **方案 1 阶梯验证驱动**（7 Phase）| 复用 TASK-24-01 0.29× 样板；便宜先做 A→C→E→D→B1→B2；累计达标即止；独立贡献可量化 |
| **D2** | `hb_buffer` 复用策略 | **thread_local + RAII** | 零线程风险 + 零 header 污染 + thread exit 自动清理 |
| **D3** | Inner blit loop 深度 | **B1 + B2 组合** | /255 乘-移近似 + pre-clip；SIMD 留触发型 P2（ARM/NEON 兼容性开销） |
| **D4** | `GlyphBitmap` 结构 | **保持 Vector<u8>** | 0 ABI 改动；`data()` + row ptr 推进即可；Vector::operator[] 内联等价裸指针 |
| **D5** | 验收阈值 | **刚性 < 3000 ns**（用户选严格）| 不设中间带；B1+B2 不达标则 AskQuestion 升 B3 SIMD（需决策分发策略）|

---

## 3. 架构与组件

### 3.1 整体结构

```
SoftwareCanvas::DrawText（真路径，改造后）
├── 【Phase 1 候选 A】thread_local HbBufferHolder
│   └── 匿名 namespace 内 RAII；hb_buffer_clear_contents() 复用
├── 【Phase 2 候选 C】FT_Set_Pixel_Sizes 状态缓存
│   └── FontManager::FontEntry 扩 ft_pixel_size 字段
├── 【Phase 3 候选 E】默认 FontHandle 记忆
│   └── software_canvas.cc 文件级 thread_local cached_default_font
├── 【Phase 4 候选 D】GlyphCache::Put 返回指针
│   └── glyph_cache.h Put 签名微改，消 L225 Put + L227 再 Get 双查
├── 【Phase 5 候选 B1】/255 乘-移近似
│   └── inner blit loop 4 处 (x/255) → ((x*257 + 32768) >> 16)
├── 【Phase 6 候选 B2】Pre-clip + row pointer
│   └── outer 算 glyph_rect ∩ canvas_rect；去 inner 4 bounds check
└── 【Phase 7】Baseline 刷新 + Memory Bank 收尾
```

### 3.2 组件细节

#### HbBufferHolder（新，候选 A）

```cpp
// 位置：software_canvas.cc 匿名 namespace 顶部
namespace {
class HbBufferHolder {
 public:
  hb_buffer_t* Acquire() {
    if (!buf_) buf_ = hb_buffer_create();
    else hb_buffer_clear_contents(buf_);
    return buf_;
  }
  ~HbBufferHolder() {
    if (buf_) hb_buffer_destroy(buf_);
  }
 private:
  hb_buffer_t* buf_ = nullptr;
};
}  // namespace
```

调用点（L183-188 替换）：

```cpp
thread_local HbBufferHolder tls_hb_holder;
hb_buffer_t* buf = tls_hb_holder.Acquire();
hb_buffer_add_utf8(buf, text.data(), ...);
// ... 原逻辑 ...
// L274 hb_buffer_destroy(buf) 删除
```

#### FT 尺寸缓存（候选 C）

```cpp
// font_manager.h FontEntry 扩字段
struct FontEntry {
  // ... 原有字段 ...
  u32 hb_pixel_size = 0;  // 已存在
  u32 ft_pixel_size = 0;  // 新增
};

// font_manager.cc 新增方法
void FontManager::SetFacePixelSize(FontHandle handle, u32 pixel_size) {
  // 仅 pixel_size 变化时调 FT_Set_Pixel_Sizes；否则空操作
}
```

`software_canvas.cc` L172 改为 `font_manager_->SetFacePixelSize(font, pixel_size);`。

#### 默认 FontHandle 记忆（候选 E）

```cpp
// software_canvas.cc 函数体内
thread_local FontHandle cached_default_font = kInvalidFont;
thread_local text::FontManager* cached_fm = nullptr;

FontHandle font = kInvalidFont;
if (font_manager_ == cached_fm && cached_default_font != kInvalidFont) {
  font = cached_default_font;
} else if (font_manager_->font_count() > 0) {
  font = font_manager_->FindFont("", 400);
  if (font == kInvalidFont) font = 1;
  cached_default_font = font;
  cached_fm = font_manager_;
}
```

#### GlyphCache::Put 返回指针（候选 D）

```cpp
// glyph_cache.h
class GlyphCache {
 public:
  // ...
  GlyphBitmap* Put(FontHandle font, u32 glyph_id, u32 pixel_size,
                   GlyphBitmap bitmap);  // 返回值由 void → GlyphBitmap*
  // ...
};

// glyph_cache.cc
GlyphBitmap* GlyphCache::Put(...) {
  Key k{font, glyph_id, pixel_size};
  auto [it, inserted] = entries_.Insert(k, std::move(bitmap));
  return &it->second;  // 具体 HashMap::Insert API 按 foundation/containers 实际签名
}
```

`software_canvas.cc` L225-227：

```cpp
// 旧：
glyph_cache_->Put(font, glyph_id, pixel_size, std::move(gbmp));
cached = glyph_cache_->Get(font, glyph_id, pixel_size);
// 新：
cached = glyph_cache_->Put(font, glyph_id, pixel_size, std::move(gbmp));
```

#### /255 替换（候选 B1）

```cpp
// 数学：x / 255 ≈ (x * 257 + 32768) >> 16  （精度损失 ≤ 1/65535）
// L253-255 原：
u8 sa = static_cast<u8>((static_cast<u32>(text_color.a) * alpha) / 255);
// 改：
u8 sa = static_cast<u8>(
    (static_cast<u32>(text_color.a) * alpha * 257u + 32768u) >> 16);

// L257/259/261 类似替换（or_/og_/ob_ 的 /255）
// L262 oa = sa + (da * inv_sa) / 255 同样替换
```

精度证明：`(x*257+32768)>>16` 对 x∈[0, 65025] 的最大偏差 ≤ 1/255（即 alpha blend 后单通道 ≤ 1 unit 差异；视觉无感知）。

#### Pre-clip + row pointer（候选 B2）

```cpp
// 原 L230-269：外循环 glyph，内层 row × col，每像素 4 bounds check
// 改造：
if (cached && cached->width > 0 && cached->height > 0) {
  i32 gx = static_cast<i32>(pen_x + x_offset) + cached->bearing_x;
  i32 gy = static_cast<i32>(pen_y - y_offset) - cached->bearing_y;

  // 外层一次算 clip rect
  i32 x0 = std::max(0, gx);
  i32 y0 = std::max(0, gy);
  i32 x1 = std::min(static_cast<i32>(width_), gx + static_cast<i32>(cached->width));
  i32 y1 = std::min(static_cast<i32>(height_), gy + static_cast<i32>(cached->height));
  if (x0 >= x1 || y0 >= y1) { pen_x += x_advance; continue; }

  const u8* alpha_base = cached->alpha.data();
  u32 stride_pixels = stride_ / 4;

  for (i32 py = y0; py < y1; ++py) {
    const u8* row = alpha_base + (py - gy) * cached->width;
    u32* dst_row = pixels_ + static_cast<u32>(py) * stride_pixels;
    for (i32 px = x0; px < x1; ++px) {
      u8 alpha = row[px - gx];
      if (alpha == 0) continue;  // early-exit 保留
      // ... /255 乘-移近似 blend（B1 已改）...
      dst_row[px] = /* 组合结果 */;
    }
  }
}
```

### 3.3 数据流（不变）

`DrawText(text, bounds, size, brush)` → `FindFont / GetFace / GetHbFont` → `hb_shape(buf)` → glyph 循环 `GlyphCache::Get/Put` → Inner blit 到 `pixels_`。本次**不改**任何阶段次序，仅优化各阶段内部开销。

### 3.4 错误处理

| 情形 | 原有行为 | 新行为 |
|---|---|---|
| `font_manager_ == nullptr` | fallback | 不变 |
| `hb_buffer_create` 失败 | 未处理（hb 规范返回 empty buffer 而非 null）| 不变，Holder Acquire 兜底处理 |
| `/255` 近似精度超限 | N/A | 数学证明 ≤ 1 unit，无需运行时处理 |
| Pre-clip 空 rect | per-pixel continue 逐个跳过 | 早 continue，功能等价 |

---

## 4. 注入点核对表（writing-plans §「管线注入点代码级可行性验证」）

| 候选 | 注入点 | 数据可访问 | 可写性 | 结论 |
|---|---|:-:|:-:|---|
| A hb_buffer 复用 | `software_canvas.cc` L183/L274 | ✅ 局部 buf | ✅ 函数体 | 可行（匿名 ns 新 RAII）|
| C FT_Set_Pixel_Sizes 缓存 | `software_canvas.cc` L172 + `font_manager.{h,cc}` | ✅ FontEntry 可扩字段 | ✅ | 可行 |
| E FindFont 缓存 | `software_canvas.cc` L154 | ✅ | ✅ 文件 thread_local | 可行 |
| D GlyphCache::Put 返回 ptr | `glyph_cache.{h,cc}` + `software_canvas.cc` L225-227 | ✅ HashMap Insert 返回 it | ✅ 签名变 void→ptr | 可行；确认 `HashMap::Insert` 已有返回 it 语义（plan Phase 0 grep 核实） |
| B1 /255 替换 | `software_canvas.cc` L253-267 | ✅ 纯数学 | ✅ | 可行 |
| B2 Pre-clip + row ptr | `software_canvas.cc` L230-269 | ✅ glyph_rect 可前算 | ✅ | 可行 |

**注入点全部可行，无需扩接口/跨模块透传。**

---

## 5. 测试策略

### 5.1 性能回归

- 每 Phase 末（1-6）：跑 `bench_drawtext` 全 8 BM + `bench_render_record / replay` smoke
- 与 `benchmarks/baseline/bench_drawtext.json` 对比，记录**独立贡献**到 Phase commit 消息与 plan 内嵌表

### 5.2 正确性回归

**新增测试**：`tests/graphics/software_canvas_test.cc`（已存在，追加）

```cpp
TEST(SoftwareCanvasTest, DrawTextPixelBlendPrecision) {
  // 构造已知 GlyphBitmap alpha + text color，检验输出 RGBA ≤ 1 unit 差异
  // 属 Mixed TDD D3 类：防回归（/255 → 乘移近似的精度守卫）
}
```

- Phase 5 (B1) 首次引入后编写；**RED 反向探针验证**：临时把 `>>16` 改 `>>15` 观察 FAIL，恢复确认 PASS（TASK-11 #3 / TASK-24-01 落实 P1）
- Phase 6 (B2) 后再跑一次确认 pre-clip 不破坏像素正确性

### 5.3 端到端

- 每 Phase 末：`ctest --test-dir build -j`（Debug，892+ tests）
- 收尾 Phase：Release 全量 `cmake --build build-bench -j` 0 err/warn

### 5.4 工具链依赖

| 工具 | 用途 | plan Phase 0 验证 |
|---|---|:-:|
| `python3` | JSON diff + bench 对比 | ✅ 必验（bc/jq 不需要）|
| `rg` | grep | ✅ 必验 |
| `find` | bench/test binary 路径 | ✅ 必验（TASK-24-01 #2 新规则）|

---

## 6. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|---|:-:|:-:|---|
| **R1**：B1+B2 合计 warm < 3000 ns 未达 | 中 | 高（刚性阈值）| AskQuestion 承接：(i) B3 SIMD（x86_64-only 守卫 vs SSE2+NEON 双版本）；(ii) 搁置本任务重规划 |
| R2：`/255` 近似导致像素断言测试 FAIL | 低 | 中 | 数学证明 ≤ 1 unit；测试 tolerance 允许 ±1；RED 反向探针验证 |
| R3：GlyphCache::Put 签名改破坏下游 | 低 | 中 | tests/text/glyph_cache_test.cc 仅用 Get + size，Put 返回值可忽略（向后兼容 via `[[nodiscard]]` 不加）|
| R4：thread_local HbBufferHolder 在 main 线程之外析构顺序与 HarfBuzz lib 释放冲突 | 低 | 低 | HarfBuzz 规范保证 `hb_buffer_destroy` 独立于 hb_font/hb_face（无隐式关联）|
| R5：FontManager::SetFacePixelSize 新增方法破坏已有 GetHbFont 内部 FT_Set_Pixel_Sizes 调用 | 低 | 中 | 保留 GetHbFont 内部原有 FT_Set_Pixel_Sizes（若存在）；新方法独立路径 |
| R6：Phase 串行累积导致 bisect 困难 | 低 | 低 | 每 Phase 独立 commit + 独立 bench 记录；回退只需 `git revert <phase-commit>` |

---

## 7. 验收决策逻辑（三段式失效 → 刚性单带）

```
Phase 6 (B2) 完成后跑 bench_drawtext：
├── warm Medium < 3000 ns → ✅ 进入 Phase 7 收尾
├── warm Medium ≥ 3000 ns → 🔴 触发 AskQuestion：
│   ├── (i) 升 B3 SIMD 分支（估加 40-60 min plan，需决策 x86_64-only vs SSE2+NEON）
│   ├── (ii) 接受当前结果为「中间态」，K7 标为 partial-resolved，留残余入新候选
│   └── (iii) 搁置本任务（/archive 为 `已搁置`），重规划
```

---

## 8. 参考引用

- `memory-bank/archive/archive-TASK-20260419-09.md` — K7 来源
- `memory-bank/techContext.md` L249-266 — Replay-Deepbench 基线 + K7
- `benchmarks/baseline/bench_drawtext.json` — 实测 baseline
- `veloxa/graphics/software/software_canvas.cc` L143-276 — DrawText 真路径
- `veloxa/text/{glyph_cache,font_manager}.{h,cc}` — 受影响次要文件
- `.cursor/rules/skills/writing-plans.mdc` — 计划规则
- TASK-24-01 `archive-TASK-20260424-01.md` — 阶梯验证样板（0.29× 历史最快数据点）

---

## 9. 变更日志

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-04-24 | v1.0 | 初始发布；5 决策锁定；头脑风暴批准 |
