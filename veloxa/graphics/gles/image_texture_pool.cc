#include "veloxa/graphics/gles/image_texture_pool.h"

#include "veloxa/graphics/image.h"

namespace vx::gfx::gles {

// RED-phase stub. Real implementation lands in Phase 1B GREEN.

ImageTexturePool::~ImageTexturePool() = default;

GLuint ImageTexturePool::GetOrUpload(const Image&) { return 0; }

void ImageTexturePool::OnContextLost() {}

void ImageTexturePool::OnContextRestored() {}

}  // namespace vx::gfx::gles
