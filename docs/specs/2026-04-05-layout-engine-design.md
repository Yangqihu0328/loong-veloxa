# Layout Engine 设计规格 — TASK-20260405-06

## 架构决策

### 1. LayoutBox：扁平 struct + LayoutType 标签
- 零虚函数开销，Arena 友好，所有类型共享同一内存布局
- Flex 专用字段在非 Flex 盒子中浪费 ~8 字节（可接受）

### 2. Layout Tree：独立树 + 指针回指 DOM
- LayoutBox 有自己的 parent/child/sibling 双向链表
- display:none 不生成 LayoutBox
- 匿名块自然支持
- Document 的 ArenaAllocator 分配

### 3. 文本测量：TextShaper 纯虚接口 + SimpleTextShaper 存根
- Layout 引擎仅依赖 TextShaper*，不依赖 FreeType
- SimpleTextShaper：width = char_count × font_size × 0.6，height = font_size × 1.2

### 4. 布局范围：Block + Flex + Inline(简化) + Positioning(relative/absolute)
- 延期：Grid、Table、Float、Fixed

### 5. 入口 API：LayoutEngine::Layout(Document*, LayoutContext) → LayoutBox*

## 核心数据结构

```cpp
enum class LayoutType : u8 { kBlock, kInline, kFlex, kText };

struct LayoutBox {
  LayoutType type;
  dom::Element* element;          // null for text boxes
  dom::Text* text_node;           // non-null for kText
  const css::ComputedStyle* style;

  LayoutBox* parent;
  LayoutBox* first_child;
  LayoutBox* last_child;
  LayoutBox* next_sibling;
  LayoutBox* prev_sibling;

  f32 x, y;                      // relative to containing block
  f32 content_width, content_height;
  f32 padding[4];                 // top, right, bottom, left
  f32 border[4];
  f32 margin[4];
};

struct LayoutContext {
  TextShaper* text_shaper;
  f32 viewport_width;
  f32 viewport_height;
  f32 root_font_size;             // for rem
};
```

## 布局算法概览

### Block Layout (CSS 2.1 BFC)
1. 宽度：auto → containing block width；否则解析 width
2. 递归布局子元素，垂直堆叠（累加 y 偏移）
3. 高度：auto → 子元素总高度；否则解析 height
4. Auto margin 水平居中
5. min/max 约束

### Flex Layout (CSS Flexbox Level 1)
1. 确定主轴/交叉轴（flex-direction）
2. 计算 flex base size（flex-basis / width / content）
3. 收集 flex items 到 flex lines（flex-wrap）
4. 解析弹性长度（grow/shrink 分配剩余/不足空间）
5. 主轴对齐（justify-content + gap）
6. 交叉轴对齐（align-items / align-self）

### Inline Layout (简化 IFC)
1. 遍历子节点，通过 TextShaper 测量文本宽度
2. 按 containing width 换行
3. 行高和 text-align 对齐

### Positioning
1. relative：在 flow 位置基础上偏移 top/left/bottom/right
2. absolute：相对最近 positioned 祖先定位
