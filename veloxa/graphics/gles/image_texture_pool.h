#ifndef VELOXA_GRAPHICS_GLES_IMAGE_TEXTURE_POOL_H_
#define VELOXA_GRAPHICS_GLES_IMAGE_TEXTURE_POOL_H_

#include <GLES3/gl3.h>

#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/hash_map.h"

namespace vx::gfx {
class Image;
}  // namespace vx::gfx

namespace vx::gfx::gles {

// RGBA8 image → GL texture cache. Because vx::gfx::Image carries no handle
// (D2=B reconcile), the cache key is the image's pixel pointer plus a (w,h)
// validation guard so a reused pointer pointing at a differently-sized image
// triggers a re-upload rather than returning a stale texture.
//
// Does not own the Image; the caller guarantees the Image (and thus the
// pixel-pointer key) stays valid for the cached lifetime. Caller must hold the
// GL context current.
class ImageTexturePool {
 public:
  ImageTexturePool() = default;
  ~ImageTexturePool();

  ImageTexturePool(const ImageTexturePool&) = delete;
  ImageTexturePool& operator=(const ImageTexturePool&) = delete;

  // Cache hit (same pixel pointer + same w/h) → returns cached texture.
  // Otherwise uploads a new GL_RGBA8 texture. Returns 0 for an invalid image.
  GLuint GetOrUpload(const Image& image);

  void OnContextLost();      // GL handles gone — clear cache, no glDelete
  void OnContextRestored();  // clear cache — lazy re-upload on next GetOrUpload

  vx::u64 cache_hits() const { return cache_hits_; }
  vx::u64 cache_misses() const { return cache_misses_; }
  vx::usize size() const { return entries_.size(); }

 private:
  struct Entry {
    GLuint texture;
    vx::u32 width;
    vx::u32 height;
  };
  vx::HashMap<vx::u64, Entry> entries_;  // key = (u64)image.pixels()
  vx::u64 cache_hits_ = 0;
  vx::u64 cache_misses_ = 0;
};

}  // namespace vx::gfx::gles

#endif  // VELOXA_GRAPHICS_GLES_IMAGE_TEXTURE_POOL_H_
