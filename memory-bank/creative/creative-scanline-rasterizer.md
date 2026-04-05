# 创意设计：扫描线光栅化器

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-02

## 设计挑战

SoftwareCanvas 需要将 Path（由 MoveTo/LineTo/QuadTo/CubicTo/ArcTo/Close 构成的向量路径）光栅化到 RGBA8888 像素 buffer 中。

**约束条件：**
- 嵌入式目标，内存和 CPU 受限
- 使用 Foundation 层的自定义容器（vx::Vector）
- C++17，GCC 11.4，-Wpedantic -Werror
- 首版需支持 CSS box model 圆角、简单图标路径
- 不使用第三方库

**成功标准：**
- 正确性：非零缠绕规则，像素级正确，内置抗锯齿
- 性能：200x200 Surface 上 10 个中等路径 < 10ms
- 可维护性：算法结构清晰
- 内存：coverage buffer + edge 存储可控

## 探索的方案

### 方案 A：整数扫描线 + 全局排序边表 + 奇偶规则

最经典的扫描线填充。全局 Edge 列表按 y_min 排序，逐行维护活跃边列表，奇偶配对填充。

- 优点：实现最简单（~200 行），内存最低，性能最优
- 缺点：奇偶规则与 CSS/SVG 默认不一致，无抗锯齿，AA 扩展需重构
- 复杂度：低
- 不选原因：填充规则不兼容 CSS/SVG 标准

### 方案 B：整数扫描线 + 分桶边表 + 非零规则

分桶边表避免全局排序，非零缠绕规则与 CSS/SVG 一致。Edge 增加方向字段。

- 优点：非零规则正确，分桶性能好，预留 AA 扩展点
- 缺点：首版无 AA，边缘锯齿明显
- 复杂度：中
- 不选原因：缺少抗锯齿

### 方案 C：覆盖率扫描线 + 非零规则 + 内置 AA（选定）

基于覆盖率的解析光栅化。每个像素计算边穿越的精确覆盖率（0.0-1.0），实现亚像素精度抗锯齿。

- 优点：内置 AA，非零规则兼容，浮点精度
- 缺点：实现复杂度最高，浮点运算开销，调试难度高
- 复杂度：高

## 选定方案：方案 C — 覆盖率扫描线

### 理由

1. 作为 UI 渲染引擎，抗锯齿是用户可见的核心质量指标
2. 非零规则兼容 CSS/SVG 标准
3. 虽然实现复杂，但算法成熟（FreeType/AGG 已验证数十年）
4. 一次到位避免后续重构

### 核心数据结构

```cpp
namespace vx::gfx::sw {

// 光栅化用的边（预处理后的直线段）
struct Edge {
  f32 x_top, y_top;       // 上端点（y 较小的端）
  f32 x_bottom, y_bottom; // 下端点（y 较大的端）
  i8 direction;            // +1 向下, -1 向上
};

class Rasterizer {
 public:
  // 初始化，绑定像素 buffer
  Rasterizer(u32* pixels, u32 width, u32 height, u32 stride);

  // 将 SoftwarePath 光栅化
  void FillPath(const SoftwarePath& path, const Brush& brush,
                const Rect& clip, const Matrix3x2& transform);

  void StrokePath(const SoftwarePath& path, const Brush& brush,
                  f32 stroke_width,
                  const Rect& clip, const Matrix3x2& transform);

  // 矩形优化路径（不走扫描线）
  void FillRect(const Rect& rect, const Brush& brush,
                const Rect& clip, const Matrix3x2& transform);

 private:
  // 阶段 1：边生成
  void GenerateEdges(const SoftwarePath& path, const Matrix3x2& transform);
  void FlattenQuad(Point p0, Point ctrl, Point p2);
  void FlattenCubic(Point p0, Point c1, Point c2, Point p3);
  void AddLineEdge(Point from, Point to);

  // 阶段 2：分桶
  void BucketSort();

  // 阶段 3：逐行光栅化
  void RenderScanlines(const Brush& brush, const Rect& clip);
  void ComputeEdgeCoverage(const Edge& edge, f32 y, f32 y_next);
  void AccumulateAndBlend(i32 y, const Brush& brush, const Rect& clip);

  // Alpha 合成
  static u32 BlendSrcOver(u32 dst, u32 src);

  u32* pixels_;
  u32 width_, height_, stride_;

  Vector<Edge> edges_;                    // 所有边
  Vector<Vector<Edge>> bucket_table_;     // 按 y 分桶
  Vector<Edge> active_edges_;             // 当前活跃边
  Vector<f32> coverage_;                  // 每行覆盖率缓冲
};

}  // namespace vx::gfx::sw
```

### 算法流程

#### 阶段 1：边生成（预处理）

输入 SoftwarePath，输出 Vector<Edge>：

1. 遍历 Path 命令，将所有曲线细分为直线段
2. 贝塞尔细分：递归 de Casteljau，弦偏差阈值 0.25px
3. 每条直线段生成一个 Edge：
   - 跳过水平边（y_top == y_bottom）
   - 确保 y_top < y_bottom（否则交换端点并翻转 direction）
4. 应用 Matrix3x2 变换到所有端点

贝塞尔细分递归深度限制 16 层，防止栈溢出。

#### 阶段 2：分桶

按 `floor(y_top)` 将每条 Edge 插入对应桶。桶表大小 = `ceil(y_max) - floor(y_min)`。

#### 阶段 3：逐行覆盖率光栅化

对每个像素行 y（从 y_min_global 到 y_max_global）：

1. 清零 coverage buffer
2. 从 bucket_table[y] 取新边加入 active_edges
3. 对每条活跃边计算覆盖率：
   - 将边裁剪到行范围 [y, y+1)
   - 插值得到裁剪后的 x 范围
   - 对每个受影响的像素列计算带符号面积贡献
   - 累加到 coverage[x]
4. 移除 y_bottom <= y+1 的过期边
5. 累加并混合：
   - running_cover = 0
   - for x: running_cover += coverage[x]
   - alpha = clamp(abs(running_cover), 0.0, 1.0)
   - 采样 Brush 颜色，调制 alpha
   - SrcOver 合成到 pixels[y * stride + x]

#### 覆盖率计算（核心子算法）

对一条边在像素行 [y, y+1) 内的段：

```
裁剪到行: y_start = max(y_top, y), y_end = min(y_bottom, y+1)
插值 x:  x_start = lerp(x_top, x_bottom, (y_start - y_top) / (y_bottom - y_top))
         x_end   = lerp(x_top, x_bottom, (y_end - y_top) / (y_bottom - y_top))

height = y_end - y_start  // 在行内的 y 跨度 (0~1)

// 对穿越的每个像素列 ix:
// 计算边在像素格子 [ix, ix+1] × [y, y+1] 内的带符号面积
// area = direction * (梯形面积)
// coverage[ix] += area
```

面积计算：将边段裁剪到像素列 [ix, ix+1]，计算裁剪后梯形的面积。
- 如果边完全在列内：area = height * (x_center - ix) * direction
- 如果边跨越列边界：分段计算

### 描边（Stroke）实现

将 stroke 转化为 fill：
1. 沿路径法线方向偏移 ±stroke_width/2 生成外轮廓
2. 将外轮廓作为闭合路径调用 FillPath
3. 首版简化：仅支持 butt 端帽、miter 连接

### Alpha 合成

SrcOver: `result = src + dst * (1 - src_alpha)`

```cpp
inline u32 BlendSrcOver(u32 dst, u32 src) {
  u32 sa = (src >> 24) & 0xFF;
  if (sa == 0) return dst;
  if (sa == 255) return src;

  u32 inv_sa = 255 - sa;
  u32 r = ((src & 0xFF) * 256 + (dst & 0xFF) * inv_sa) >> 8;
  u32 g = (((src >> 8) & 0xFF) * 256 + ((dst >> 8) & 0xFF) * inv_sa) >> 8;
  u32 b = (((src >> 16) & 0xFF) * 256 + ((dst >> 16) & 0xFF) * inv_sa) >> 8;
  u32 a = (sa * 256 + ((dst >> 24) & 0xFF) * inv_sa) >> 8;
  return r | (g << 8) | (b << 16) | (a << 24);
}
```

### 性能考量

- coverage buffer: width * 4 bytes (f32)，200px = 800 bytes
- edges: 每个 20 bytes，中等路径约 100 条边 = 2KB
- active_edges: 通常 < 20 条
- 矩形 FillRect 走优化路径，不经过扫描线
- 热路径（覆盖率计算 + 混合）可后续用 SIMD 优化

### 测试策略

- 矩形填充：4 角和中心像素验证
- 三角形：内部不透明、边缘半透明（AA 验证）
- 自相交路径：非零规则不镂空
- 圆角矩形：角部 AA 渐变
- 空路径/退化路径：不崩溃
