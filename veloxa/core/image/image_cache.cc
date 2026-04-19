#include "veloxa/core/image/image_cache.h"

#include "veloxa/core/image/image_decoder.h"

namespace vx::image {

StatusOr<gfx::ImageHandle> ImageCache::Load(StringView path) {
  // O(1) hit path via HashMap (TASK-20260419-11 K6 fix).
  String key(path);
  if (auto* existing = path_to_handle_.Find(key)) {
    return *existing;
  }

  auto result = DecodeFromFile(path);
  if (!result.ok()) {
    return result.status();
  }

  Entry entry;
  entry.path = String(path);
  entry.image = static_cast<gfx::Image&&>(result.value());
  images_.push_back(static_cast<Entry&&>(entry));
  auto handle = static_cast<gfx::ImageHandle>(images_.size());
  path_to_handle_.Insert(key, handle);
  return handle;
}

const gfx::Image* ImageCache::Get(gfx::ImageHandle handle) const {
  if (handle == gfx::kInvalidImage || handle > images_.size()) {
    return nullptr;
  }
  return &images_[handle - 1].image;
}

void ImageCache::Clear() {
  images_.clear();
  path_to_handle_.clear();
}

}  // namespace vx::image
