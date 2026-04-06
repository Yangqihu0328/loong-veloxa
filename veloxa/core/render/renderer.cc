#include "veloxa/core/render/renderer.h"

#include <algorithm>

#include "veloxa/core/css/enums.h"
#include "veloxa/core/render/render_utils.h"
#include "veloxa/foundation/containers/small_vector.h"

namespace vx::render {
namespace {

void PaintBorders(layout::LayoutBox* box, const css::ComputedStyle* style,
                  DisplayList& list) {
  f32 bx = box->x - box->padding[layout::LayoutBox::kLeft] -
           box->border[layout::LayoutBox::kLeft];
  f32 by = box->y - box->padding[layout::LayoutBox::kTop] -
           box->border[layout::LayoutBox::kTop];
  f32 bbw = box->border_box_width();
  f32 bbh = box->border_box_height();
  const f32* bw = box->border;

  if (style->border_style[0] != css::BorderStyle::kNone &&
      bw[layout::LayoutBox::kTop] > 0) {
    list.push_back(PaintCommand::FillRect(
        {bx, by, bbw, bw[layout::LayoutBox::kTop]},
        CssColorToGfx(style->border_color[0])));
  }

  if (style->border_style[2] != css::BorderStyle::kNone &&
      bw[layout::LayoutBox::kBottom] > 0) {
    list.push_back(PaintCommand::FillRect(
        {bx, by + bbh - bw[layout::LayoutBox::kBottom], bbw,
         bw[layout::LayoutBox::kBottom]},
        CssColorToGfx(style->border_color[2])));
  }

  if (style->border_style[3] != css::BorderStyle::kNone &&
      bw[layout::LayoutBox::kLeft] > 0) {
    list.push_back(PaintCommand::FillRect(
        {bx, by + bw[layout::LayoutBox::kTop], bw[layout::LayoutBox::kLeft],
         bbh - bw[layout::LayoutBox::kTop] - bw[layout::LayoutBox::kBottom]},
        CssColorToGfx(style->border_color[3])));
  }

  if (style->border_style[1] != css::BorderStyle::kNone &&
      bw[layout::LayoutBox::kRight] > 0) {
    list.push_back(PaintCommand::FillRect(
        {bx + bbw - bw[layout::LayoutBox::kRight],
         by + bw[layout::LayoutBox::kTop], bw[layout::LayoutBox::kRight],
         bbh - bw[layout::LayoutBox::kTop] - bw[layout::LayoutBox::kBottom]},
        CssColorToGfx(style->border_color[1])));
  }
}

void RecordBox(layout::LayoutBox* box, DisplayList& list) {
  const css::ComputedStyle* style = box->style;
  css::ComputedStyle default_style;
  if (!style) style = &default_style;

  if (style->visibility == css::Visibility::kHidden) return;

  f32 bx = box->x - box->padding[layout::LayoutBox::kLeft] -
           box->border[layout::LayoutBox::kLeft];
  f32 by = box->y - box->padding[layout::LayoutBox::kTop] -
           box->border[layout::LayoutBox::kTop];
  f32 bbw = box->border_box_width();
  f32 bbh = box->border_box_height();
  gfx::Rect border_box{bx, by, bbw, bbh};

  bool has_layer = style->opacity < 1.0f;
  if (has_layer)
    list.push_back(PaintCommand::PushLayer(border_box, style->opacity));

  gfx::Color bg = CssColorToGfx(style->background_color);
  if (bg.a > 0) list.push_back(PaintCommand::FillRect(border_box, bg));

  PaintBorders(box, style, list);

  bool has_clip = style->overflow == css::Overflow::kHidden;
  if (has_clip) {
    gfx::Rect content_rect{box->x, box->y, box->content_width,
                           box->content_height};
    list.push_back(PaintCommand::PushClipRect(content_rect));
  }

  if (box->type == layout::LayoutType::kText && box->text_node != nullptr) {
    StringView text_data = box->text_node->data();
    if (!text_data.empty()) {
      gfx::Color text_color = CssColorToGfx(style->color);
      f32 font_size = 16.0f;
      if (style->font_size.unit == css::Unit::kPx) {
        font_size = style->font_size.value;
      }
      gfx::Rect text_rect{box->x, box->y, box->content_width,
                           box->content_height};
      list.push_back(
          PaintCommand::DrawText(text_data, text_rect, font_size, text_color));
    }
  }

  SmallVector<layout::LayoutBox*, 16> children;
  for (auto* child = box->first_child; child; child = child->next_sibling) {
    children.push_back(child);
  }
  std::stable_sort(
      children.begin(), children.end(),
      [](const layout::LayoutBox* a, const layout::LayoutBox* b) {
        i32 za = a->style ? a->style->z_index : 0;
        i32 zb = b->style ? b->style->z_index : 0;
        return za < zb;
      });
  for (auto* child : children) {
    RecordBox(child, list);
  }

  if (has_clip) list.push_back(PaintCommand::PopClip());
  if (has_layer) list.push_back(PaintCommand::PopLayer());
}

}  // namespace

DisplayList Record(layout::LayoutBox* root) {
  DisplayList list;
  if (!root) return list;
  RecordBox(root, list);
  return list;
}

void Replay(const DisplayList& list, gfx::Canvas* canvas) {
  for (const auto& cmd : list) {
    switch (cmd.type) {
      case PaintCommand::Type::kFillRect:
        canvas->FillRect(cmd.rect, gfx::Brush::Solid(cmd.color));
        break;
      case PaintCommand::Type::kDrawText:
        canvas->DrawText(cmd.text, cmd.rect, cmd.param,
                         gfx::Brush::Solid(cmd.color));
        break;
      case PaintCommand::Type::kPushClipRect:
        canvas->PushClipRect(cmd.rect);
        break;
      case PaintCommand::Type::kPopClip:
        canvas->PopClip();
        break;
      case PaintCommand::Type::kPushLayer:
        canvas->PushLayer(cmd.rect, cmd.param);
        break;
      case PaintCommand::Type::kPopLayer:
        canvas->PopLayer();
        break;
    }
  }
}

void Paint(layout::LayoutBox* root, gfx::Canvas* canvas) {
  DisplayList list = Record(root);
  Replay(list, canvas);
}

gfx::Rect ComputeDirtyRect(const DisplayList& old_list,
                            const DisplayList& new_list,
                            f32 viewport_width, f32 viewport_height) {
  if (old_list.size() != new_list.size()) {
    return {0, 0, viewport_width, viewport_height};
  }
  gfx::Rect dirty{0, 0, 0, 0};
  for (usize i = 0; i < old_list.size(); ++i) {
    if (old_list[i] != new_list[i]) {
      dirty = gfx::Rect::Union(dirty, old_list[i].rect);
      dirty = gfx::Rect::Union(dirty, new_list[i].rect);
    }
  }
  return dirty;
}

}  // namespace vx::render
