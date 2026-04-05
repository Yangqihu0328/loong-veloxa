#include "veloxa/core/layout/flex_layout.h"

#include <algorithm>

#include "veloxa/core/layout/layout_utils.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::layout {

void LayoutFlex(LayoutBox* box, f32 containing_width,
                const LayoutContext& ctx) {
  const auto* style = box->style;
  if (!style) return;

  bool is_row = (style->flex_direction == css::FlexDirection::kRow ||
                 style->flex_direction == css::FlexDirection::kRowReverse);
  bool is_reverse =
      (style->flex_direction == css::FlexDirection::kRowReverse ||
       style->flex_direction == css::FlexDirection::kColumnReverse);

  f32 font_size =
      style->font_size.is_none()
          ? ctx.root_font_size
          : ResolveLength(style->font_size, ctx.root_font_size,
                          ctx.root_font_size, ctx);

  // §9.2  Resolve container box model.
  ResolveBoxModel(*style, containing_width, font_size, ctx, box->padding,
                  box->border, box->margin);

  // §9.2  Determine container main size.
  f32 container_main;
  if (is_row) {
    if (style->width.is_auto() || style->width.is_none()) {
      container_main = containing_width - box->padding[1] - box->padding[3] -
                       box->border[1] - box->border[3] - box->margin[1] -
                       box->margin[3];
    } else {
      container_main =
          ResolveLength(style->width, containing_width, font_size, ctx);
      if (style->box_sizing == css::BoxSizing::kBorderBox)
        container_main -= (box->padding[1] + box->padding[3] + box->border[1] +
                           box->border[3]);
    }
    box->content_width = std::max(0.0f, container_main);
  } else {
    if (style->width.is_auto() || style->width.is_none()) {
      box->content_width = containing_width - box->padding[1] -
                           box->padding[3] - box->border[1] - box->border[3] -
                           box->margin[1] - box->margin[3];
    } else {
      box->content_width =
          ResolveLength(style->width, containing_width, font_size, ctx);
      if (style->box_sizing == css::BoxSizing::kBorderBox)
        box->content_width -= (box->padding[1] + box->padding[3] +
                               box->border[1] + box->border[3]);
    }
    if (!style->height.is_auto() && !style->height.is_none()) {
      container_main = ResolveLength(style->height, 0, font_size, ctx);
      if (style->box_sizing == css::BoxSizing::kBorderBox)
        container_main -= (box->padding[0] + box->padding[2] + box->border[0] +
                           box->border[2]);
    } else {
      container_main = 1e6f;
    }
  }

  f32 gap_size = ResolveLength(style->gap, container_main, font_size, ctx);

  // §9.3  Collect flex items and compute hypothetical main sizes.
  struct FlexItem {
    LayoutBox* box;
    f32 flex_base;
    f32 hypothetical_main;
    f32 grow;
    f32 shrink;
    f32 cross_size;
    bool frozen = false;
  };

  Vector<FlexItem> items;
  for (LayoutBox* child = box->first_child; child; child = child->next_sibling) {
    if (!child->style) continue;
    if (child->style->position == css::Position::kAbsolute) continue;

    f32 child_font =
        child->style->font_size.is_none()
            ? font_size
            : ResolveLength(child->style->font_size, font_size,
                            ctx.root_font_size, ctx);

    ResolveBoxModel(*child->style, box->content_width, child_font, ctx,
                    child->padding, child->border, child->margin);

    f32 base;
    if (!child->style->flex_basis.is_none() &&
        !child->style->flex_basis.is_auto()) {
      base = ResolveLength(child->style->flex_basis, container_main,
                           child_font, ctx);
    } else if (is_row && !child->style->width.is_none() &&
               !child->style->width.is_auto()) {
      base =
          ResolveLength(child->style->width, container_main, child_font, ctx);
    } else if (!is_row && !child->style->height.is_none() &&
               !child->style->height.is_auto()) {
      base =
          ResolveLength(child->style->height, container_main, child_font, ctx);
    } else {
      base = 0;
    }

    f32 hyp = base;

    const auto& min_prop = is_row ? child->style->min_width
                                  : child->style->min_height;
    if (!min_prop.is_none() && !min_prop.is_auto()) {
      f32 min_val = ResolveLength(min_prop, container_main, child_font, ctx);
      if (hyp < min_val) hyp = min_val;
    }

    const auto& max_prop = is_row ? child->style->max_width
                                  : child->style->max_height;
    if (!max_prop.is_none() && !max_prop.is_auto()) {
      f32 max_val = ResolveLength(max_prop, container_main, child_font, ctx);
      if (hyp > max_val) hyp = max_val;
    }

    FlexItem item;
    item.box = child;
    item.flex_base = base;
    item.hypothetical_main = hyp;
    item.grow = child->style->flex_grow;
    item.shrink = child->style->flex_shrink;
    item.cross_size = 0;
    items.push_back(item);
  }

  // §9.3  Collect flex lines.
  struct FlexLine {
    u32 start;
    u32 end;
    f32 cross_size;
  };

  Vector<FlexLine> lines;
  if (style->flex_wrap == css::FlexWrap::kNowrap || items.size() == 0) {
    lines.push_back({0, static_cast<u32>(items.size()), 0});
  } else {
    u32 line_start = 0;
    f32 line_main = 0;
    for (u32 i = 0; i < items.size(); ++i) {
      f32 item_outer = items[i].hypothetical_main;
      if (is_row) {
        item_outer += items[i].box->margin[1] + items[i].box->margin[3] +
                      items[i].box->padding[1] + items[i].box->padding[3] +
                      items[i].box->border[1] + items[i].box->border[3];
      } else {
        item_outer += items[i].box->margin[0] + items[i].box->margin[2] +
                      items[i].box->padding[0] + items[i].box->padding[2] +
                      items[i].box->border[0] + items[i].box->border[2];
      }

      if (line_main + item_outer > container_main && i > line_start) {
        lines.push_back({line_start, i, 0});
        line_start = i;
        line_main = 0;
      }
      line_main += item_outer;
      if (i > line_start) line_main += gap_size;
    }
    if (line_start < items.size()) {
      lines.push_back({line_start, static_cast<u32>(items.size()), 0});
    }
  }

  // §9.7  Resolve flexible lengths per line.
  for (auto& line : lines) {
    f32 total_hyp = 0;
    f32 total_gaps =
        (line.end > line.start + 1)
            ? gap_size * static_cast<f32>(line.end - line.start - 1)
            : 0;
    for (u32 i = line.start; i < line.end; ++i) {
      total_hyp += items[i].hypothetical_main;
      if (is_row) {
        total_hyp += items[i].box->margin[1] + items[i].box->margin[3] +
                     items[i].box->padding[1] + items[i].box->padding[3] +
                     items[i].box->border[1] + items[i].box->border[3];
      } else {
        total_hyp += items[i].box->margin[0] + items[i].box->margin[2] +
                     items[i].box->padding[0] + items[i].box->padding[2] +
                     items[i].box->border[0] + items[i].box->border[2];
      }
    }

    f32 free_space = container_main - total_hyp - total_gaps;

    if (free_space > 0) {
      f32 total_grow = 0;
      for (u32 i = line.start; i < line.end; ++i) total_grow += items[i].grow;
      if (total_grow > 0) {
        for (u32 i = line.start; i < line.end; ++i) {
          items[i].hypothetical_main +=
              free_space * (items[i].grow / total_grow);
        }
      }
    } else if (free_space < 0) {
      f32 total_shrink = 0;
      for (u32 i = line.start; i < line.end; ++i)
        total_shrink += items[i].shrink * items[i].flex_base;
      if (total_shrink > 0) {
        for (u32 i = line.start; i < line.end; ++i) {
          f32 ratio = (items[i].shrink * items[i].flex_base) / total_shrink;
          items[i].hypothetical_main += free_space * ratio;
          if (items[i].hypothetical_main < 0) items[i].hypothetical_main = 0;
        }
      }
    }
  }

  // §9.4  Set item content sizes and compute cross sizes.
  for (auto& item : items) {
    f32 child_font =
        item.box->style->font_size.is_none()
            ? font_size
            : ResolveLength(item.box->style->font_size, font_size,
                            ctx.root_font_size, ctx);
    if (is_row) {
      item.box->content_width = std::max(0.0f, item.hypothetical_main);
      if (!item.box->style->height.is_none() &&
          !item.box->style->height.is_auto()) {
        item.box->content_height =
            ResolveLength(item.box->style->height, 0, child_font, ctx);
      }
      item.cross_size = item.box->border_box_height() + item.box->margin[0] +
                        item.box->margin[2];
    } else {
      item.box->content_height = std::max(0.0f, item.hypothetical_main);
      if (!item.box->style->width.is_none() &&
          !item.box->style->width.is_auto()) {
        item.box->content_width = ResolveLength(item.box->style->width,
                                                box->content_width,
                                                child_font, ctx);
      } else {
        item.box->content_width = box->content_width;
      }
      item.cross_size = item.box->border_box_width() + item.box->margin[1] +
                        item.box->margin[3];
    }
  }

  // §9.4  Determine cross size of each flex line.
  for (auto& line : lines) {
    f32 max_cross = 0;
    for (u32 i = line.start; i < line.end; ++i) {
      if (items[i].cross_size > max_cross) max_cross = items[i].cross_size;
    }
    line.cross_size = max_cross;
  }

  // §9.5  Main-axis alignment (justify-content) and position writing.
  f32 cross_offset = 0;
  for (auto& line : lines) {
    f32 used_main = 0;
    u32 count = line.end - line.start;
    for (u32 i = line.start; i < line.end; ++i) {
      if (is_row) {
        used_main += items[i].box->border_box_width() +
                     items[i].box->margin[1] + items[i].box->margin[3];
      } else {
        used_main += items[i].box->border_box_height() +
                     items[i].box->margin[0] + items[i].box->margin[2];
      }
    }
    used_main += gap_size * (count > 1 ? count - 1 : 0);
    f32 remaining = container_main - used_main;

    f32 main_offset = 0;
    f32 item_gap = gap_size;

    switch (style->justify_content) {
      case css::JustifyContent::kFlexStart:
        main_offset = 0;
        break;
      case css::JustifyContent::kFlexEnd:
        main_offset = remaining;
        break;
      case css::JustifyContent::kCenter:
        main_offset = remaining / 2;
        break;
      case css::JustifyContent::kSpaceBetween:
        main_offset = 0;
        if (count > 1) item_gap = gap_size + remaining / (count - 1);
        break;
      case css::JustifyContent::kSpaceAround:
        if (count > 0) {
          f32 space = remaining / count;
          main_offset = space / 2;
          item_gap = gap_size + space;
        }
        break;
    }

    if (is_reverse) {
      main_offset = container_main;
      for (u32 idx = line.start; idx < line.end; ++idx) {
        auto& it = items[idx];
        if (is_row) {
          main_offset -= it.box->margin[1] + it.box->border_box_width();
          it.box->x = main_offset;
          main_offset -= it.box->margin[3] + item_gap;
        } else {
          main_offset -= it.box->margin[2] + it.box->border_box_height();
          it.box->y = main_offset;
          main_offset -= it.box->margin[0] + item_gap;
        }
      }
    } else {
      for (u32 i = line.start; i < line.end; ++i) {
        auto& it = items[i];
        if (is_row) {
          it.box->x = main_offset + it.box->margin[3];
          main_offset += it.box->margin[3] + it.box->border_box_width() +
                         it.box->margin[1];
          if (i < line.end - 1) main_offset += item_gap;
        } else {
          it.box->y = main_offset + it.box->margin[0];
          main_offset += it.box->margin[0] + it.box->border_box_height() +
                         it.box->margin[2];
          if (i < line.end - 1) main_offset += item_gap;
        }
      }
    }

    // §9.6  Cross-axis alignment (align-items / align-self).
    for (u32 i = line.start; i < line.end; ++i) {
      auto& it = items[i];
      css::AlignItems align = style->align_items;
      if (it.box->style->align_self != css::AlignItems::kAuto) {
        align = it.box->style->align_self;
      }

      f32 item_cross;
      if (is_row) {
        item_cross = it.box->border_box_height() + it.box->margin[0] +
                     it.box->margin[2];
      } else {
        item_cross = it.box->border_box_width() + it.box->margin[1] +
                     it.box->margin[3];
      }

      f32 cross_pos = 0;
      switch (align) {
        case css::AlignItems::kFlexStart:
        case css::AlignItems::kAuto:
          cross_pos = 0;
          break;
        case css::AlignItems::kFlexEnd:
          cross_pos = line.cross_size - item_cross;
          break;
        case css::AlignItems::kCenter:
          cross_pos = (line.cross_size - item_cross) / 2;
          break;
        case css::AlignItems::kStretch:
          cross_pos = 0;
          if (is_row) {
            if (it.box->style->height.is_auto() ||
                it.box->style->height.is_none()) {
              it.box->content_height =
                  line.cross_size - it.box->padding[0] - it.box->padding[2] -
                  it.box->border[0] - it.box->border[2] - it.box->margin[0] -
                  it.box->margin[2];
              if (it.box->content_height < 0) it.box->content_height = 0;
            }
          } else {
            if (it.box->style->width.is_auto() ||
                it.box->style->width.is_none()) {
              it.box->content_width =
                  line.cross_size - it.box->padding[1] - it.box->padding[3] -
                  it.box->border[1] - it.box->border[3] - it.box->margin[1] -
                  it.box->margin[3];
              if (it.box->content_width < 0) it.box->content_width = 0;
            }
          }
          break;
        case css::AlignItems::kBaseline:
          cross_pos = 0;
          break;
      }

      if (is_row) {
        it.box->y = cross_offset + cross_pos + it.box->margin[0];
      } else {
        it.box->x = cross_offset + cross_pos + it.box->margin[3];
      }
    }

    cross_offset += line.cross_size;
  }

  // §9.9  Determine container cross size.
  if (is_row) {
    if (style->height.is_auto() || style->height.is_none()) {
      box->content_height = cross_offset;
    } else {
      box->content_height = ResolveLength(style->height, 0, font_size, ctx);
      if (style->box_sizing == css::BoxSizing::kBorderBox)
        box->content_height -= (box->padding[0] + box->padding[2] +
                                box->border[0] + box->border[2]);
    }
  } else {
    if (style->height.is_auto() || style->height.is_none()) {
      f32 max_y = 0;
      for (auto& item : items) {
        f32 bottom =
            item.box->y + item.box->border_box_height() + item.box->margin[2];
        if (bottom > max_y) max_y = bottom;
      }
      box->content_height = max_y;
    }
  }
}

}  // namespace vx::layout
