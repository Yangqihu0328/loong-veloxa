#include "veloxa/platform/headless/memory_surface.h"

#include <cstdio>
#include <cstring>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/memory/malloc_allocator.h"

namespace vx::platform {

MemorySurface::MemorySurface(vx::u32 width, vx::u32 height)
    : width_(width), height_(height), pixels_(nullptr), locked_(false) {
  vx::usize total = static_cast<vx::usize>(width_) * height_;
  if (total > 0) {
    pixels_ = static_cast<vx::u32*>(
        vx::DefaultAllocator().Allocate(total * sizeof(vx::u32)));
    std::memset(pixels_, 0, total * sizeof(vx::u32));
  }
}

MemorySurface::~MemorySurface() {
  if (pixels_) {
    vx::usize total = static_cast<vx::usize>(width_) * height_;
    vx::DefaultAllocator().Deallocate(pixels_, total * sizeof(vx::u32));
  }
}

vx::u32 MemorySurface::width() const { return width_; }

vx::u32 MemorySurface::height() const { return height_; }

vx::u32 MemorySurface::stride() const { return width_ * sizeof(vx::u32); }

vx::u32* MemorySurface::Lock() {
  VX_DCHECK(!locked_);
  locked_ = true;
  return pixels_;
}

void MemorySurface::Unlock() {
  VX_DCHECK(locked_);
  locked_ = false;
}

void MemorySurface::Resize(vx::u32 width, vx::u32 height) {
  VX_DCHECK(!locked_);
  if (pixels_) {
    vx::usize old_total = static_cast<vx::usize>(width_) * height_;
    vx::DefaultAllocator().Deallocate(pixels_, old_total * sizeof(vx::u32));
    pixels_ = nullptr;
  }
  width_ = width;
  height_ = height;
  vx::usize total = static_cast<vx::usize>(width_) * height_;
  if (total > 0) {
    pixels_ = static_cast<vx::u32*>(
        vx::DefaultAllocator().Allocate(total * sizeof(vx::u32)));
    std::memset(pixels_, 0, total * sizeof(vx::u32));
  }
}

vx::Status MemorySurface::SavePPM(const char* path) const {
  std::FILE* f = std::fopen(path, "wb");
  if (!f) {
    return vx::Status(vx::StatusCode::kInternal, "Failed to open file");
  }

  std::fprintf(f, "P6\n%u %u\n255\n", width_, height_);

  vx::usize total = static_cast<vx::usize>(width_) * height_;
  for (vx::usize i = 0; i < total; ++i) {
    vx::u32 pixel = pixels_[i];
    vx::u8 rgb[3];
    rgb[0] = static_cast<vx::u8>(pixel & 0xFF);
    rgb[1] = static_cast<vx::u8>((pixel >> 8) & 0xFF);
    rgb[2] = static_cast<vx::u8>((pixel >> 16) & 0xFF);
    if (std::fwrite(rgb, 1, 3, f) != 3) {
      std::fclose(f);
      return vx::Status(vx::StatusCode::kInternal, "Failed to write pixel data");
    }
  }

  std::fclose(f);
  return vx::Status::Ok();
}

}  // namespace vx::platform
