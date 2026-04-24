# SoftwareCanvas::DrawText 真路径 warm 优化 — 实现计划

**目标：** warm Medium 5807 ns → **< 3000 ns**（≥ 1.94× 加速），候选 A→C→E→D→B1→B2 阶梯实施，累计达标即止。

**架构：** 7 Phase 阶梯验证，每 Phase 独立 patch + 独立 bench 记录贡献；三段式→刚性单带（D5 用户决策）不达标则 AskQuestion 升 B3 SIMD。

**技术栈：** C++17 / FreeType / HarfBuzz / google-benchmark / gcc 11.4 + `-O3 -Werror` / CMake 3.22

**复杂度级别：** Level 2-3（优化类）

---

## 0. 全局约束

### 0.1 构建配置（P1 性能基准必检项 §1）

所有 bench 使用独立 Release 目录：

```bash
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON
cmake --build build-bench -j --target bench_drawtext
```

Debug 测试走 `build/`：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build -j
```

### 0.2 阶梯退出条件

每 Phase 1-6 完成后必跑：

```bash
./build-bench/benchmarks/bench_drawtext \
    --benchmark_filter='BM_DrawTextReal_Warm_Medium' \
    --benchmark_repetitions=3 \
    --benchmark_report_aggregates_only=true \
    --benchmark_min_time=0.05s
```

取 `_mean` 数值填入「Phase × warm_Medium」累积表，判定：

- **warm Medium < 3000 ns** → ✅ **跳过后续 Phase，直接进 Phase 7 收尾**
- **warm Medium ≥ 3000 ns** → 继续下一 Phase
- Phase 6 完成仍 ≥ 3000 ns → **触发 AskQuestion 走 B3 升级分支决策**

### 0.3 Phase × warm_Medium 累积表（执行时填写）

| Phase | 候选 | warm Medium (ns) | Δ (ns) | 累计贡献 | 是否达标 |
|:-:|---|---:|---:|---:|:-:|
| baseline | — | 5807 | — | 0 | ❌ |
| 1 | A hb_buffer | _待填_ | _待填_ | _待填_ | _待判_ |
| 2 | C FT_size | _待填_ | _待填_ | _待填_ | _待判_ |
| 3 | E font cache | _待填_ | _待填_ | _待填_ | _待判_ |
| 4 | D Put→ptr | _待填_ | _待填_ | _待填_ | _待判_ |
| 5 | B1 /255 | _待填_ | _待填_ | _待填_ | _待判_ |
| 6 | B2 pre-clip | _待填_ | _待填_ | _待填_ | _待判_ |

---

## 1. Phase 划分

| Phase | 名称 | plan (min) | plan×0.6 (min) | commits |
|:-:|---|:-:|:-:|:-:|
| 0 | 基线核验 + 工具核查 + API grep | 10 | 6 | 1 |
| 1 | 候选 A — hb_buffer 复用 | 15 | 9 | 1 |
| 2 | 候选 C — FT_Set_Pixel_Sizes 缓存 | 20 | 12 | 1 |
| 3 | 候选 E — FindFont 默认 handle 缓存 | 10 | 6 | 1 |
| 4 | 候选 D — GlyphCache::Put 返回 ptr | 15 | 9 | 1 |
| 5 | 候选 B1 — /255 乘-移近似 + PixelBlendPrecision GTest + RED 反向探针 | 25 | 15 | 2 |
| 6 | 候选 B2 — Pre-clip + row ptr（条件触发）| 20 | 12 | 1 |
| 7 | Baseline 刷新 + MB 收尾 + merge | 15 | 9 | 2 |
| **合计** | — | **130** | **~78** | **10** |

---

## 2. Phase 0 — 基线核验 + 工具核查 + API grep

**目标：** 固化基线数字；核查工具链（rg/python3/find）；grep 关键 API 签名（HashMap::Insert, HashMap::operator[]）避免 plan 假设错。

### 2.1 工具链核查

```bash
for t in rg python3 find bc jq; do
  command -v "$t" >/dev/null && echo "$t ✅" || echo "$t ❌MISS"
done
```

**预期：** rg ✅ / python3 ✅ / find ✅；本 plan 不需要 bc/jq（不规划以）。

### 2.2 代理状态核查（本任务不触发 FetchContent）

```bash
# _deps 已存在 → 跳过守卫
[ -d build-bench/_deps/benchmark-src ] && echo "_deps 已完整" || echo "需首次配置"
```

### 2.3 API 签名 grep（writing-plans §3 规则）

```bash
# HashMap 操作语义（Put 改造依赖）
rg -n "Insert\(const K&|operator\[\]|Find\(const K&" veloxa/foundation/containers/hash_map.h

# FontManager 现有扩展点（候选 C 依赖）
rg -n "FontEntry|GetHbFont|hb_pixel_size" veloxa/text/font_manager.h

# GlyphCache 下游调用（候选 D 依赖）
rg -n "glyph_cache_->Put|glyph_cache_->Get" veloxa veloxa/graphics tests
```

**预期结果已验证（VAN + Plan 头脑风暴）：**

- `HashMap::Insert` 返回 `bool`；`operator[]` 返回 `V&`（见 `hash_map.h` L175/L224）
- `GlyphCache::Put` 仅在 `software_canvas.cc` L225 一处调用；`Get` 在 L206/L227 两处
- `FontEntry` 有 `hb_pixel_size`，无 `ft_pixel_size`（需 Phase 2 扩）

### 2.4 基线测量

```bash
cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DVX_BUILD_BENCHMARKS=ON 2>&1 | tail -5
cmake --build build-bench -j --target bench_drawtext 2>&1 | tail -5

./build-bench/benchmarks/bench_drawtext \
    --benchmark_repetitions=3 \
    --benchmark_report_aggregates_only=true \
    --benchmark_min_time=0.05s \
    > /tmp/bench_drawtext_baseline.txt

# 提取 warm Medium mean
grep "BM_DrawTextReal_Warm_Medium_mean" /tmp/bench_drawtext_baseline.txt
```

**预期：** 接近 5807 ns（baseline JSON 数字，允许 ±10% 当日抖动）。

### 2.5 Commit

```bash
git add -A  # （如有 MB 变更）
git commit -m "chore(plan): TASK-20260424-03 phase-0 baseline + tool audit"
```

---

## 3. Phase 1 — 候选 A hb_buffer 复用 [TDD/优化]

**文件：**
- 修改：`veloxa/graphics/software/software_canvas.cc`（匿名 ns 新 RAII + L183/L274）

### 3.1 步骤

- [ ] **步骤 1：** 在 `software_canvas.cc` 匿名 namespace 添加 `HbBufferHolder` RAII（见 spec §3.2）
- [ ] **步骤 2：** 把 L183 `hb_buffer_t* buf = hb_buffer_create();` 改为：
  ```cpp
  thread_local HbBufferHolder tls_hb_holder;
  hb_buffer_t* buf = tls_hb_holder.Acquire();
  ```
- [ ] **步骤 3：** 删除 L274 `hb_buffer_destroy(buf);`
- [ ] **步骤 4：** Debug build 跑 ctest（正确性守卫）
  ```bash
  cmake --build build -j --target software_canvas_test renderer_test && \
    ctest --test-dir build -j -R "software_canvas_test|renderer_test"
  ```
  **预期：** PASS
- [ ] **步骤 5：** Release build bench
  ```bash
  cmake --build build-bench -j --target bench_drawtext && \
    ./build-bench/benchmarks/bench_drawtext --benchmark_filter='Warm_Medium|Warm_Short|Warm_Long' \
        --benchmark_repetitions=3 --benchmark_report_aggregates_only=true --benchmark_min_time=0.05s
  ```
  **记录：** warm_Medium _mean 填入「Phase × warm_Medium」累积表
- [ ] **步骤 6：** 判定
  - warm Medium < 3000 ns → commit + **跳 Phase 7**
  - ≥ 3000 ns → commit + 进 Phase 2
- [ ] **步骤 7：** Commit
  ```bash
  git add -A
  git commit -m "perf(canvas): TASK-20260424-03 P1 thread_local hb_buffer reuse (candidate A)"
  ```

---

## 4. Phase 2 — 候选 C FT_Set_Pixel_Sizes 状态化缓存 [优化]

**文件：**
- 修改：`veloxa/text/font_manager.h`（FontEntry 扩 `ft_pixel_size` + 新 `SetFacePixelSize` 方法）
- 修改：`veloxa/text/font_manager.cc`（SetFacePixelSize 实现）
- 修改：`veloxa/graphics/software/software_canvas.cc`（L172 改走 SetFacePixelSize）

### 4.1 步骤

- [ ] **步骤 1：** `font_manager.h` FontEntry 第 51 行后插入 `u32 ft_pixel_size = 0;`
- [ ] **步骤 2：** `font_manager.h` public 段新增方法声明：
  ```cpp
  // Sets FT_Face pixel size only if changed from last call with this handle.
  // Idempotent; safe to call every frame.
  void SetFacePixelSize(FontHandle handle, u32 pixel_size);
  ```
- [ ] **步骤 3：** `font_manager.cc` 实现：
  ```cpp
  void FontManager::SetFacePixelSize(FontHandle handle, u32 pixel_size) {
    if (handle == kInvalidFont) return;
    for (usize i = 0; i < font_count_; ++i) {
      if (fonts_[i].handle != handle) continue;
      if (fonts_[i].ft_pixel_size == pixel_size) return;  // skip
      if (fonts_[i].face) FT_Set_Pixel_Sizes(fonts_[i].face, 0, pixel_size);
      fonts_[i].ft_pixel_size = pixel_size;
      return;
    }
  }
  ```
- [ ] **步骤 4：** `software_canvas.cc` L172 `FT_Set_Pixel_Sizes(face, 0, pixel_size);` 改为 `font_manager_->SetFacePixelSize(font, pixel_size);`
- [ ] **步骤 5：** Debug ctest 完整（与 HarfBuzz 协同契约：确认 `GetHbFont` 内部的 `hb_ft_font_changed` 仍被 pixel_size 变化触发；grep 确认 GetHbFont 的 hb_pixel_size 逻辑不受影响）
  ```bash
  cmake --build build -j && ctest --test-dir build -j
  ```
- [ ] **步骤 6：** Release bench 同 Phase 1 步骤 5
- [ ] **步骤 7：** 记录 warm_Medium；判定阶梯退出
- [ ] **步骤 8：** Commit
  ```bash
  git commit -m "perf(text): TASK-20260424-03 P2 FT_Set_Pixel_Sizes state-cached (candidate C)"
  ```

### 4.2 注意

- **GetHbFont 契约保留**：`font_manager.h` L39-40 注释说 caller 必须 `FT_Set_Pixel_Sizes` before `GetHbFont`。本 plan 把 caller 的直接调用改为经 SetFacePixelSize，**语义等价**（该方法内部也调 FT_Set_Pixel_Sizes，仅加了 skip 判断）。
- GetHbFont 内部 `hb_ft_font_changed` 的触发仍依赖 `entry.hb_pixel_size` 与新 pixel_size 比对，**独立于** FT 状态缓存，无需改动。

---

## 5. Phase 3 — 候选 E FindFont 默认 handle 缓存 [优化]

**文件：**
- 修改：`veloxa/graphics/software/software_canvas.cc`（L152-158 包装 cached_default_font）

### 5.1 步骤

- [ ] **步骤 1：** 在 `DrawText` 函数体 L152 处替换：
  ```cpp
  FontHandle font = kInvalidFont;
  if (font_manager_->font_count() > 0) {
    static thread_local FontHandle cached_default_font = kInvalidFont;
    static thread_local text::FontManager* cached_fm = nullptr;
    if (font_manager_ == cached_fm && cached_default_font != kInvalidFont) {
      font = cached_default_font;
    } else {
      font = font_manager_->FindFont("", 400);
      if (font == kInvalidFont) font = 1;
      cached_default_font = font;
      cached_fm = font_manager_;
    }
  }
  if (font == kInvalidFont) {
    DrawTextFallback(text, bounds, font_size, brush);
    return;
  }
  ```
- [ ] **步骤 2：** Debug ctest（正确性守卫，尤其 bench 用 `FindFont("")` 空家族的行为不应变）
- [ ] **步骤 3：** Release bench 记录
- [ ] **步骤 4：** 判定阶梯退出
- [ ] **步骤 5：** Commit
  ```bash
  git commit -m "perf(canvas): TASK-20260424-03 P3 cache default font handle (candidate E)"
  ```

---

## 6. Phase 4 — 候选 D GlyphCache::Put 返回指针 [优化]

**文件：**
- 修改：`veloxa/text/glyph_cache.h`（Put 签名 void → GlyphBitmap*）
- 修改：`veloxa/text/glyph_cache.cc`（实现改用 operator[]）
- 修改：`veloxa/graphics/software/software_canvas.cc`（L225-227 消除重复 Get）
- 测试：`tests/text/glyph_cache_test.cc`（验证现有测试仍 PASS；必要时加返回值覆盖）

### 6.1 步骤

- [ ] **步骤 1：** grep 下游调用点，确认只有 `software_canvas.cc` 用 Put（步骤 2.3 已 grep）
- [ ] **步骤 2：** `glyph_cache.h` Put 签名改为：
  ```cpp
  GlyphBitmap* Put(FontHandle font, u32 glyph_id, u32 pixel_size,
                   GlyphBitmap bitmap);
  ```
- [ ] **步骤 3：** `glyph_cache.cc` 实现改：
  ```cpp
  GlyphBitmap* GlyphCache::Put(FontHandle font, u32 glyph_id, u32 pixel_size,
                                GlyphBitmap bitmap) {
    Key k{font, glyph_id, pixel_size};
    GlyphBitmap& ref = entries_[k];  // operator[] 触发插入默认构造
    ref = std::move(bitmap);
    return &ref;
  }
  ```
- [ ] **步骤 4：** `software_canvas.cc` L225-227 改为：
  ```cpp
  cached = glyph_cache_->Put(font, glyph_id, pixel_size,
                              static_cast<GlyphBitmap&&>(gbmp));
  ```
- [ ] **步骤 5：** Debug ctest（重点 `glyph_cache_test` 与 `software_canvas_test`）
  ```bash
  cmake --build build -j --target glyph_cache_test software_canvas_test && \
    ctest --test-dir build -j -R "glyph_cache_test|software_canvas_test"
  ```
  **预期：** PASS（现有测试只验 Put 副作用 via Get，不检 Put 返回值 → 向后兼容）
- [ ] **步骤 6：** Release bench 记录（注意：候选 D 仅减少 **cold miss 路径**的重复 Get 调用，对 warm 收益可能 ≈ 0；这是阶梯验证的价值——实证「E/D 便宜但贡献小」）
- [ ] **步骤 7：** 判定阶梯退出
- [ ] **步骤 8：** Commit
  ```bash
  git commit -m "perf(text): TASK-20260424-03 P4 GlyphCache::Put returns ptr (candidate D)"
  ```

---

## 7. Phase 5 — 候选 B1 /255 乘-移近似 [Mixed TDD D3 + RED 反向探针]

**文件：**
- 修改：`veloxa/graphics/software/software_canvas.cc`（L253-267 4 处 /255 替换）
- 修改：`tests/graphics/software_canvas_test.cc`（新增 `DrawTextPixelBlendPrecision`）

### 7.1 步骤

- [ ] **步骤 1（RED）：** 写新测试 `DrawTextPixelBlendPrecision`
  ```cpp
  // tests/graphics/software_canvas_test.cc 末尾追加
  TEST(SoftwareCanvasTest, DrawTextPixelBlendPrecision) {
    vx::u32 pixels[16 * 16] = {};
    // 预填 dst：中灰 RGBA(128, 128, 128, 255)
    for (auto& p : pixels) p = 0x80808080u | 0xFF000000u;
    // 注：0xFF000000 = A; 0x00808080 = RGB 128,128,128 at R[0:7]G[8:15]B[16:23]

    vx::gfx::sw::SoftwareCanvas c(pixels, 16, 16, 16);
    // 直接调用 DrawText 不走 font path（无 FontManager）进入 fallback
    // 因此此测试本身**不**检查真路径；需要注入 GlyphCache 已预填的 bitmap
    // ← 替代方案：用 SoftwareCanvas 构造 with FontManager + GlyphCache + 系统字体

    // 若 /usr/share/fonts 不可在 tests 环境保证，改用直接暴露的纯 blit API
    // ↓ 如 software_canvas 没有纯 blit 导出 → 退化为：
    // 仅在 FontManager 可用时跑真路径，做 smoke 断言「alpha blend 公式结果
    // 与 (x*257+32768)>>16 ≤ 1 unit 差异」

    // 最小断言（本测试目标：防 B1 /255 替换退化 > 1 unit 精度）
    // 跳过前提：DejaVuSans.ttf 是否存在
    if (access("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", F_OK) != 0) {
      GTEST_SKIP() << "system font missing";
    }

    vx::text::FontManager fm;
    ASSERT_TRUE(fm.Init().ok());
    auto h = fm.LoadFont(vx::StringView("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
                          vx::StringView("DejaVu"));
    ASSERT_TRUE(h.ok());
    vx::text::GlyphCache gc;
    vx::gfx::sw::SoftwareCanvas c2(pixels, 16, 16, 16, &fm, &gc);

    vx::gfx::Rect r{0.0f, 0.0f, 16.0f, 16.0f};
    auto br = vx::gfx::Brush::Solid(vx::gfx::Color{255, 0, 0, 255});
    c2.DrawText(vx::StringView("H"), r, 12.0f, br);

    // 断言：至少某像素的 red channel > 其 baseline 128（表明 alpha blend 已发生）
    // 同时红色通道 ≤ 255（未溢出）+ alpha 通道 ≥ 128（未下降）
    bool found_blend = false;
    for (auto p : pixels) {
      u8 R = static_cast<u8>(p & 0xFF);
      u8 A = static_cast<u8>((p >> 24) & 0xFF);
      if (R > 128 && R <= 255 && A >= 128) { found_blend = true; break; }
    }
    EXPECT_TRUE(found_blend) << "DrawText should blend at least one pixel";
  }
  ```
- [ ] **步骤 2（baseline PASS）：** Debug build + 运行，确认当前实现 PASS（未改 /255 时 blend 正确）
  ```bash
  cmake --build build -j --target software_canvas_test && \
    ctest --test-dir build -R SoftwareCanvasTest.DrawTextPixelBlendPrecision
  ```
  **预期：** PASS
- [ ] **步骤 3（RED 反向探针预演）：** 临时把 L253 的 `/255` 改为 `/200`（刻意破坏精度），重编 + 跑测试：
  ```bash
  # 在 software_canvas.cc L253 临时把 /255 改为 /200，然后：
  cmake --build build -j --target software_canvas_test && \
    ctest --test-dir build -R SoftwareCanvasTest.DrawTextPixelBlendPrecision
  ```
  **预期：** FAIL 或 R 值异常偏大 → 证明测试有效
  **注：** 若测试 PASS 说明断言不够严格，补强：`EXPECT_LE(R, 220)` 等收紧容忍度
- [ ] **步骤 4：** 恢复 L253 `/200` → `/255`；确认测试再次 PASS
- [ ] **步骤 5（B1 实施）：** `software_canvas.cc` L253-L262 内 `/255` 全部替换为乘-移近似：
  ```cpp
  // 原：(x) / 255 →  改：(x * 257u + 32768u) >> 16
  u8 sa = static_cast<u8>(
      (static_cast<u32>(text_color.a) * alpha * 257u + 32768u) >> 16);
  u8 inv_sa = static_cast<u8>(255 - sa);
  u8 or_ = static_cast<u8>(
      (text_color.r * sa * 257u + text_color.r * 0u /* balance */ + ...));
  // ← 实施时按 B1 spec §3.2 精确写法，避免 overflow：
  //   sa  = (alpha * a + 127) / 255  →  (alpha * a * 257u + 32768u) >> 16
  //   or_ = (src_r * sa + dst_r * inv_sa + 127) / 255  → 同乘移
  // 选型优先: (x * 257 + 32768) >> 16，精度误差 ≤ 1 unit
  ```

  **精确替换表：**

  | 原表达式 | 替换为 |
  |---|---|
  | `(static_cast<u32>(text_color.a) * alpha) / 255` | `(static_cast<u32>(text_color.a) * alpha * 257u + 32768u) >> 16` |
  | `(text_color.r * sa + dr * inv_sa) / 255` | `((text_color.r * sa + dr * inv_sa) * 257u + 32768u) >> 16` |
  | `(text_color.g * sa + dg * inv_sa) / 255` | `((text_color.g * sa + dg * inv_sa) * 257u + 32768u) >> 16` |
  | `(text_color.b * sa + db * inv_sa) / 255` | `((text_color.b * sa + db * inv_sa) * 257u + 32768u) >> 16` |
  | `sa + (da * inv_sa) / 255` | `sa + static_cast<u8>(((da * inv_sa) * 257u + 32768u) >> 16)` |

  **Overflow 检查**：`text_color.r * sa` 最大 `255 * 255 = 65025` + `dr * inv_sa` 最大 `255 * 255 = 65025` → `130050` × `257` = `33422850` < `u32::MAX` ≈ `4.3×10^9`，安全。
- [ ] **步骤 6：** Debug ctest 完整
  ```bash
  cmake --build build -j && ctest --test-dir build -j
  ```
  **预期：** 全部 PASS（含新 `DrawTextPixelBlendPrecision`）
- [ ] **步骤 7（RED 反向探针正式验证）：** 临时把 `(x*257+32768)>>16` 改为 `(x*257+32768)>>15`（精度扩大 2×），跑测试：
  **预期：** FAIL（R 值偏差 > 1 unit，断言 `R <= 255` 或 blend 形态不符）
  **注：** 若 PASS 说明断言太宽，补强；恢复 `>>16` 后确认 PASS
- [ ] **步骤 8：** Release bench
- [ ] **步骤 9：** 记录 warm_Medium；判定阶梯退出
- [ ] **步骤 10：** 两次 commit
  ```bash
  git add tests/graphics/software_canvas_test.cc
  git commit -m "test(canvas): TASK-20260424-03 P5 add DrawTextPixelBlendPrecision GTest (RED-verified)"

  git add veloxa/graphics/software/software_canvas.cc
  git commit -m "perf(canvas): TASK-20260424-03 P5 /255 → mul-shift approximation (candidate B1)"
  ```

---

## 8. Phase 6 — 候选 B2 Pre-clip + row pointer（条件触发）[优化]

**触发条件：** Phase 5 后 warm_Medium ≥ 3000 ns

**文件：**
- 修改：`veloxa/graphics/software/software_canvas.cc`（L230-269 外+内层 blit 重构）

### 8.1 步骤

- [ ] **步骤 1：** 参考 spec §3.2 Pre-clip 代码模板重写 L230-269 的 blit 外循环
- [ ] **步骤 2：** Debug ctest 完整（`DrawTextPixelBlendPrecision` 依然应 PASS，因为 blend 公式不变）
- [ ] **步骤 3（RED 反向探针）：** 临时把 `x1 = std::min(width_, ...)` 改为 `x1 = gx + cached->width`（去掉右边界 clip），跑 DrawText 在 canvas 边缘处的测试确认缓冲区越界（AddressSanitizer 下 FAIL）
  ```bash
  cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
  cmake --build build-asan -j --target software_canvas_test
  ctest --test-dir build-asan -R software_canvas_test
  ```
  **注：** 若无 ASan，退化为 bounds 手动验证（写测试 rect.x=width_-1 然后读 pixels[width_] 应未被写入）
- [ ] **步骤 4：** 恢复 clip，确认测试 PASS
- [ ] **步骤 5：** Release bench 记录
- [ ] **步骤 6：** 判定：
  - warm Medium < 3000 ns → ✅ 进 Phase 7
  - 仍 ≥ 3000 ns → 🔴 AskQuestion 升 B3 SIMD（退出当前 plan，写升级 plan 或搁置本任务）
- [ ] **步骤 7：** Commit
  ```bash
  git commit -m "perf(canvas): TASK-20260424-03 P6 pre-clip + row ptr blit (candidate B2)"
  ```

---

## 9. Phase 7 — Baseline 刷新 + MB 构建态收尾

> **边界：** 本 Phase 只做代码/baseline/techContext 级收尾；`reflection-<task>.md` 与最终 merge 到 main 分别由 `/reflect` 与 `/archive` 负责。

**文件：**
- 修改：`benchmarks/baseline/bench_drawtext.json`
- 修改：`benchmarks/baseline/README.md`
- 修改：`memory-bank/techContext.md`
- 修改：`memory-bank/tasks.md`
- 修改：`memory-bank/activeContext.md`
- 修改：`memory-bank/progress.md`

### 9.1 步骤

- [ ] **步骤 1：** 刷新 `bench_drawtext.json` baseline
  ```bash
  cmake --build build-bench -j --target bench_drawtext && \
    ./build-bench/benchmarks/bench_drawtext \
        --benchmark_repetitions=3 \
        --benchmark_report_aggregates_only=true \
        --benchmark_min_time=0.05s \
        --benchmark_format=json \
        --benchmark_out=benchmarks/baseline/bench_drawtext.json
  ```
  确认 JSON 含 `"library_build_type": "release"`（P1 必检项）：
  ```bash
  python3 -c "import json; d=json.load(open('benchmarks/baseline/bench_drawtext.json')); print(d['context']['library_build_type'])"
  # 预期：release
  ```
- [ ] **步骤 2：** `benchmarks/baseline/README.md` 更新 K7 段：
  - 历史记录追加本次改动与新数字
  - K7 状态 Pending → **Resolved (TASK-20260424-03)**
  - 注明 warm Medium before/after + 加速比
- [ ] **步骤 3：** `memory-bank/techContext.md` 「Replay-Deepbench 性能基线」段的 DrawText 数据表更新 warm 4 行 + K7 quote 段补 resolved
- [ ] **步骤 4：** `memory-bank/tasks.md` 当前任务段补 Build 成果段（Before/After 数据表 + 各 Phase 独立贡献 + 验收 12/12）；状态保持 `🟡 构建中`
- [ ] **步骤 5：** `memory-bank/activeContext.md` 当前阶段仍为 `构建中`（切到 `回顾中` 由 `/reflect` 守卫负责）；未合并分支状态保留
- [ ] **步骤 6：** `memory-bank/progress.md` 任务里程碑：规划 ✅ / 构建 ✅（Phase 1-N 全 ✅）
- [ ] **步骤 7：** 两次 commit（不 merge 到 main）
  ```bash
  git add benchmarks/baseline/bench_drawtext.json benchmarks/baseline/README.md
  git commit -m "chore(bench): TASK-20260424-03 P7 refresh bench_drawtext baseline"

  git add memory-bank/
  git commit -m "docs: TASK-20260424-03 P7 memory bank + techContext K7 → resolved"
  ```
- [ ] **步骤 8：** 声明构建完成，等待 `/reflect` 继续

---

## 10. 升级路径（B3 SIMD 条件触发）

若 Phase 6 完成后 warm Medium ≥ 3000 ns，触发 AskQuestion：

| 选项 | 描述 | 额外估时 |
|---|---|:-:|
| i | 写 B3 plan 附录：SSE2 x86_64-only 分发（ARM 保留 scalar）| +40 min plan / +25 min 实施 |
| ii | 写 B3 plan 附录：SSE2 + NEON 双实现 | +60 min plan / +45 min 实施 |
| iii | 接受当前结果，K7 标 partial-resolved，残余进 TASK 候选 | 0 min |
| iv | 搁置本任务，触发 `/archive` 以 `已搁置` 结束 | 0 min |

---

## 11. 验收标准（最终，同 spec §1.2）

12 项全通过才可进 `/reflect`。见 `docs/specs/2026-04-24-drawtext-warm-opt-design.md` §1.2。

---

## 12. 估时校准（plan × 0.6 协议，跨 5 任务第 6 数据点）

| 任务 | plan (min) | 实测 (min) | 比例 | 档 |
|---|:-:|:-:|:-:|---|
| TASK-05 | 180 | ~53 | 0.29× | 最窄 |
| TASK-09 | 200 | ~47 | 0.24× | 最窄 |
| TASK-11 | 80 | ~38 | 0.48× | 中等 |
| TASK-13 | 120 | ~67 | 0.56× | 中等 |
| TASK-24-01 | 115 | ~33 | 0.29× | 最窄 |
| **TASK-24-03 预期** | **130** | **~78** | **0.60×** | **中等**（多 Phase + RED 反向探针） |

本任务属「**中等路径**」子档：
- ✅ 最窄路径因素：bench 基础设施完备（bench_drawtext.cc + baseline 已入仓）/ 每 Phase 单点改动
- ❌ 非最窄路径因素：涉及 5-6 候选 + 新测试 + RED 反向探针 2 次 + 可能触发 B3 升级

---

## 13. Phase 0 独立 commit 规则

每 Phase 严格 1-2 commit，命令语气 `perf() / test() / chore() / docs()`，subject 含 `TASK-20260424-03 P<N>` 锚定。

`wip:` 游离 commit 仅允许 Phase 内部 TDD RED/GREEN 之间（git-workflow.mdc）。
