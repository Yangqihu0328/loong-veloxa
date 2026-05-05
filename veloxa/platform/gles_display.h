#ifndef VELOXA_PLATFORM_GLES_DISPLAY_H_
#define VELOXA_PLATFORM_GLES_DISPLAY_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"

namespace vx::platform {

// EGL display + GL context abstraction shared between G1 (desktop SDL2) and
// G2 (embedded DRM/KMS) backends. Subclasses own the platform-specific GL
// context lifecycle; callers interact only with this abstract interface.
//
// Lifecycle:
//   1. construct (no GL state created)
//   2. Initialize() -> creates the GL context, populates extension cache,
//      reads gles version
//   3. MakeCurrent() / DoneCurrent() any number of times
//   4. SwapBuffers() once per frame after rendering
//   5. RestoreContext() if IsContextLost() returns true (embedded GLES)
//   6. Shutdown() releases GL state; the same instance may be re-Initialize'd
//
// IsContextLost() surfaces GL_CONTEXT_LOST_KHR (KHR_robustness) — always
// false on desktop drivers but mandatory to handle on embedded GLES where
// suspend/resume can invalidate contexts. Subclasses must implement
// RestoreContext() so callers can rebuild the GL state without process
// restart.
//
// G2 (embedded) will add a GpuFence factory once the DRM/KMS blueprint
// lands; the placeholder comment below reserves the API shape.
class GLESDisplay {
 public:
  virtual ~GLESDisplay() = default;

  // Lifecycle. Initialize must succeed before any other GL method is called.
  virtual vx::Status Initialize() = 0;
  virtual void Shutdown() = 0;
  virtual bool IsValid() const = 0;

  // Context binding. Holding the context current is required before any GL
  // resource is created or used; subclasses may keep it implicitly current
  // after Initialize.
  virtual bool MakeCurrent() = 0;
  virtual void DoneCurrent() = 0;

  // Frame present (SDL_GL_SwapWindow on the SDL2 backend).
  virtual void SwapBuffers() = 0;

  // Embedded GLES robustness: detect / recover from GL_CONTEXT_LOST_KHR.
  virtual bool IsContextLost() const = 0;
  virtual vx::Status RestoreContext() = 0;

  // GL extension query. Subclasses are expected to populate an internal
  // cache during Initialize() so this is O(1) per call.
  virtual bool HasExtension(const char* name) const = 0;

  // Negotiated GLES version. Both default to 3.0 if Initialize succeeded
  // against the minimum-supported context.
  virtual vx::i32 gles_major_version() const = 0;
  virtual vx::i32 gles_minor_version() const = 0;

  // GpuFence factory (B8 G2 reservation):
  // virtual std::unique_ptr<GpuFence> CreateFence() = 0;
  //   Will be added once the DRM/KMS blueprint formalizes GpuFence; G2
  //   subclasses will need this for embedded sync semantics. Left out of
  //   the abstract surface in G1 so SDL2 doesn't have to stub it.
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_GLES_DISPLAY_H_
