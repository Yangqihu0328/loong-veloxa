# Graphics HAL + Platform HAL 实现计划

**目标：** 为 Veloxa 引擎构建图形抽象层和平台抽象层，实现软件渲染器 + Headless 后端，WSL2 完全可测试。

**架构：** 纯虚接口 + 具体后端实现。Graphics HAL 提供 Canvas/Path/Brush 2D 绘制抽象，Platform HAL 提供 Surface/EventLoop 平台无关的窗口/事件抽象。首版实现 SoftwareCanvas + MemorySurface + HeadlessEventLoop。

**技术栈：** C++17 / CMake / Google Test / Google C++ Style

**复杂度级别：** Level 4（多子系统 + 软件光栅化算法）

---

## 环境约束

- GCC 11.4.0, `-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-type-limits`
- `-Wpedantic` 禁止 C99 柔性数组成员
- 系统 GTest v1.11.0（find_package）
- WSL2 无 GPU / 无窗口系统 — 仅离屏渲染和 Headless 后端可运行时测试

## 编译器约束

- `std::function` 可用（C++17），用于 EventLoop::Task
- `std::unique_ptr` 可用，用于 Canvas::CreatePath() 返回值
- `std::chrono::steady_clock` 可用，用于定时器
- virtual 析构函数必须声明，所有接口类析构为 `virtual ~T() = default`

---

## 文件结构

### 将创建的文件

| 文件路径 | 职责 |
|---------|------|
| `veloxa/graphics/CMakeLists.txt` | Graphics 库构建 |
| `veloxa/graphics/types.h` | Color/Point/Rect/Matrix3x2 值类型 |
| `veloxa/graphics/brush.h` | Brush tagged union |
| `veloxa/graphics/path.h` | Path 纯虚接口 |
| `veloxa/graphics/canvas.h` | Canvas 纯虚接口 |
| `veloxa/graphics/software/rasterizer.h` | 扫描线光栅化器 |
| `veloxa/graphics/software/rasterizer.cc` | 实现 |
| `veloxa/graphics/software/software_path.h` | SoftwarePath |
| `veloxa/graphics/software/software_path.cc` | 实现 |
| `veloxa/graphics/software/software_canvas.h` | SoftwareCanvas |
| `veloxa/graphics/software/software_canvas.cc` | 实现 |
| `veloxa/platform/CMakeLists.txt` | Platform 库构建 |
| `veloxa/platform/surface.h` | Surface 纯虚接口 |
| `veloxa/platform/event_loop.h` | EventLoop 纯虚接口 |
| `veloxa/platform/headless/memory_surface.h` | MemorySurface |
| `veloxa/platform/headless/memory_surface.cc` | 实现 |
| `veloxa/platform/headless/headless_event_loop.h` | HeadlessEventLoop |
| `veloxa/platform/headless/headless_event_loop.cc` | 实现 |
| `tests/graphics/types_test.cc` | 值类型测试 |
| `tests/graphics/software_path_test.cc` | SoftwarePath 测试 |
| `tests/graphics/software_canvas_test.cc` | SoftwareCanvas 测试 |
| `tests/platform/memory_surface_test.cc` | MemorySurface 测试 |
| `tests/platform/event_loop_test.cc` | HeadlessEventLoop 测试 |
| `tests/graphics/integration_test.cc` | 渲染管线集成测试 |

### 将修改的文件

| 文件路径 | 变更 |
|---------|------|
| `CMakeLists.txt` | [共享文件] 添加 graphics/platform 子目录 |
| `tests/CMakeLists.txt` | [共享文件] 添加 graphics/platform 测试目标 |

---

## Phase 1：项目脚手架

### 任务 1.1：CMake 配置 + 目录结构

**文件：**
- 修改：`CMakeLists.txt`
- 创建：`veloxa/graphics/CMakeLists.txt`
- 创建：`veloxa/platform/CMakeLists.txt`

- [ ] **步骤 1：更新根 CMakeLists.txt**

```cmake
# 在 add_subdirectory(veloxa/foundation) 之后添加
add_subdirectory(veloxa/platform)
add_subdirectory(veloxa/graphics)
```

- [ ] **步骤 2：创建 Platform CMakeLists.txt**

```cmake
# veloxa/platform/CMakeLists.txt
add_library(vx_platform STATIC)

target_sources(vx_platform PRIVATE
  headless/memory_surface.cc
  headless/headless_event_loop.cc
)

target_include_directories(vx_platform PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(vx_platform PUBLIC vx_foundation)
target_compile_features(vx_platform PUBLIC cxx_std_17)
```

- [ ] **步骤 3：创建 Graphics CMakeLists.txt**

```cmake
# veloxa/graphics/CMakeLists.txt
add_library(vx_graphics STATIC)

target_sources(vx_graphics PRIVATE
  software/rasterizer.cc
  software/software_path.cc
  software/software_canvas.cc
)

target_include_directories(vx_graphics PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(vx_graphics PUBLIC vx_foundation vx_platform)
target_compile_features(vx_graphics PUBLIC cxx_std_17)
```

- [ ] **步骤 4：创建占位 .cc 文件，验证构建**

- [ ] **步骤 5：提交**

---

## Phase 2：Graphics 值类型

### 任务 2.1：types.h — Color/Point/Rect/Matrix3x2 [TDD]

**文件：**
- 创建：`veloxa/graphics/types.h`
- 测试：`tests/graphics/types_test.cc`

核心接口：

```cpp
#ifndef VELOXA_GRAPHICS_TYPES_H_
#define VELOXA_GRAPHICS_TYPES_H_

#include <cmath>
#include "veloxa/foundation/base/types.h"

namespace vx::gfx {

struct Color {
  u8 r, g, b, a;

  static constexpr Color FromRGBA(u8 r, u8 g, u8 b, u8 a = 255) {
    return {r, g, b, a};
  }
  static constexpr Color Transparent() { return {0, 0, 0, 0}; }
  static constexpr Color Black() { return {0, 0, 0, 255}; }
  static constexpr Color White() { return {255, 255, 255, 255}; }
  static constexpr Color Red() { return {255, 0, 0, 255}; }
  static constexpr Color Green() { return {0, 255, 0, 255}; }
  static constexpr Color Blue() { return {0, 0, 255, 255}; }

  constexpr bool operator==(Color other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  constexpr bool operator!=(Color other) const { return !(*this == other); }

  // Pack to RGBA u32 (R in lowest byte)
  constexpr u32 ToRGBA32() const {
    return static_cast<u32>(r) | (static_cast<u32>(g) << 8) |
           (static_cast<u32>(b) << 16) | (static_cast<u32>(a) << 24);
  }
  static constexpr Color FromRGBA32(u32 packed) {
    return {static_cast<u8>(packed), static_cast<u8>(packed >> 8),
            static_cast<u8>(packed >> 16), static_cast<u8>(packed >> 24)};
  }
};

struct Point {
  f32 x, y;
  constexpr Point operator+(Point other) const { return {x + other.x, y + other.y}; }
  constexpr Point operator-(Point other) const { return {x - other.x, y - other.y}; }
  constexpr Point operator*(f32 s) const { return {x * s, y * s}; }
  constexpr bool operator==(Point other) const { return x == other.x && y == other.y; }
};

struct Rect {
  f32 x, y, w, h;

  constexpr f32 right() const { return x + w; }
  constexpr f32 bottom() const { return y + h; }
  constexpr bool Contains(Point p) const {
    return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
  }
  constexpr bool IsEmpty() const { return w <= 0 || h <= 0; }

  // Intersection
  Rect Intersect(const Rect& other) const;

  constexpr bool operator==(const Rect& other) const {
    return x == other.x && y == other.y && w == other.w && h == other.h;
  }
};

struct Matrix3x2 {
  // [a b]   [tx]     m[0] m[1]
  // [c d] + [ty]     m[2] m[3]
  //                  m[4] m[5]
  f32 m[6];

  static Matrix3x2 Identity();
  static Matrix3x2 Translation(f32 tx, f32 ty);
  static Matrix3x2 Scale(f32 sx, f32 sy);
  static Matrix3x2 Rotation(f32 radians);

  Point Apply(Point p) const;
  Matrix3x2 Multiply(const Matrix3x2& other) const;
};

}  // namespace vx::gfx

#endif  // VELOXA_GRAPHICS_TYPES_H_
```

测试要点：
- Color 构造、FromRGBA32/ToRGBA32 往返、预定义颜色
- Point 算术运算
- Rect Contains/Intersect/IsEmpty
- Matrix3x2 Identity/Translation/Scale/Rotation、Apply 点变换、Multiply 组合

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 2.2：brush.h — Brush tagged union [TDD]

**文件：**
- 创建：`veloxa/graphics/brush.h`

核心接口：

```cpp
struct Brush {
  enum class Type : u8 { kSolid, kLinearGradient };

  Type type;

  struct LinearGradient {
    Point start, end;
    Color color_start, color_end;
  };

  union {
    Color solid;
    LinearGradient linear;
  };

  static Brush Solid(Color c);
  static Brush Linear(Point start, Point end, Color c0, Color c1);

  // 采样：给定归一化位置返回颜色（用于光栅化器）
  Color Sample(Point p) const;
};
```

测试包含在 types_test.cc 中。

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(graphics): value types - Color, Point, Rect, Matrix3x2, Brush"`

---

## Phase 3：Platform HAL 接口 + Headless 实现

### 任务 3.1：Surface 接口 + MemorySurface [TDD]

**文件：**
- 创建：`veloxa/platform/surface.h`
- 创建：`veloxa/platform/headless/memory_surface.h`
- 创建：`veloxa/platform/headless/memory_surface.cc`
- 测试：`tests/platform/memory_surface_test.cc`

测试要点：
- 构造指定尺寸，width/height/stride 正确
- Lock/Unlock 返回有效指针，可读写像素
- Resize 后尺寸更新，旧指针无效
- SavePPM 写入文件、文件可读取验证头部
- 零尺寸 Surface 行为

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 3.2：EventLoop 接口 + HeadlessEventLoop [TDD]

**文件：**
- 创建：`veloxa/platform/event_loop.h`
- 创建：`veloxa/platform/headless/headless_event_loop.h`
- 创建：`veloxa/platform/headless/headless_event_loop.cc`
- 测试：`tests/platform/event_loop_test.cc`

测试要点：
- PostTask 按 FIFO 顺序执行
- PollOnce 处理当前队列不阻塞
- Run + Quit 正确退出
- SetTimer 单次触发
- SetTimer repeat 多次触发
- CancelTimer 取消后不触发
- 嵌套 PostTask（任务中投递新任务）

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(platform): surface and event_loop with headless implementations"`

---

## Phase 4：Graphics HAL 接口

### 任务 4.1：Path 纯虚接口 [TDD]

**文件：**
- 创建：`veloxa/graphics/path.h`

纯接口头文件，无实现。测试在 Phase 5 与 SoftwarePath 一起。

### 任务 4.2：Canvas 纯虚接口 [TDD]

**文件：**
- 创建：`veloxa/graphics/canvas.h`

纯接口头文件，无实现。测试在 Phase 5 与 SoftwareCanvas 一起。

- [ ] **步骤 1：创建接口头文件**
- [ ] **步骤 2：验证编译**
- [ ] **步骤 3：提交** `git commit -m "feat(graphics): Canvas and Path pure virtual interfaces"`

---

## Phase 5：软件渲染器 — 基础

### 任务 5.1：SoftwarePath [TDD]

**文件：**
- 创建：`veloxa/graphics/software/software_path.h`
- 创建：`veloxa/graphics/software/software_path.cc`
- 测试：`tests/graphics/software_path_test.cc`

核心设计：
- 内部存储命令列表：`Vector<PathCommand>` where `PathCommand` 是 tagged union (MoveTo/LineTo/QuadTo/CubicTo/ArcTo/Close)
- Bounds() 遍历所有点计算包围盒
- Contains() 使用射线法（ray casting）
- IsEmpty() 检查命令列表为空

测试要点：
- MoveTo + LineTo 构建三角形，Bounds 正确
- Close 自动连接首尾
- Contains 对三角形内外点正确
- Reset 清空
- QuadTo/CubicTo 的 Bounds 包含控制点

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 5.2：Rasterizer 扫描线填充 [TDD]

**文件：**
- 创建：`veloxa/graphics/software/rasterizer.h`
- 创建：`veloxa/graphics/software/rasterizer.cc`

核心设计（方案 C：覆盖率扫描线 + 非零规则 + 内置 AA）：

详细设计见 `memory-bank/creative/creative-scanline-rasterizer.md`

```cpp
namespace vx::gfx::sw {

struct Edge {
  f32 x_top, y_top;       // 上端点
  f32 x_bottom, y_bottom; // 下端点
  i8 direction;            // +1 向下, -1 向上
};

class Rasterizer {
 public:
  Rasterizer(u32* pixels, u32 width, u32 height, u32 stride);

  void FillPath(const SoftwarePath& path, const Brush& brush,
                const Rect& clip, const Matrix3x2& transform);
  void StrokePath(const SoftwarePath& path, const Brush& brush,
                  f32 stroke_width,
                  const Rect& clip, const Matrix3x2& transform);
  void FillRect(const Rect& rect, const Brush& brush,
                const Rect& clip, const Matrix3x2& transform);

  static u32 BlendSrcOver(u32 dst, u32 src);

 private:
  void GenerateEdges(const SoftwarePath& path, const Matrix3x2& transform);
  void FlattenQuad(Point p0, Point ctrl, Point p2);
  void FlattenCubic(Point p0, Point c1, Point c2, Point p3);
  void AddLineEdge(Point from, Point to);
  void BucketSort();
  void RenderScanlines(const Brush& brush, const Rect& clip);
  void ComputeEdgeCoverage(const Edge& edge, f32 y, f32 y_next);
  void AccumulateAndBlend(i32 y, const Brush& brush, const Rect& clip);

  u32* pixels_;
  u32 width_, height_, stride_;
  Vector<Edge> edges_;
  Vector<Vector<Edge>> bucket_table_;
  Vector<Edge> active_edges_;
  Vector<f32> coverage_;  // 每行覆盖率缓冲
};

}  // namespace vx::gfx::sw
```

覆盖率扫描线算法：
1. 边生成：Path 命令展平为直线段 Edge（贝塞尔 de Casteljau 细分，阈值 0.25px）
2. 分桶：按 floor(y_top) 分桶，避免全局排序
3. 逐行光栅化：
   a. 清零 coverage buffer
   b. 新边加入活跃边列表
   c. 对每条活跃边计算解析覆盖率（带符号面积）→ 累加到 coverage[x]
   d. 从左到右累加 coverage，得到 alpha = clamp(abs(running), 0, 1)
   e. 采样 Brush 颜色，调制 alpha，SrcOver 合成
4. 非零缠绕规则：通过 direction 字段和有符号面积实现
5. 描边：路径法线偏移 ±width/2 生成外轮廓 → 转为 FillPath

测试：通过 SoftwareCanvas 间接验证（像素级 + AA 边缘半透明验证）。

### 任务 5.3：SoftwareCanvas — 基础绘制 [TDD]

**文件：**
- 创建：`veloxa/graphics/software/software_canvas.h`
- 创建：`veloxa/graphics/software/software_canvas.cc`
- 测试：`tests/graphics/software_canvas_test.cc`

核心设计：
- 构造接收 `u32* pixels, u32 width, u32 height, u32 stride`
- 维护：clip_stack_ (Vector<Rect>)、state_stack_ (Vector<State>)、layer_stack_
- State = { Matrix3x2 transform; Rect clip; }
- Begin/End 标记帧
- Clear 全 buffer 写入颜色
- FillRect 裁剪后逐像素填充 + SrcOver
- FillRoundedRect 构造圆角路径后走 FillPath
- FillPath 调用 Rasterizer::FillPath
- StrokePath 调用 Rasterizer::StrokePath
- Clip：PushClipRect 压栈 + 与当前 clip 取交集，PopClip 弹栈
- Transform：PushState/PopState 保存恢复完整状态
- Layer：分配临时 buffer，PopLayer 时 alpha 合成回主 buffer
- CreatePath 返回 SoftwarePath 实例

测试要点：
- Clear 验证全 buffer 颜色
- FillRect 验证指定区域像素为填充色，外部为背景色
- FillRect 半透明 alpha 合成验证
- FillRoundedRect 角部像素为背景色
- PushClipRect 限制绘制区域
- PushState/PopState 恢复变换
- Translation 变换后绘制偏移
- FillPath 三角形填充验证
- StrokeLine 验证线段像素
- PushLayer 透明度合成

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(graphics): software renderer - path, rasterizer, canvas"`

---

## Phase 6：集成测试

### 任务 6.1：完整渲染管线测试 [TDD]

**文件：**
- 创建：`tests/graphics/integration_test.cc`

测试场景：
1. MemorySurface(200,200) + SoftwareCanvas → Clear + FillRect → Lock 验证像素
2. 多层绘制：背景色 + 蓝色矩形 + 半透明红色矩形叠加 → 验证混合结果
3. 变换 + 裁剪：Translate(50,50) + FillRect → 验证偏移位置正确
4. Path 三角形 → FillPath → 验证内部像素
5. EventLoop + 渲染：PostTask 中执行渲染 → PollOnce → 验证输出
6. SavePPM → 读取文件验证 PPM 头部格式正确

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(graphics): integration tests for render pipeline"`

---

## Phase 7：更新 CMake + 最终验证

### 任务 7.1：tests/CMakeLists.txt 更新

**文件：**
- 修改：`tests/CMakeLists.txt`

```cmake
# 追加 Graphics 测试
vx_add_test(gfx_types_test graphics/types_test.cc)
target_link_libraries(gfx_types_test PRIVATE vx_graphics)

vx_add_test(software_path_test graphics/software_path_test.cc)
target_link_libraries(software_path_test PRIVATE vx_graphics)

vx_add_test(software_canvas_test graphics/software_canvas_test.cc)
target_link_libraries(software_canvas_test PRIVATE vx_graphics)

vx_add_test(gfx_integration_test graphics/integration_test.cc)
target_link_libraries(gfx_integration_test PRIVATE vx_graphics)

# 追加 Platform 测试
vx_add_test(memory_surface_test platform/memory_surface_test.cc)
target_link_libraries(memory_surface_test PRIVATE vx_platform)

vx_add_test(event_loop_test platform/event_loop_test.cc)
target_link_libraries(event_loop_test PRIVATE vx_platform)
```

注：vx_add_test 默认链接 vx_foundation，需额外链接 vx_graphics/vx_platform。

- [ ] **步骤 1-4：** 更新 CMake，运行全部测试
- [ ] **步骤 5：提交** `git commit -m "feat(graphics+platform): final cmake and test infrastructure"`

---

## 任务总览

| Phase | 任务 | 文件数 | 预估时间 |
|-------|------|--------|---------|
| 1 | 项目脚手架 | 4 | 20 min |
| 2 | 值类型 (Color/Point/Rect/Matrix3x2/Brush) | 3 | 1.5 h |
| 3 | Platform HAL (Surface/EventLoop + Headless) | 7 | 2.5 h |
| 4 | Graphics HAL 接口 (Canvas/Path) | 2 | 30 min |
| 5 | 软件渲染器 (SoftwarePath/Rasterizer/SoftwareCanvas) | 7 | 5 h |
| 6 | 集成测试 | 1 | 1 h |
| 7 | CMake 收尾 | 1 | 15 min |
| **总计** | **11 个任务** | **~25 个文件** | **~11 h** |

## 需要创意阶段的组件

1. ~~**扫描线光栅化器算法细节**~~ — ✅ 已完成，选定方案 C（覆盖率扫描线 + 非零规则 + 内置 AA），见 `memory-bank/creative/creative-scanline-rasterizer.md`
