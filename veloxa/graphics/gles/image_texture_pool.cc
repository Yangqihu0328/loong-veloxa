#include "veloxa/graphics/gles/image_texture_pool.h"

#include "veloxa/graphics/image.h"

namespace vx::gfx::gles {

ImageTexturePool::~ImageTexturePool() {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->value.texture != 0) glDeleteTextures(1, &it->value.texture);
  }
}

GLuint ImageTexturePool::GetOrUpload(const Image& image) {
  if (!image.valid()) return 0;
  const vx::u64 key = reinterpret_cast<vx::u64>(image.pixels());

  if (Entry* e = entries_.Find(key)) {
    if (e->width == image.width() && e->height == image.height()) {
      ++cache_hits_;
      return e->texture;
    }
    // Pixel pointer reused for a differently-sized image → drop and re-upload.
    if (e->texture != 0) glDeleteTextures(1, &e->texture);
    entries_.Erase(key);
  }

  ++cache_misses_;
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  // RGBA8 rows are 4-byte aligned by construction (4 bytes/texel).
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
               static_cast<GLsizei>(image.width()),
               static_cast<GLsizei>(image.height()), 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image.pixels());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  entries_.Insert(key, Entry{tex, image.width(), image.height()});
  return tex;
}

void ImageTexturePool::OnContextLost() {
  // GL handles are gone after context loss; do not glDelete (invalid).
  entries_.clear();
}

void ImageTexturePool::OnContextRestored() {
  // Drop cache; textures re-upload lazily on next GetOrUpload.
  entries_.clear();
}

}  // namespace vx::gfx::gles
