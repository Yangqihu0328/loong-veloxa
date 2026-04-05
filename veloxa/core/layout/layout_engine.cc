#include "veloxa/core/layout/layout_engine.h"

#include <algorithm>
#include <new>

#include "veloxa/core/css/style_resolver.h"
#include "veloxa/core/layout/flex_layout.h"
#include "veloxa/core/layout/text_shaper.h"

namespace vx::layout {

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
    resolved =
        css::StyleResolver::Resolve(element, *ctx.stylesheets, parent_style);
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

  LayoutBox* root = BuildTree(doc, nullptr, layout_arena, ctx);
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

  f32 y_offset = 0;
  for (LayoutBox* child = box->first_child; child;
       child = child->next_sibling) {
    if (child->style &&
        child->style->position == css::Position::kAbsolute)
      continue;
    LayoutChild(child, box->content_width, ctx);
    child->x = child->margin[LayoutBox::kLeft];
    child->y = y_offset + child->margin[LayoutBox::kTop];
    y_offset =
        child->y + child->border_box_height() + child->margin[LayoutBox::kBottom];
  }

  if (style->height.is_auto() || style->height.is_none()) {
    box->content_height = y_offset;
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
