#ifndef VELOXA_PLATFORM_SURFACE_H_
#define VELOXA_PLATFORM_SURFACE_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"

namespace vx::platform {

class Surface {
 public:
  virtual ~Surface() = default;
  virtual vx::u32 width() const = 0;
  virtual vx::u32 height() const = 0;
  virtual vx::u32 stride() const = 0;
  virtual vx::u32* Lock() = 0;
  virtual void Unlock() = 0;
  virtual void Resize(vx::u32 width, vx::u32 height) = 0;
  virtual vx::Status SavePPM(const char* path) const = 0;

  // Push the rendered pixel buffer to the screen if the backend supports it.
  // Default no-op for headless / file-only backends.
  // Contract: call exactly once per frame, after Lock/Unlock pair completes.
  virtual void Present() {}
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_SURFACE_H_
