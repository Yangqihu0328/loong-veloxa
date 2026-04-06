#ifndef VELOXA_CORE_RENDER_RENDERER_H_
#define VELOXA_CORE_RENDER_RENDERER_H_

#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/graphics/canvas.h"

namespace vx::render {

DisplayList Record(layout::LayoutBox* root);

void Replay(const DisplayList& list, gfx::Canvas* canvas);

void Paint(layout::LayoutBox* root, gfx::Canvas* canvas);

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_RENDERER_H_
