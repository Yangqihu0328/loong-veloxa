# GLES 图像采样过滤选项 NEAREST/LINEAR 设计规格

**任务：** TASK-20260602-01（G1.9 技术债 #2 清理）
**复杂度：** Level 2
**日期：** 2026-06-02
**安全相关：** ❌ 否（纯 GL 采样状态 / 无外部输入 / 无 GLSL 拼接）

---

## 1. 背景与目标

G1.9（TASK-20260529-04）`ImageTexturePool::GetOrUpload` 在纹理上传时硬编码 `GL_TEXTURE_MIN/MAG_FILTER = GL_LINEAR`，调用方无法选择 NEAREST（像素艺术 / 精确像素映射）。本任务为 `GLESCanvas` 补齐采样过滤选项，**保持 LINEAR 默认**（向后兼容 G1.9）。

---

## 2. 设计决策（D1-D5 / 头脑风暴锁定）

| # | 决策 | 选定 | 理由 |
|:-:|---|---|---|
| **D1** | 过滤旋钮归属 | **GLES 局部 setter**（`GLESCanvas::SetImageSamplingFilter`，**不**上抽象 `Canvas`）| 复杂度缩减；`ImageTexturePool` 零改动；software/renderer/paint_command/C-API 零改动；enum 入 `types.h` 为未来提升留门 |
| **D2** | 缓存键 / filter 应用 | **不**纳入缓存键；`DrawImage` 每次绑定纹理后 `glTexParameteri` 权威设置 | 纹理=内容、filter=采样器状态；同图不同 filter 多次 draw 正确，无缓存膨胀；不引入 GL sampler object（YAGNI）|
| **D3** | 跨后端语义 | **仅 GLES**；software 维持 NEAREST-only | 同步 software 双线性 = 独立大任务，out of scope；差异文档化记 techContext |
| **D4** | 默认值 | `SamplingFilter::kLinear` | 保留 G1.9 行为，向后兼容 |
| **D5** | PushState/PopState | filter **不**纳入 State 存档 | MVP；与 transform 不同；文档化为已知限制 |

---

## 3. 接口设计

### 3.1 共享枚举（`veloxa/graphics/types.h`）

```cpp
// 图像采样过滤模式。kLinear = 双线性平滑（缩放默认）；kNearest = 最近邻
// （像素艺术 / 精确像素映射）。当前仅 GLESCanvas::DrawImage 消费；software
// 后端固定 NEAREST（整数截断），不读取本枚举（D3 跨后端差异，记技术债）。
enum class SamplingFilter { kLinear, kNearest };
```

### 3.2 GLESCanvas 公有 API（`gles_canvas.h`）

```cpp
// 设置后续 DrawImage 的纹理采样过滤模式（GLES 专属，非抽象 Canvas API）。
// 默认 kLinear（向后兼容 G1.9）。不受 PushState/PopState 影响（D5）。
void SetImageSamplingFilter(SamplingFilter filter) { image_filter_ = filter; }
SamplingFilter image_sampling_filter() const { return image_filter_; }
```

私有成员：

```cpp
SamplingFilter image_filter_ = SamplingFilter::kLinear;
```

### 3.3 DrawImage filter 应用（`gles_canvas.cc`）

`DrawImage` 在 `glBindTexture(GL_TEXTURE_2D, tex)`（P1#A 副作用契约重绑）**之后**、`glDrawArrays` **之前**，按当前 `image_filter_` 权威设置 MIN/MAG：

```cpp
glBindTexture(GL_TEXTURE_2D, tex);  // bind AFTER GetOrUpload (P1#A)
const GLint gl_filter =
    (image_filter_ == SamplingFilter::kNearest) ? GL_NEAREST : GL_LINEAR;
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter);
glDrawArrays(GL_TRIANGLES, 0, 6);
```

> `ImageTexturePool::GetOrUpload` 上传时仍设 LINEAR 作为初值，但 `DrawImage` 每次 draw 权威覆盖 → 同一缓存纹理在不同 filter 间切换正确。`ImageTexturePool` 本身零改动。

---

## 4. 测试策略（TDD / 承接 P1#1 解析采样 + P1#2 双通道）

**核心区分手法：** 上传 **2×1 图（texel0=红 / texel1=蓝）**，src 全图 `{0,0,2,1}` 放大到 64×64 dst：
- GL MAG_FILTER 主导（2px→64px 放大）。
- **LINEAR**：texel 中心 u=0.25(红)/0.75(蓝)，中间混合 → 接缝区出现紫色（R、B 均中等）。
- **NEAREST**：texel 边界 u=0.5（x≈32）硬切 → 仅纯红/纯蓝，**无紫色**。

像素分类（doc 行扫描，glReadPixels 底左→顶左换算复用现有 `GlY`）：
- 红：`R>200 && B<60`
- 蓝：`B>200 && R<60`
- 紫（混合证据）：`R∈[60,200] && B∈[60,200]`

| ID | 名称 | 断言 |
|:-:|---|---|
| F1 | `SetGetSamplingFilter` | `SetImageSamplingFilter(kNearest)` → getter==kNearest；默认 getter==kLinear |
| F2 | `DrawImage_DefaultLinear_BlendsAtSeam` | 不调 setter，接缝扫描行紫色像素 `> 0`（默认 LINEAR 保留）|
| F3 | `DrawImage_Nearest_HardSeam` | setter(kNearest)，接缝扫描行紫色像素 `== 0` 且红、蓝各 `> 0` |
| F4 | `DrawImage_Linear_SoftSeam` | 显式 setter(kLinear)，接缝紫色像素 `> 0` |
| F5 | `DrawImage_FilterSwitch_NoGLError` | kNearest↔kLinear 切换多次 draw `glGetError()==GL_NO_ERROR` |

扫描带：dst `{0,0,64,64}`，接缝在 x≈32；扫描 doc-Y=32 行的 x∈[0,64)（取一行足够区分；F3 验证整行无紫，F2/F4 验证有紫）。

---

## 5. 影响面与风险

| 维度 | 结论 |
|---|---|
| 文件变更 | 4（`types.h` / `gles_canvas.{h,cc}` / 测试）；`ImageTexturePool` **零改动** |
| 抽象 Canvas API | 不变（D1=① 局部）|
| 向后兼容 | ✅ 默认 LINEAR，现有 G1.9 像素测不受影响 |
| 三矩阵 | gles 1432→~1437（+5）/ software 1303 / no-devtool 1141（0 退化）|
| 0 新依赖 | ✅ |

| 风险 | 级别 | 缓解 |
|---|:-:|---|
| Mesa swrast NEAREST/LINEAR MAG 行为不符预期 | 🟡 | F2/F3 早测；2×1 放大是经典可判别用例 |
| 紫色阈值 `[60,200]` 误判（边缘抗锯齿污染）| 🟢 | 接缝整行扫描取「存在性」而非单点；NEAREST 整行严格无紫 |
| filter 不入 PushState 引用户困惑 | 🟢 | D5 文档化 |

---

## 6. 反复模式预防

| # | 模式 | 抑制 |
|:-:|---|---|
| #1 前置依赖未验证 | Canvas/types/ImageTexturePool/software 采样均已实证读取 ✅ |
| #3 TDD 倒置 | 单轮 RED 先于 GREEN ✅ |
| #4 反向探针弱 | F2（默认 LINEAR 保留）+ F3（NEAREST 无紫）互为反向 ✅ |
| #7 双 config 盲区 | §5 三矩阵 ✅ |
| #8 ctest baseline | gles 1432 实测基线 ✅ |
| P1#1 解析采样 | 接缝 x≈32 解析推导 + 整行扫描 ✅ |
| P1#2 双通道 | 红/蓝/紫均双通道判别 ✅ |

---

**Source:** G1.9 archive 技术债 #2 + `image_texture_pool.cc`（LINEAR 硬编码）+ `software_canvas.cc:282-320`（NEAREST 整数截断实证）+ `types.h`（enum 放置）+ G1.9 P1#A（draw 前重绑后设 filter）
