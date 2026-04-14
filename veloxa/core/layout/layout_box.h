#ifndef VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_
#define VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_

#include "veloxa/core/css/computed_style.h"
#include "veloxa/core/dom/element.h"
#include "veloxa/core/dom/text.h"
#include "veloxa/foundation/base/types.h"

namespace vx::layout {

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

  u32 child_count() const {
    u32 count = 0;
    for (auto* c = first_child; c; c = c->next_sibling) ++count;
    return count;
  }
};

}  // namespace vx::layout

#endif  // VELOXA_CORE_LAYOUT_LAYOUT_BOX_H_
