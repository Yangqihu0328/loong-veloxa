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

}  // namespace vx::render

#endif  // VELOXA_CORE_RENDER_RENDERER_H_
