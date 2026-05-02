#ifndef VELOXA_CORE_RENDER_RENDERER_H_
#define VELOXA_CORE_RENDER_RENDERER_H_

#include "veloxa/core/layout/layout_box.h"
#include "veloxa/core/render/paint_command.h"
#include "veloxa/graphics/canvas.h"

namespace vx::image {
class ImageCache;
}

namespace vx::render {

DisplayList Record(layout::LayoutBox* root,
                   image::ImageCache* image_cache = nullptr);

void Replay(const DisplayList& list, gfx::Canvas* canvas,
            image::ImageCache* image_cache = nullptr);

void Paint(layout::LayoutBox* root, gfx::Canvas* canvas,
           image::ImageCache* image_cache = nullptr);

gfx::Rect ComputeDirtyRect(const DisplayList& old_list,
                            const DisplayList& new_list,
                            f32 viewport_width, f32 viewport_height);

// T5 mitigation (TASK-20260502-01 A.0.4): erase all PaintCommand entries with
// type == kOverlayHighlight from the DisplayList. Called by the DevTool
// subsystem at the start of each frame to prevent overlay command
// accumulation across frames or injection via shared DisplayList state.
// Non-overlay commands (kFillRect / kDrawText / etc.) are preserved.
void ResetOverlayCommands(DisplayList& list);

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_RENDERER_H_
