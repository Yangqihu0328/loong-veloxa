# 创意设计：渲染管线（Render Pipeline）

**日期：** 2026-04-05
**状态：** 已批准
**关联任务：** TASK-20260405-07

## 设计挑战

将 LayoutBox 布局树转换为 Canvas 绘制指令，实现 HTML→DOM→CSS→Layout→Render→PPM 的端到端可视化。

**约束条件：**
- 输入：`LayoutBox*`（含绝对坐标 x/y、盒模型尺寸、ComputedStyle*）
- 输出：Canvas 绘制调用（最终写入 Surface 像素缓冲区）
- Canvas 当前无 DrawText 方法
- CSS 颜色格式（RRGGBBAA）与 gfx::Color::ToRGBA32 格式（R[0:7]A[24:31]）不同

## 决策 1：Renderer 公共 API — Display List / 命令缓冲

### 选定方案

采用 Display List 模式：先将 LayoutBox 树序列化为有序 PaintCommand 列表，再回放到 Canvas。

### 核心数据结构

```cpp
namespace vx::render {

struct PaintCommand {
  enum class Type : u8 {
    kFillRect,       // 背景色、边框填充
    kDrawText,       // 文本绘制
    kPushClipRect,   // overflow:hidden 裁剪
    kPopClip,
    kPushLayer,      // opacity < 1 透明层
    kPopLayer,
  };

  Type type;
  gfx::Rect rect;       // FillRect: 目标矩形; DrawText: 文本边界; PushClipRect: 裁剪区; PushLayer: 层边界
  gfx::Color color;     // FillRect: 填充色; DrawText: 文字色
  f32 param = 0;        // DrawText: font_size; PushLayer: opacity
  StringView text;      // DrawText: 文本内容（生命周期须 ≤ DOM 树）
};

using DisplayList = Vector<PaintCommand>;

// 1. 序列化：LayoutBox 树 → 有序 PaintCommand 列表
DisplayList Record(layout::LayoutBox* root);

// 2. 回放：PaintCommand 列表 → Canvas 绘制调用
void Replay(const DisplayList& list, gfx::Canvas* canvas);

// 3. 便捷入口：Record + Replay
void Paint(layout::LayoutBox* root, gfx::Canvas* canvas);

}  // namespace vx::render
```

### 生命周期约束

- `PaintCommand::text` 是 `StringView`，指向 DOM Text 节点的 `data()`
- **调用者必须确保 DOM 树在 Replay 完成前存活**
- 典型用法：`Record` → `Replay` 在同一帧内完成

### 优势

- 可序列化用于调试（dump display list）
- 可缓存/diff 用于增量渲染
- Record 和 Replay 可独立测试
- 未来可支持 GPU 命令缓冲

## 决策 2：文本渲染 — 扩展 Canvas::DrawText

### Canvas 接口新增

```cpp
// canvas.h 新增纯虚函数
virtual void DrawText(StringView text, const Rect& bounds,
                      f32 font_size, const Brush& brush) = 0;
```

### SoftwareCanvas 实现（V1 存根）

逐字符绘制填充矩形，模拟位图字体效果：

```cpp
void SoftwareCanvas::DrawText(StringView text, const Rect& bounds,
                               f32 font_size, const Brush& brush) {
  f32 char_width = font_size * 0.6f;
  f32 char_height = font_size;
  f32 x = bounds.x;
  f32 y = bounds.y;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == ' ') { x += char_width; continue; }
    if (x + char_width > bounds.right()) break;  // 超出边界停止
    FillRect({x, y, char_width - 1, char_height}, brush);
    x += char_width;
  }
}
```

**关键点：** 每个字符画一个小矩形（留 1px 间距），空格跳过。效果类似像素字体，在 PPM 中可清晰辨认字符数和词边界。

### 未来演进

集成 FreeType + HarfBuzz 后，SoftwareCanvas::DrawText 替换为真正的字形光栅化，上层代码（Renderer）零修改。

## 决策 3：绘制顺序 — Stacking Context 排序

### 算法

```
RecordBox(box, display_list):
  if visibility == hidden → 跳过
  if display == none → 跳过（BuildTree 已过滤，双重保险）

  // 1. Opacity 层
  if opacity < 1.0 → push PushLayer(border_box, opacity)

  // 2. Overflow 裁剪
  if overflow == hidden → push PushClipRect(content_area)

  // 3. 背景色（仅 alpha > 0 时绘制）
  if background_color.a > 0 → push FillRect(border_box, bg_color)

  // 4. 边框（每边独立检查 border_style != none && border_width > 0）
  for side in [top, right, bottom, left]:
    if border_style[side] != none && border_width[side] > 0:
      push FillRect(edge_rect, border_color[side])

  // 5. 文本（仅 kText 节点）
  if box.type == kText && box.text_node:
    push DrawText(text, content_rect, font_size, color)

  // 6. 子元素 — 按 z-index 排序
  children = collect all children
  stable_sort(children, by z_index ascending)
  for child in sorted children:
    RecordBox(child, display_list)

  // 7. 出栈
  if overflow == hidden → push PopClip
  if opacity < 1.0 → push PopLayer
```

### Stacking Context 规则（V1 简化版）

- `z-index` 值直接用于排序（`stable_sort` 保证同 z-index 元素保持 DOM 源序）
- 负 z-index 先绘制 → 正 z-index 后绘制 → DOM 源序作为 tiebreaker
- `opacity < 1` 通过 `PushLayer/PopLayer` 创建合成层

### 边框矩形计算

CSS 边框模型：border 在 padding box 外侧，每边是一个细长矩形。

```
border_box 起点 = (box.x - border[left], box.y - border[top])

Top    边框: Rect(bx, by, border_box_width, border[top])
Bottom 边框: Rect(bx, by + border_box_height - border[bottom], border_box_width, border[bottom])
Left   边框: Rect(bx, by + border[top], border[left], border_box_height - border[top] - border[bottom])
Right  边框: Rect(bx + border_box_width - border[right], by + border[top], border[right], border_box_height - border[top] - border[bottom])
```

## 跨模块数据格式：颜色转换

### 格式差异

| 来源 | 格式 | Red Opaque | Black Opaque |
|------|------|------------|-------------|
| ComputedStyle `u32` | RRGGBBAA | `0xFF0000FF` | `0x000000FF` |
| gfx::Color::ToRGBA32() | R[0:7]G[8:15]B[16:23]A[24:31] | `0xFF0000FF` | `0xFF000000` |

### 转换函数

```cpp
// render_utils.h
inline gfx::Color CssColorToGfx(u32 css_color) {
  return gfx::Color::FromRGBA(
    static_cast<u8>((css_color >> 24) & 0xFF),  // R
    static_cast<u8>((css_color >> 16) & 0xFF),  // G
    static_cast<u8>((css_color >> 8) & 0xFF),   // B
    static_cast<u8>(css_color & 0xFF));          // A
}
```

**验证：**
- `CssColorToGfx(0xFF0000FF)` → `Color{255, 0, 0, 255}` = Red ✓
- `CssColorToGfx(0x000000FF)` → `Color{0, 0, 0, 255}` = Black ✓
- `CssColorToGfx(0x00000000)` → `Color{0, 0, 0, 0}` = Transparent ✓

## 文件清单

### 新增文件

| 文件 | 内容 |
|------|------|
| `veloxa/core/render/paint_command.h` | PaintCommand 结构体 + DisplayList 类型别名 |
| `veloxa/core/render/renderer.h` | Record / Replay / Paint 函数声明 |
| `veloxa/core/render/renderer.cc` | Record（RecordBox 递归 + z-index 排序）、Replay、Paint 实现 |
| `veloxa/core/render/render_utils.h` | CssColorToGfx 等转换工具（header-only） |
| `tests/core/render/paint_command_test.cc` | PaintCommand 构造、DisplayList 操作 |
| `tests/core/render/renderer_test.cc` | Record 单元测试（各种 LayoutBox 场景） |
| `tests/core/render/replay_test.cc` | Replay 单元测试（MockCanvas 验证绘制调用序列） |
| `tests/core/render/integration_test.cc` | HTML→PPM 全管线集成测试 |

### 修改文件

| 文件 | 修改 |
|------|------|
| `veloxa/graphics/canvas.h` | 新增 `DrawText` 纯虚函数 |
| `veloxa/graphics/software/software_canvas.h` | 新增 `DrawText` override 声明 |
| `veloxa/graphics/software/software_canvas.cc` | 实现 `DrawText`（逐字符 FillRect） |
| `veloxa/core/CMakeLists.txt` | 新增 `render/renderer.cc` |
| `tests/CMakeLists.txt` | 新增测试目标 |

## 边界输入清单

| 场景 | 预期行为 |
|------|---------|
| 空 LayoutBox 树（nullptr root） | Record 返回空 DisplayList；Paint 不崩溃 |
| 所有元素 visibility:hidden | DisplayList 为空 |
| opacity=0 的元素 | PushLayer(0) 包裹，子元素仍 Record（透明但存在） |
| background_color alpha=0（透明） | 跳过 FillRect |
| border_style=none 的边 | 跳过该边的 FillRect |
| 纯文本节点（无 style） | 使用默认 color/font_size |
| z-index 负值 | 排在 z-index=0 的兄弟之前绘制 |
| 嵌套 overflow:hidden | PushClipRect 嵌套，Canvas clip stack 正确出入栈 |
| 空文本节点 | DrawText 不生成（text.empty() 检查） |

## 实现指导

1. **颜色转换是第一个陷阱** — CssColorToGfx 必须在所有颜色使用点调用，单元测试覆盖 red/green/blue/black/transparent
2. **DrawText 添加到 Canvas 是接口变更** — 修改 canvas.h 后需确认 SoftwareCanvas 编译通过
3. **Record 的递归深度** — 与 DOM 树深度一致，通常 < 50 层，栈溢出风险极低
4. **stable_sort 保证 DOM 源序** — z-index 相同的元素保持原始 DOM 顺序
5. **边框矩形的 left/right 边需排除 top/bottom 的高度** — 避免角落重叠双重绘制
