#include "veloxa/core/event/hit_test.h"

#include <algorithm>

#include "veloxa/core/css/computed_style.h"
#include "veloxa/foundation/containers/vector.h"

namespace vx::event {
namespace {

dom::Element* HitTestBox(layout::LayoutBox* box, f32 x, f32 y) {
  const auto* style = box->style;

  if (style && style->visibility == css::Visibility::kHidden) return nullptr;

  if (style && style->overflow == css::Overflow::kHidden) {
    f32 px = box->x - box->padding[layout::LayoutBox::kLeft];
    f32 py = box->y - box->padding[layout::LayoutBox::kTop];
    f32 pw = box->padding_box_width();
    f32 ph = box->padding_box_height();
    if (x < px || x >= px + pw || y < py || y >= py + ph) return nullptr;
  }

  Vector<layout::LayoutBox*> children;
  for (auto* c = box->first_child; c; c = c->next_sibling)
    children.push_back(c);

  std::stable_sort(children.begin(), children.end(),
                   [](const layout::LayoutBox* a, const layout::LayoutBox* b) {
                     i32 za = a->style ? a->style->z_index : 0;
                     i32 zb = b->style ? b->style->z_index : 0;
                     return za < zb;
                   });

  for (usize i = children.size(); i-- > 0;) {
    dom::Element* result = HitTestBox(children[i], x, y);
    if (result) return result;
  }

  f32 bx = box->x - box->padding[layout::LayoutBox::kLeft] -
           box->border[layout::LayoutBox::kLeft];
  f32 by = box->y - box->padding[layout::LayoutBox::kTop] -
           box->border[layout::LayoutBox::kTop];
  f32 bw = box->border_box_width();
  f32 bh = box->border_box_height();

  if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
    if (box->element) return box->element;
    if (box->parent && box->parent->element) return box->parent->element;
  }

  return nullptr;
}

}  // namespace

dom::Element* HitTest(layout::LayoutBox* root, f32 x, f32 y) {
  if (!root) return nullptr;
  return HitTestBox(root, x, y);
}

}  // namespace vx::event
