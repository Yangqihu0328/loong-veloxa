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

}  // namespace vx::devtool
