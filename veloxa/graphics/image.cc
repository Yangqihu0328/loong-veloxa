#include "veloxa/graphics/image.h"

namespace vx::gfx {

Image::Image(u32 width, u32 height)
    : width_(width),
      height_(height),
      pixels_(new u32[static_cast<usize>(width) * height]()) {}

}  // namespace vx::gfx
