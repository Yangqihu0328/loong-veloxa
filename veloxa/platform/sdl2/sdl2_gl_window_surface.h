#ifndef VELOXA_PLATFORM_SDL2_SDL2_GL_WINDOW_SURFACE_H_
#define VELOXA_PLATFORM_SDL2_SDL2_GL_WINDOW_SURFACE_H_

#include <SDL2/SDL.h>

#include <memory>

#include "veloxa/platform/surface.h"

namespace vx::platform {

class Sdl2EGLDisplay;  // forward — keeps EGL/GLES off the public header

// Surface backed by an SDL2 window with an OpenGL ES context (G1.3).
//
// Ownership:
//   * SDL_Window — owned; created with SDL_WINDOW_OPENGL in ctor, destroyed
//     in dtor. SDL_InitSubSystem(VIDEO) is refcounted (mirrors the software
//     Sdl2WindowSurface path).
//   * Sdl2EGLDisplay — owned via std::unique_ptr; the display borrows the
//     SDL_Window we own. Destruction ordering: drop the display first (which
//     deletes the GLContext while the window is still alive), then destroy
//     the SDL_Window. Doing it the other way would leak the context on top of
//     a dead window handle.
//
// Surface contract notes (GLES path):
//   * Lock() returns nullptr / Unlock() is a no-op — rasterisation happens
//     GPU-side via GLESCanvas (G1.4+); there is no CPU pixel buffer.
//   * stride() returns 0 for the same reason.
//   * SavePPM() uses glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE) + Y-flip + P6
//     RGB output. Mesa swrast under SDL_VIDEODRIVER=offscreen may or may not
//     render to the default framebuffer; the test (T4) handles this with a
//     driver-strictness GTEST_SKIP.
//   * Present() defers to Sdl2EGLDisplay::SwapBuffers() (SDL_GL_SwapWindow).
//
// If construction fails (zero dimensions, SDL error, EGL context refused),
// valid() returns false and all owned handles remain nullptr; the destructor
// is safe in that state.
class Sdl2GLWindowSurface : public Surface {
 public:
  Sdl2GLWindowSurface(vx::u32 width, vx::u32 height, const char* title);
  ~Sdl2GLWindowSurface() override;

  Sdl2GLWindowSurface(const Sdl2GLWindowSurface&) = delete;
  Sdl2GLWindowSurface& operator=(const Sdl2GLWindowSurface&) = delete;

  vx::u32 width() const override { return width_; }
  vx::u32 height() const override { return height_; }
  vx::u32 stride() const override { return 0; }   // GLES path: no CPU stride
  vx::u32* Lock() override { return nullptr; }     // GLES path: no-op
  void Unlock() override {}                        // GLES path: no-op

  void Resize(vx::u32 width, vx::u32 height) override;
  vx::Status SavePPM(const char* path) const override;
  void Present() override;

  // GLES-specific accessors used by Application (G1.13) and tests.
  bool valid() const { return window_ != nullptr && display_ != nullptr; }
  SDL_Window* window() const { return window_; }
  Sdl2EGLDisplay* gles_display() const { return display_.get(); }

 private:
  vx::u32 width_;
  vx::u32 height_;
  SDL_Window* window_ = nullptr;
  std::unique_ptr<Sdl2EGLDisplay> display_;
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_SDL2_SDL2_GL_WINDOW_SURFACE_H_
