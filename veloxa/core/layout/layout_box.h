#ifndef VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_
#define VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/base/types.h"

namespace vx::layout {

// 2D point in layout-space coordinates (相对 parent 工作坐标系)。
// 用于 LayoutBox::*_box_origin() helper 返回类型，避免 (x, y) 双 f32 拼接式调用。
struct Point {
  f32 x;
  f32 y;
};

enum class LayoutType : u8 {
  kBlock,
  kInline,
  kFlex,
  kText,
  kReplaced,
};

struct LayoutBox {
  LayoutType type = LayoutType::kBlock;
  dom::Element* element = nullptr;
  dom::Text* text_node = nullptr;
  const css::ComputedStyle* style = nullptr;

  LayoutBox* parent = nullptr;
  LayoutBox* first_child = nullptr;
  LayoutBox* last_child = nullptr;
  LayoutBox* next_sibling = nullptr;
  LayoutBox* prev_sibling = nullptr;

  u32 image_handle = 0;

  f32 x = 0, y = 0;
  f32 content_width = 0, content_height = 0;
  f32 padding[4] = {};
  f32 border[4] = {};
  f32 margin[4] = {};

  enum { kTop = 0, kRight = 1, kBottom = 2, kLeft = 3 };

  void AppendChild(LayoutBox* child) {
    child->parent = this;
    child->prev_sibling = last_child;
    child->next_sibling = nullptr;
    if (last_child) {
      last_child->next_sibling = child;
    } else {
      first_child = child;
    }
    last_child = child;
  }

  f32 padding_box_width() const {
    return content_width + padding[kRight] + padding[kLeft];
  }
  f32 padding_box_height() const {
    return content_height + padding[kTop] + padding[kBottom];
  }
  f32 border_box_width() const {
    return padding_box_width() + border[kRight] + border[kLeft];
  }
  f32 border_box_height() const {
    return padding_box_height() + border[kTop] + border[kBottom];
  }
  f32 margin_box_width() const {
    return border_box_width() + margin[kRight] + margin[kLeft];
  }
  f32 margin_box_height() const {
    return border_box_height() + margin[kTop] + margin[kBottom];
  }

  // ===========================================================================
  // Origin helpers (CSS box model coordinates).
  //
  // 约定：(x, y) 是 border-box 左上角，由布局阶段累加 margin 后写入。
  // 调用方决定坐标系含义（绝对坐标 / 相对 parent padding-box）。
  // ===========================================================================
  Point border_box_origin() const { return {x, y}; }
  Point padding_box_origin() const {
    return {x + border[kLeft], y + border[kTop]};
  }
  Point content_box_origin() const {
    return {x + border[kLeft] + padding[kLeft],
            y + border[kTop] + padding[kTop]};
  }

  // border-box four sides
  f32 border_box_top() const { return y; }
  f32 border_box_left() const { return x; }
  f32 border_box_right() const { return x + border_box_width(); }
  f32 border_box_bottom() const { return y + border_box_height(); }

  // padding-box four sides
  f32 padding_box_top() const { return y + border[kTop]; }
  f32 padding_box_left() const { return x + border[kLeft]; }
  f32 padding_box_right() const {
    return padding_box_left() + padding_box_width();
  }
  f32 padding_box_bottom() const {
    return padding_box_top() + padding_box_height();
  }

  // content-box four sides
  f32 content_box_top() const { return y + border[kTop] + padding[kTop]; }
  f32 content_box_left() const { return x + border[kLeft] + padding[kLeft]; }
  f32 content_box_right() const { return content_box_left() + content_width; }
  f32 content_box_bottom() const { return content_box_top() + content_height; }

  // margin-box four sides (border-box + outward margin)
  f32 margin_box_top() const { return y - margin[kTop]; }
  f32 margin_box_left() const { return x - margin[kLeft]; }
  f32 margin_box_right() const { return border_box_right() + margin[kRight]; }
  f32 margin_box_bottom() const {
    return border_box_bottom() + margin[kBottom];
  }

  u32 child_count() const {
    u32 count = 0;
    for (auto* c = first_child; c; c = c->next_sibling) ++count;
    return count;
  }
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_
