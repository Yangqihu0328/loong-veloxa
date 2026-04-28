#include "veloxa/core/layout/layout_engine.h"

#include <algorithm>
#include <new>

#include "veloxa/core/css/style_resolver.h"
#include "veloxa/core/image/image_cache.h"
#include "veloxa/core/layout/flex_layout.h"
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
  const css::ComputedStyle* style = box->style;
  f32 font_size = ResolveFontSize(style, ctx);
  u16 font_weight = style ? style->font_weight : 400;

  if (style) {
    ResolveBoxModel(*style, containing_width, font_size, ctx, box->padding,
                    box->border, box->margin);
  }

  f32 available = containing_width - box->padding[LayoutBox::kRight] -
                  box->padding[LayoutBox::kLeft];
  f32 x_offset = 0;
  f32 y_offset = 0;
  f32 line_height = 0;

  for (LayoutBox* child = box->first_child; child;
       child = child->next_sibling) {
    if (child->type == LayoutType::kText && child->text_node &&
        ctx.text_shaper) {
      TextMetrics metrics = ctx.text_shaper->Measure(
          child->text_node->data(), font_size, font_weight);
      child->content_width = metrics.width;
      child->content_height = metrics.height;

      if (x_offset + metrics.width > available && x_offset > 0) {
        y_offset += line_height;
        x_offset = 0;
        line_height = 0;
      }

      child->x = x_offset;
      child->y = y_offset;
      x_offset += metrics.width;
      line_height = std::max(line_height, metrics.height);
    } else {
      LayoutChild(child, available, ctx);
      if (x_offset + child->margin_box_width() > available && x_offset > 0) {
        y_offset += line_height;
        x_offset = 0;
        line_height = 0;
      }
      child->x = x_offset + child->margin[LayoutBox::kLeft];
      child->y = y_offset + child->margin[LayoutBox::kTop];
      x_offset += child->margin_box_width();
      line_height = std::max(line_height, child->margin_box_height());
    }
  }

  box->content_height = y_offset + line_height;
  if (!style || style->width.is_auto() || style->width.is_none()) {
    box->content_width = containing_width - box->padding[LayoutBox::kRight] -
                         box->padding[LayoutBox::kLeft];
  } else {
    box->content_width =
        ResolveLength(style->width, containing_width, font_size, ctx);
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
