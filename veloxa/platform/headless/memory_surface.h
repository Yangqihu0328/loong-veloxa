#ifndef VELOXA_PLATFORM_HEADLESS_MEMORY_SURFACE_H_
#define VELOXA_PLATFORM_HEADLESS_MEMORY_SURFACE_H_

#include "veloxa/platform/surface.h"

namespace vx::platform {

class MemorySurface : public Surface {
 public:
  MemorySurface(vx::u32 width, vx::u32 height);
  ~MemorySurface() override;

  vx::u32 width() const override;
  vx::u32 height() const override;
  vx::u32 stride() const override;
  vx::u32* Lock() override;
  void Unlock() override;
  void Resize(vx::u32 width, vx::u32 height) override;
  vx::Status SavePPM(const char* path) const override;

  const vx::u32* data() const { return pixels_; }

 private:
  vx::u32 width_;
  vx::u32 height_;
  vx::u32* pixels_;
  bool locked_;
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_HEADLESS_MEMORY_SURFACE_H_
