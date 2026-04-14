#include "veloxa/core/image/image_cache.h"

#include "veloxa/core/image/image_decoder.h"

namespace vx::image {

StatusOr<gfx::ImageHandle> ImageCache::Load(StringView path) {
  for (usize i = 0; i < images_.size(); ++i) {
    if (images_[i].path.view() == path) {
      return static_cast<gfx::ImageHandle>(i + 1);
    }
  }

  auto result = DecodeFromFile(path);
  if (!result.ok()) {
    return result.status();
  }

  Entry entry;
  entry.path = String(path);
  entry.image = static_cast<gfx::Image&&>(result.value());
  images_.push_back(static_cast<Entry&&>(entry));
  return static_cast<gfx::ImageHandle>(images_.size());
}

const gfx::Image* ImageCache::Get(gfx::ImageHandle handle) const {
  if (handle == gfx::kInvalidImage || handle > images_.size()) {
    return nullptr;
  }
  return &images_[handle - 1].image;
}

void ImageCache::Clear() { images_.clear(); }

}  // namespace vx::image
