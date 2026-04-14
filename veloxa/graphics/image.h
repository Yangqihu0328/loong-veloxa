#ifndef VELOXA_GRAPHICS_IMAGE_H_
#define VELOXA_GRAPHICS_IMAGE_H_

#include <memory>

#include "veloxa/foundation/base/types.h"

namespace vx::gfx {

class Image {
 public:
  Image() = default;
  Image(u32 width, u32 height);

  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  Image(Image&& other) noexcept
      : width_(other.width_),
        height_(other.height_),
        pixels_(static_cast<std::unique_ptr<u32[]>&&>(other.pixels_)) {
    other.width_ = 0;
    other.height_ = 0;
  }

  Image& operator=(Image&& other) noexcept {
    if (this != &other) {
      width_ = other.width_;
      height_ = other.height_;
      pixels_ = static_cast<std::unique_ptr<u32[]>&&>(other.pixels_);
      other.width_ = 0;
      other.height_ = 0;
    }
    return *this;
  }

  u32 width() const { return width_; }
  u32 height() const { return height_; }
  u32 stride() const { return width_ * 4; }
  const u32* pixels() const { return pixels_.get(); }
  u32* pixels() { return pixels_.get(); }
  bool valid() const { return pixels_ != nullptr && width_ > 0 && height_ > 0; }

 private:
  u32 width_ = 0, height_ = 0;
  std::unique_ptr<u32[]> pixels_;
};

using ImageHandle = u32;
static constexpr ImageHandle kInvalidImage = 0;

}  // namespace vx::gfx

#endif  // VELOXA_GRAPHICS_IMAGE_H_
