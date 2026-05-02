#include "veloxa/devtool/inspector/inspector_overlay.h"

namespace vx::devtool {

void InspectorOverlay::InjectHoverHighlight(render::DisplayList& list,
                                             const layout::LayoutBox* hovered_box,
                                             gfx::Color color,
                                             f32 stroke_width) {
  if (hovered_box == nullptr) {
    return;
  }
  const gfx::Rect border_box{hovered_box->x, hovered_box->y,
                              hovered_box->border_box_width(),
                              hovered_box->border_box_height()};
  list.push_back(
      render::PaintCommand::OverlayHighlight(border_box, color, stroke_width));
}

void InspectorOverlay::InjectDirtyRectHighlights(
    render::DisplayList& list, const Vector<gfx::Rect>& rects) {
  for (vx::usize i = 0; i < rects.size(); i++) {
    if (rects[i].IsEmpty()) continue;
    list.push_back(render::PaintCommand::OverlayDirtyRect(rects[i]));
  }
}

}  // namespace vx::devtool
