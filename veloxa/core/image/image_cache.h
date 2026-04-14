#ifndef VELOXA_CORE_IMAGE_IMAGE_CACHE_H_
#define VELOXA_CORE_IMAGE_IMAGE_CACHE_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/string.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/image.h"

namespace vx::image {

class ImageCache {
 public:
  StatusOr<gfx::ImageHandle> Load(StringView path);
  const gfx::Image* Get(gfx::ImageHandle handle) const;
  void Clear();
  usize size() const { return images_.size(); }

 private:
  struct Entry {
    String path;
    gfx::Image image;
  };
  Vector<Entry> images_;
};

}  // namespace vx::image

#endif  // VELOXA_CORE_IMAGE_IMAGE_CACHE_H_
