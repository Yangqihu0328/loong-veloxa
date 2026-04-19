#ifndef VELOXA_CORE_IMAGE_IMAGE_CACHE_H_
#define VELOXA_CORE_IMAGE_IMAGE_CACHE_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/hash_map.h"
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

  // djb2; mirrors veloxa/core/css/property.cc:84 StringViewHash. Owned String
  // key avoids dangling SSO pointers when `images_` resizes.
  struct StringHash {
    usize operator()(const String& s) const {
      usize h = 5381;
      auto sv = s.view();
      for (usize i = 0; i < sv.size(); ++i) {
        h = ((h << 5) + h) + static_cast<u8>(sv[i]);
      }
      return h;
    }
  };

  struct StringEq {
    bool operator()(const String& a, const String& b) const {
      return a.view() == b.view();
    }
  };

  Vector<Entry> images_;
  HashMap<String, gfx::ImageHandle, StringHash, StringEq> path_to_handle_;
};

}  // namespace vx::image

#endif  // VELOXA_CORE_IMAGE_IMAGE_CACHE_H_
