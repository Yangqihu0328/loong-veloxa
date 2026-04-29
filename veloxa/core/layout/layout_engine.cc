#include "veloxa/core/layout/layout_engine.h"

#include <algorithm>
#include <new>

#include "veloxa/core/css/style_resolver.h"
#include "veloxa/core/image/image_cache.h"
#include "veloxa/core/layout/flex_layout.h"
#include "veloxa/core/layout/line_box.h"
#include "veloxa/core/layout/margin_collapse.h"
#include "veloxa/core/layout/text_shaper.h"

namespace vx::layout {

namespace {

// =============================================================================
// Margin collapsing helpers — CSS 2.1 §8.3.1 (TASK-20260426-01 R3)
//
// 设计依据：memory-bank/creative/creative-margin-collapsing.md 方案 A
// （MarginChain in-line 累积 + LayoutBlock 内部栈式状态）。
// =============================================================================

// 是否参与 normal flow 的 margin collapse（排除 absolute / fixed）。
inline bool IsInFlow(const LayoutBox* child) {
  if (!child->style) return true;
  return child->style->position != css::Position::kAbsolute;
}

// 是否创建独立的 BFC，从而**阻断**自身 margin 与外部 sibling chain 的合并。
// D1.3 范围：仅识别 overflow ∈ {hidden, scroll, auto}。完整 BFC trigger
// （float / display: flow-root / inline-block / 绝对定位）留 P3。
//
// 性能：默认 ComputedStyle.overflow == kVisible → 单次比较早退（hot path）。
inline bool CreatesBlockFormattingContext(const LayoutBox* box) {
  if (!box->style) return false;
  return box->style->overflow != css::Overflow::kVisible;
}

// W3C §8.3.1 collapse-through 判定：
//   1) 无 top/bottom border / padding
//   2) 无 inline content / 无 in-flow 子内容（即 content_height == 0）
//   3) height 是 auto / none
//   4) 不是 BFC root
//
// 性能优化（R3 第二轮，TASK-20260426-01 选项 B）：
//   1) 最高频早退（bench corpus 大多 box 有内容） → content_height != 0
//   2) padding/border 4 个 vertical 比较合并为 1 个 sum 非零检查；数学等价：
//      所有量非负 ⇒ sum == 0 ⇔ 全 0。让编译器更易向量化 + 省 3 个分支。
//   3) BFC root 检查 inline 化（去掉 CreatesBlockFormattingContext 函数调用，
//      避免重复 style 解引用）。
//   4) 字段访问按 cache locality 倒排：先 box 结构内连续字段，最后才解引用 style。
inline bool IsCollapseThrough(const LayoutBox* child) {
  if (child->content_height != 0.0f) return false;
  if (child->type != LayoutType::kBlock) return false;
  // 合并 4 个 vertical 内边距/边框非零检查（数学等价：非负 sum=0 ⇔ 全 0）。
  // 编译器在 -O3 下倾向把 4 个连续 f32 加法 schedule 成 SIMD ADDPS。
  const f32 v_extents = child->padding[LayoutBox::kTop] +
                        child->padding[LayoutBox::kBottom] +
                        child->border[LayoutBox::kTop] +
                        child->border[LayoutBox::kBottom];
  if (v_extents != 0.0f) return false;
  if (!child->style) return false;
  // BFC root 阻断 inline 化（与 CreatesBlockFormattingContext 同语义）。
  if (child->style->overflow != css::Overflow::kVisible) return false;
  // height: <length> 非 auto/none 时不 collapse-through（值为 0 也占位）。
  // W3C §8.3.1 严格要求 height: auto，保留不可省。
  const auto& h = child->style->height;
  return h.is_auto() || h.is_none();
}

// =============================================================================
// Line box helpers — CSS 2.1 §10.8 / §10.8.1 (TASK-20260426-01 R4 #21)
//
// 设计依据：memory-bank/creative/creative-line-box-model.md
//   D2.A 显式 LineBox + Vector<LineBox>
//   D2.B 严格 2-pass vertical-align 算法
//   D2.C TextMetrics 加 ascent/descent（baseline 兼容字段保留）
//   D2.D inline-block 当 atomic（border_box_height 当 ascent + 0 当 descent）
//   D2.E line-height 数字 / length / percent 单位语义
// =============================================================================

// 解析 line-height 为绝对像素值（CSS 2.1 §10.8.1）。
//   number（无单位）→ font_size 倍数
//   length / percent → 通过 ResolveLength（percent 基于 font_size，不是 containing）
//   auto / none      → 1.2 × font_size（CSS 'normal' 默认值）
inline f32 ResolveLineHeightPx(const css::ComputedStyle* style, f32 font_size,
                                const LayoutContext& ctx) {
  if (!style) return font_size * 1.2f;
  const auto& lh = style->line_height;
  if (lh.is_auto() || lh.is_none()) return font_size * 1.2f;
  if (lh.unit == css::Unit::kNumber) return lh.value * font_size;
  // length / percent：percent 基于 font_size（CSS 2.1 §10.8.1 特殊规则）
  return ResolveLength(lh, font_size, font_size, ctx);
}

// 计算 inline 单元贡献给 line-box 的 ascent / descent（CSS 2.1 §10.8.1）。
//   text 节点 → TextMetrics.ascent / descent
//   inline-block / replaced → border_box_height 当 ascent + 0 当 descent（atomic）
//   nested inline → 不参与 line-box metrics（该 inline 自身 layout 后取其 children
//     的总 ascent / descent；当前简化为 height 当 ascent，0 当 descent；与
//     LayoutInline 主循环递归保持一致）
inline void ComputeInlineMetrics(const LayoutBox* child, f32 child_ascent_text,
                                  f32 child_descent_text, f32& ascent,
                                  f32& descent) {
  if (child->type == LayoutType::kText) {
    ascent = child_ascent_text;
    descent = child_descent_text;
    return;
  }
  // inline-block / replaced / nested inline → atomic
  ascent = child->border_box_height();
  descent = 0.0f;
}

// vertical-align 6 关键字（不含 kTop/kBottom）+ length 的 baseline_offset 计算。
//
// 坐标约定：item.y = baseline_y - item.ascent + offset
//   → offset > 0 ⇒ item 向下沉（y 增大）
//   → offset < 0 ⇒ item 向上升（y 减小）
//
// 与 creative-line-box-model.md §伪码完全对齐（见 D2.B）：
//   sub        = +0.2 × parent_font_size      （下沉）
//   super      = −0.4 × parent_font_size      （上升）
//   middle     = −(x_height/2 + item_h/2)     （中线对齐 baseline + x_height/2）
//   text-top   = −(parent_ascent − item_ascent)
//   text-bottom= +(parent_descent − item_descent)
//   length     = −resolved_length             （CSS 2.1：正值上移 → 负 offset）
inline f32 ComputeNonExtremeAlign(const LayoutBox* item_box,
                                   f32 item_ascent, f32 item_descent,
                                   f32 parent_font_size, f32 parent_ascent,
                                   f32 parent_descent,
                                   const LayoutContext& ctx) {
  using css::VerticalAlign;
  if (!item_box->style) return 0.0f;
  const auto va = item_box->style->vertical_align;
  switch (va) {
    case VerticalAlign::kBaseline:
      return 0.0f;
    case VerticalAlign::kSub:
      return +parent_font_size * 0.2f;  // 下沉
    case VerticalAlign::kSuper:
      return -parent_font_size * 0.4f;  // 上升
    case VerticalAlign::kMiddle: {
      // CSS 2.1 §10.8.1：item 中线对齐 baseline + x-height/2。
      // x-height 简化为 0.5 × font_size（无 face metric 时通行做法）。
      const f32 x_height = parent_font_size * 0.5f;
      return -(x_height * 0.5f + (item_ascent - item_descent) * 0.5f);
    }
    case VerticalAlign::kTextTop:
      // item 顶部对齐 parent ascent 顶 → offset 把 item 上拉。
      return -(parent_ascent - item_ascent);
    case VerticalAlign::kTextBottom:
      return +(parent_descent - item_descent);
    case VerticalAlign::kLength: {
      // CSS 2.1：length / percent 正值上移 → offset 取负号。
      // percent 基于 font_size（line-height 默认参考）。
      const auto& off = item_box->style->vertical_align_offset;
      return -ResolveLength(off, parent_font_size, parent_font_size, ctx);
    }
    case VerticalAlign::kTop:
    case VerticalAlign::kBottom:
      // Phase 2 处理；本函数不应被调用到。
      return 0.0f;
  }
  return 0.0f;
}

// 严格 2-pass vertical-align 算法（CSS 2.1 §10.8.1 / creative D2.B）：
//   Phase 1 — 收集非 {kTop, kBottom} item，更新 line.max_ascent / max_descent
//   Phase 2 — 解析 kTop / kBottom item.baseline_offset；再次更新 max_*
//   Phase 3 — line.baseline_y = line.top + max_ascent + half_leading_top
//             逐 item 写最终 child->y = baseline_y - item.ascent + offset
inline void FinalizeLine(LineBox& line, f32 parent_font_size,
                          f32 parent_ascent, f32 parent_descent,
                          f32 line_height_px, const LayoutContext& ctx) {
  using css::VerticalAlign;

  // 坐标约定（统一全 Phase）：item.y = baseline_y - ascent + offset。
  //   offset > 0 ⇒ 下沉（baseline 之上延伸 = ascent − offset 变小）
  //   offset < 0 ⇒ 上升（baseline 之上延伸 = ascent − offset 变大）
  // 因此 max_ascent / max_descent 公式：
  //   max_ascent  = max(prev, item.ascent  − offset)
  //   max_descent = max(prev, item.descent + offset)

  // Phase 1
  for (auto& item : line.items) {
    const auto va = item.box->style ? item.box->style->vertical_align
                                     : VerticalAlign::kBaseline;
    if (va == VerticalAlign::kTop || va == VerticalAlign::kBottom) continue;
    item.baseline_offset =
        ComputeNonExtremeAlign(item.box, item.ascent, item.descent,
                                parent_font_size, parent_ascent,
                                parent_descent, ctx);
    line.max_ascent =
        std::max(line.max_ascent, item.ascent - item.baseline_offset);
    line.max_descent =
        std::max(line.max_descent, item.descent + item.baseline_offset);
  }

  // strut（CSS 2.1 §10.8 隐式 strut）：用 parent ascent/descent 撑高基线行框，
  // 即便所有 item 都是 atomic（descent=0）也能产生合理的 baseline 位置。
  line.max_ascent = std::max(line.max_ascent, parent_ascent);
  line.max_descent = std::max(line.max_descent, parent_descent);

  // Phase 2 — kTop / kBottom 用 max_ 决定 offset，再回写 max_。
  // 推导：
  //   kTop    item 顶 == line 顶 (= baseline_y - max_ascent)
  //           → offset = item.ascent - max_ascent
  //   kBottom item 底 == line 底 (= baseline_y + max_descent)
  //           → offset = max_descent - item.descent
  for (auto& item : line.items) {
    if (!item.box->style) continue;
    const auto va = item.box->style->vertical_align;
    if (va == VerticalAlign::kTop) {
      item.baseline_offset = item.ascent - line.max_ascent;
    } else if (va == VerticalAlign::kBottom) {
      item.baseline_offset = line.max_descent - item.descent;
    }
  }
  for (auto& item : line.items) {
    if (!item.box->style) continue;
    const auto va = item.box->style->vertical_align;
    if (va == VerticalAlign::kTop || va == VerticalAlign::kBottom) {
      line.max_ascent =
          std::max(line.max_ascent, item.ascent - item.baseline_offset);
      line.max_descent =
          std::max(line.max_descent, item.descent + item.baseline_offset);
    }
  }

  // line-height vs metrics height — 半-leading（CSS 2.1 §10.8.1）。
  const f32 metrics_height = line.max_ascent + line.max_descent;
  line.line_height = std::max(line_height_px, metrics_height);
  const f32 leading = line.line_height - metrics_height;
  const f32 half_leading_top = leading * 0.5f;

  // Phase 3 — baseline_y + 写每个 item 的最终 y。
  line.baseline_y = line.top + line.max_ascent + half_leading_top;
  for (auto& item : line.items) {
    item.box->y = line.baseline_y - item.ascent + item.baseline_offset;
  }
}

}  // namespace

LayoutBox* LayoutEngine::CreateBox(LayoutType type, ArenaAllocator& arena) {
  void* mem = arena.Allocate(sizeof(LayoutBox), alignof(LayoutBox));
  auto* box = new (mem) LayoutBox();
  box->type = type;
  return box;
}

f32 LayoutEngine::ResolveFontSize(const css::ComputedStyle* style,
                                  const LayoutContext& ctx) {
  if (!style || style->font_size.is_none()) return ctx.root_font_size;
  return ResolveLength(style->font_size, ctx.root_font_size,
                       ctx.root_font_size, ctx);
}

LayoutBox* LayoutEngine::BuildTree(dom::Element* element,
                                   const css::ComputedStyle* parent_style,
                                   ArenaAllocator& arena,
                                   const LayoutContext& ctx) {
  css::ComputedStyle resolved;
  if (ctx.stylesheets) {
    resolved = css::StyleResolver::Resolve(element, *ctx.stylesheets,
                                           parent_style,
                                           element->inline_declarations(),
                                           ctx.event_manager);
  } else if (parent_style) {
    resolved = *parent_style;
  }

  if (resolved.display == css::Display::kNone) return nullptr;

  LayoutType type = LayoutType::kBlock;
  if (resolved.display == css::Display::kFlex) {
    type = LayoutType::kFlex;
  } else if (resolved.display == css::Display::kInline ||
             resolved.display == css::Display::kInlineBlock) {
    type = LayoutType::kInline;
  }

  void* style_mem =
      arena.Allocate(sizeof(css::ComputedStyle), alignof(css::ComputedStyle));
  auto* style_ptr = new (style_mem) css::ComputedStyle(resolved);

  LayoutBox* box = CreateBox(type, arena);
  box->element = element;
  box->style = style_ptr;

  if (element->tag_id() == dom::TagId::kImg && ctx.image_cache) {
    auto* src_attr = element->GetAttribute(InternedString::Intern("src"));
    if (src_attr) {
      auto result = ctx.image_cache->Load(
          StringView(src_attr->data(), src_attr->size()));
      if (result.ok()) {
        box->type = LayoutType::kReplaced;
        box->image_handle = result.value();
        auto* img = ctx.image_cache->Get(box->image_handle);
        if (img && img->valid()) {
          if (style_ptr->width.is_none() || style_ptr->width.is_auto()) {
            box->content_width = static_cast<f32>(img->width());
          }
          if (style_ptr->height.is_none() || style_ptr->height.is_auto()) {
            box->content_height = static_cast<f32>(img->height());
          }
        }
      }
    }
  }

  for (dom::Node* child = element->first_child(); child;
       child = child->next_sibling()) {
    if (child->is_text()) {
      auto* text = static_cast<dom::Text*>(child);
      if (text->data().size() == 0) continue;
      bool all_ws = true;
      for (usize i = 0; i < text->data().size(); ++i) {
        char c = text->data()[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
          all_ws = false;
          break;
        }
      }
      if (all_ws) continue;
      LayoutBox* text_box = CreateBox(LayoutType::kText, arena);
      text_box->text_node = text;
      text_box->style = style_ptr;
      box->AppendChild(text_box);
    } else if (child->is_element()) {
      auto* child_el = static_cast<dom::Element*>(child);
      LayoutBox* child_box = BuildTree(child_el, style_ptr, arena, ctx);
      if (child_box) box->AppendChild(child_box);
    }
  }

  return box;
}

LayoutBox* LayoutEngine::Layout(dom::Document* doc, const LayoutContext& ctx) {
  static ArenaAllocator layout_arena(8192);
  layout_arena.Reset();
  return Layout(doc, ctx, layout_arena);
}

LayoutBox* LayoutEngine::Layout(dom::Document* doc, const LayoutContext& ctx,
                                ArenaAllocator& arena) {
  LayoutBox* root = BuildTree(doc, nullptr, arena, ctx);
  if (!root) return nullptr;

  LayoutBlock(root, ctx.viewport_width, ctx);
  ApplyPositioning(root, ctx);
  return root;
}

void LayoutEngine::LayoutChild(LayoutBox* child, f32 containing_width,
                               const LayoutContext& ctx) {
  switch (child->type) {
    case LayoutType::kBlock:
      LayoutBlock(child, containing_width, ctx);
      break;
    case LayoutType::kFlex:
      LayoutFlex(child, containing_width, ctx);
      break;
    case LayoutType::kInline:
      LayoutInline(child, containing_width, ctx);
      break;
    case LayoutType::kText:
      if (child->text_node && ctx.text_shaper && child->style) {
        f32 fs = ResolveFontSize(child->style, ctx);
        TextMetrics m = ctx.text_shaper->Measure(
            child->text_node->data(), fs, child->style->font_weight);
        child->content_width = m.width;
        child->content_height = m.height;
      }
      break;
    case LayoutType::kReplaced:
      break;
  }
}

void LayoutEngine::LayoutBlock(LayoutBox* box, f32 containing_width,
                               const LayoutContext& ctx) {
  const css::ComputedStyle* style = box->style;
  if (!style) return;

  f32 font_size = ResolveFontSize(style, ctx);

  ResolveBoxModel(*style, containing_width, font_size, ctx, box->padding,
                  box->border, box->margin);

  if (style->width.is_auto() || style->width.is_none()) {
    box->content_width =
        containing_width - box->padding[LayoutBox::kRight] -
        box->padding[LayoutBox::kLeft] - box->border[LayoutBox::kRight] -
        box->border[LayoutBox::kLeft] - box->margin[LayoutBox::kRight] -
        box->margin[LayoutBox::kLeft];
  } else {
    f32 resolved =
        ResolveLength(style->width, containing_width, font_size, ctx);
    if (style->box_sizing == css::BoxSizing::kBorderBox) {
      resolved -= (box->padding[LayoutBox::kRight] +
                   box->padding[LayoutBox::kLeft] +
                   box->border[LayoutBox::kRight] +
                   box->border[LayoutBox::kLeft]);
    }
    box->content_width = std::max(0.0f, resolved);
  }

  if (!style->min_width.is_none() && !style->min_width.is_auto()) {
    f32 min_w =
        ResolveLength(style->min_width, containing_width, font_size, ctx);
    if (box->content_width < min_w) box->content_width = min_w;
  }
  if (!style->max_width.is_none() && !style->max_width.is_auto()) {
    f32 max_w =
        ResolveLength(style->max_width, containing_width, font_size, ctx);
    if (style->box_sizing == css::BoxSizing::kBorderBox) {
      max_w -= (box->padding[LayoutBox::kRight] +
                box->padding[LayoutBox::kLeft] +
                box->border[LayoutBox::kRight] +
                box->border[LayoutBox::kLeft]);
    }
    if (box->content_width > max_w)
      box->content_width = std::max(0.0f, max_w);
  }

  if (style->margin_left.is_auto() && style->margin_right.is_auto()) {
    f32 remaining = containing_width - box->border_box_width();
    box->margin[LayoutBox::kRight] = box->margin[LayoutBox::kLeft] =
        std::max(0.0f, remaining / 2.0f);
  }

  // ---------------------------------------------------------------------------
  // Children 布局：CSS 2.1 §8.3.1 vertical margin collapsing
  //
  // 算法（creative D1.1 方案 A，spec §5.3 简化版）：
  //   维持「滚动 chain」cur_chain 累积同级相邻 margin。
  //   - sibling collapse：相邻 margin → max(pos)+min(neg)
  //   - collapse-through：空 box 的 top + bottom margin 都进 chain，不占 height
  //   - negative：MarginChain 协议自动处理
  //   - BFC root（overflow != visible）：flush 之前 chain，自身 margin 不并入
  //
  // 范围限制（D1.3，P3 TASK-26-02 完成）：
  //   first/last child 与 parent 的 margin collapse 受 LayoutChild API 边界
  //   约束（保持 D1.2「内部栈式状态」），未跨函数 propagate。当前 R3 仅做
  //   sibling-level collapse；外层 BFC root（root box / overflow!=visible 容器）
  //   自然吸收 first/last margin（margin chain 不出函数边界）。
  // ---------------------------------------------------------------------------
  MarginChain cur_chain;
  f32 y_cursor = 0.0f;
  bool any_in_flow = false;

  for (LayoutBox* child = box->first_child; child;
       child = child->next_sibling) {
    if (!IsInFlow(child)) continue;
    any_in_flow = true;

    // 关键顺序：margin / padding / border / content_height 都由 LayoutChild
    // 解析（含 auto margin 居中、box-sizing、min/max-width 等）。collapse-through
    // 判定亦需 child 已布局完才能读 content_height，因此**必须**先 layout 再
    // 累积 chain。
    LayoutChild(child, box->content_width, ctx);

    if (CreatesBlockFormattingContext(child)) {
      // BFC root 阻断 collapse：先 flush 之前 chain，再独立放置。
      y_cursor += cur_chain.Collapsed();
      cur_chain = MarginChain{};

      child->x = child->margin[LayoutBox::kLeft];
      child->y = y_cursor + child->margin[LayoutBox::kTop];
      y_cursor = child->margin_box_bottom();
      // 不开新 chain — 下一 sibling 的 margin-top 与本 BFC root 的 margin-bottom
      // 不再 collapse。
      continue;
    }

    cur_chain.Add(child->margin[LayoutBox::kTop]);
    child->x = child->margin[LayoutBox::kLeft];
    child->y = y_cursor + cur_chain.Collapsed();

    if (IsCollapseThrough(child)) {
      // 空 box：margin-bottom 也并入当前 chain，不占 vertical 空间。
      // child.y 保持「tentative」位置（绝对位置基准，调试 / hit-test 用）。
      cur_chain.Add(child->margin[LayoutBox::kBottom]);
      child->collapsed_through = true;
      continue;
    }

    // 非 collapse-through：flush chain 至 y_cursor，开始新 chain（仅含
    // child.margin-bottom）。
    y_cursor = child->y + child->border_box_height();
    cur_chain = MarginChain{};
    cur_chain.Add(child->margin[LayoutBox::kBottom]);
  }

  // 末尾 chain 处理：当 parent height auto 时，最后一个 sibling 的
  // margin-bottom（含 collapse-through 链 + 末尾普通 box）计入 content_height。
  // P3 完整 BFC 改造将允许此 chain 跨函数 propagate 到祖先 LayoutBlock。
  f32 trailing_margin = any_in_flow ? cur_chain.Collapsed() : 0.0f;

  if (style->height.is_auto() || style->height.is_none()) {
    box->content_height = y_cursor + trailing_margin;
  } else {
    f32 resolved = ResolveLength(style->height, 0.0f, font_size, ctx);
    if (style->box_sizing == css::BoxSizing::kBorderBox) {
      resolved -= (box->padding[LayoutBox::kTop] +
                   box->padding[LayoutBox::kBottom] +
                   box->border[LayoutBox::kTop] +
                   box->border[LayoutBox::kBottom]);
    }
    box->content_height = std::max(0.0f, resolved);
  }

  if (!style->min_height.is_none() && !style->min_height.is_auto()) {
    f32 min_h = ResolveLength(style->min_height, 0.0f, font_size, ctx);
    if (box->content_height < min_h) box->content_height = min_h;
  }
  if (!style->max_height.is_none() && !style->max_height.is_auto()) {
    f32 max_h = ResolveLength(style->max_height, 0.0f, font_size, ctx);
    if (style->box_sizing == css::BoxSizing::kBorderBox) {
      max_h -= (box->padding[LayoutBox::kTop] +
                box->padding[LayoutBox::kBottom] +
                box->border[LayoutBox::kTop] +
                box->border[LayoutBox::kBottom]);
    }
    if (box->content_height > max_h)
      box->content_height = std::max(0.0f, max_h);
  }
}

void LayoutEngine::LayoutInline(LayoutBox* box, f32 containing_width,
                                const LayoutContext& ctx) {
  // CSS 2.1 §10.8 Inline Formatting Context — TASK-20260426-01 R4 #21。
  //
  // 与 R3 之前的简化路径区别：
  //   1. 显式 LineBox 抽象（每行有 baseline_y / max_ascent / max_descent）
  //   2. TextMetrics 拆 ascent / descent，line-box 计算用真实 baseline 对齐
  //   3. vertical-align 6 关键字 + length（D2.B 严格 2-pass 算法）
  //   4. line-height 半-leading（CSS 2.1 §10.8.1）
  //
  // 不在边界（spec §2 / creative §约束）：
  //   - bidi（unicode-bidi / direction）— 假设 LTR
  //   - inline-block 内部 IFC 递归 — 当 atomic
  //   - 复杂 strut 体系（用 parent style 的 font metrics 简化模拟）

  const css::ComputedStyle* style = box->style;
  const f32 font_size = ResolveFontSize(style, ctx);
  const u16 font_weight = style ? style->font_weight : 400;

  if (style) {
    ResolveBoxModel(*style, containing_width, font_size, ctx, box->padding,
                    box->border, box->margin);
  }

  // 父级度量（用于 vertical-align 关键字解析中的 parent_ascent/descent 与
  // text-top/text-bottom 锚点）。简化策略：用 parent box style 的字体度量。
  f32 parent_ascent = font_size * 0.8f;
  f32 parent_descent = font_size * 0.2f;
  if (ctx.text_shaper) {
    const TextMetrics pm =
        ctx.text_shaper->Measure(StringView("M"), font_size, font_weight);
    parent_ascent = pm.ascent;
    parent_descent = pm.descent;
  }
  const f32 line_height_px = ResolveLineHeightPx(style, font_size, ctx);

  const f32 available = containing_width - box->padding[LayoutBox::kRight] -
                        box->padding[LayoutBox::kLeft];

  Vector<LineBox> lines;
  LineBox cur;
  cur.start_x = 0.0f;
  cur.end_x = 0.0f;
  cur.top = 0.0f;

  // Phase 0：把 children 收集到 LineBox（仅置 child->x，y 在 FinalizeLine 写）。
  for (LayoutBox* child = box->first_child; child;
       child = child->next_sibling) {
    f32 child_width = 0.0f;
    f32 item_ascent = 0.0f;
    f32 item_descent = 0.0f;

    if (child->type == LayoutType::kText && child->text_node &&
        ctx.text_shaper) {
      const TextMetrics m = ctx.text_shaper->Measure(
          child->text_node->data(), font_size, font_weight);
      child->content_width = m.width;
      child->content_height = m.height;
      child_width = m.width;
      item_ascent = m.ascent;
      item_descent = m.descent;
    } else {
      LayoutChild(child, available, ctx);
      child_width = child->margin_box_width();
      // inline-block / replaced / nested inline → atomic
      ComputeInlineMetrics(child, 0.0f, 0.0f, item_ascent, item_descent);
    }

    // 换行检测：超过可用宽度且当前行非空。
    if (cur.end_x + child_width > available && !cur.empty()) {
      FinalizeLine(cur, font_size, parent_ascent, parent_descent,
                    line_height_px, ctx);
      // 下一行的 top 紧贴本行 line_height
      const f32 next_top = cur.top + cur.line_height;
      lines.push_back(std::move(cur));
      cur = LineBox{};
      cur.start_x = 0.0f;
      cur.end_x = 0.0f;
      cur.top = next_top;
    }

    // 写 child->x（最终 x 坐标 = cur.end_x + 左 margin）。y 在 FinalizeLine 写。
    if (child->type == LayoutType::kText) {
      child->x = cur.end_x;
    } else {
      child->x = cur.end_x + child->margin[LayoutBox::kLeft];
    }

    LineBoxItem it;
    it.box = child;
    it.ascent = item_ascent;
    it.descent = item_descent;
    it.baseline_offset = 0.0f;
    cur.items.push_back(it);
    cur.end_x += child_width;
  }

  // Flush last line
  if (!cur.empty()) {
    FinalizeLine(cur, font_size, parent_ascent, parent_descent,
                  line_height_px, ctx);
    lines.push_back(std::move(cur));
  }

  // 容器尺寸：
  //   height = sum(line.line_height)；显式 height 覆盖（inline-block 等 atomic
  //     原子用例下 width/height 必须生效，否则其 ascent = border_box_height = 0
  //     会让 vertical-align 关键字失去物理意义）。
  //   width  = 显式 width 优先；否则 fit-content（取 max line.end_x），避免
  //     默认填满 containing_width 让相邻 inline atomic 必然换行。
  f32 total_height = 0.0f;
  for (const auto& line : lines) total_height += line.line_height;

  if (style && !style->height.is_auto() && !style->height.is_none()) {
    f32 resolved_h = ResolveLength(style->height, 0.0f, font_size, ctx);
    if (style->box_sizing == css::BoxSizing::kBorderBox) {
      resolved_h -= (box->padding[LayoutBox::kTop] +
                     box->padding[LayoutBox::kBottom] +
                     box->border[LayoutBox::kTop] +
                     box->border[LayoutBox::kBottom]);
    }
    box->content_height = std::max(0.0f, resolved_h);
  } else {
    box->content_height = total_height;
  }

  if (style && !style->width.is_auto() && !style->width.is_none()) {
    f32 resolved_w =
        ResolveLength(style->width, containing_width, font_size, ctx);
    if (style->box_sizing == css::BoxSizing::kBorderBox) {
      resolved_w -= (box->padding[LayoutBox::kRight] +
                     box->padding[LayoutBox::kLeft] +
                     box->border[LayoutBox::kRight] +
                     box->border[LayoutBox::kLeft]);
    }
    box->content_width = std::max(0.0f, resolved_w);
  } else {
    f32 max_end = 0.0f;
    for (const auto& line : lines)
      max_end = std::max(max_end, line.end_x);
    box->content_width = max_end;
  }
}

void LayoutEngine::ApplyPositioning(LayoutBox* box, const LayoutContext& ctx) {
  f32 font_size = ResolveFontSize(box->style, ctx);

  for (LayoutBox* child = box->first_child; child;
       child = child->next_sibling) {
    if (!child->style) continue;
    auto pos = child->style->position;

    if (pos == css::Position::kRelative) {
      if (!child->style->left.is_none() && !child->style->left.is_auto()) {
        child->x += ResolveLength(child->style->left, box->content_width,
                                  font_size, ctx);
      } else if (!child->style->right.is_none() &&
                 !child->style->right.is_auto()) {
        child->x -= ResolveLength(child->style->right, box->content_width,
                                  font_size, ctx);
      }
      if (!child->style->top.is_none() && !child->style->top.is_auto()) {
        child->y += ResolveLength(child->style->top, box->content_height,
                                  font_size, ctx);
      } else if (!child->style->bottom.is_none() &&
                 !child->style->bottom.is_auto()) {
        child->y -= ResolveLength(child->style->bottom, box->content_height,
                                  font_size, ctx);
      }
    } else if (pos == css::Position::kAbsolute) {
      f32 child_font = ResolveFontSize(child->style, ctx);
      ResolveBoxModel(*child->style, box->content_width, child_font, ctx,
                      child->padding, child->border, child->margin);

      if (!child->style->left.is_none() && !child->style->left.is_auto()) {
        child->x = ResolveLength(child->style->left, box->content_width,
                                 child_font, ctx) +
                   child->margin[LayoutBox::kLeft];
      }
      if (!child->style->top.is_none() && !child->style->top.is_auto()) {
        child->y = ResolveLength(child->style->top, box->content_height,
                                 child_font, ctx) +
                   child->margin[LayoutBox::kTop];
      }

      if (!child->style->width.is_none() && !child->style->width.is_auto()) {
        child->content_width = ResolveLength(
            child->style->width, box->content_width, child_font, ctx);
      }
      if (!child->style->height.is_none() &&
          !child->style->height.is_auto()) {
        child->content_height = ResolveLength(
            child->style->height, box->content_height, child_font, ctx);
      }

      LayoutChild(child, child->content_width, ctx);
    }

    ApplyPositioning(child, ctx);
  }
}

}  // namespace vx::layout
