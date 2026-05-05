#ifndef VELOXA_PLATFORM_SDL2_SDL2_EGL_DISPLAY_H_
#define VELOXA_PLATFORM_SDL2_SDL2_EGL_DISPLAY_H_

#include <SDL2/SDL.h>

#include <string>
#include <unordered_set>

#include "veloxa/platform/gles_display.h"

namespace vx::platform {

// SDL2-backed implementation of GLESDisplay.
//
// Owns the SDL_GLContext bound to the supplied SDL_Window (which itself must
// have been created with SDL_WINDOW_OPENGL). The SDL_Window is borrowed —
// the caller (typically Sdl2GLWindowSurface, G1.3) retains ownership.
//
// Initialize() requests a GLES 3.0+ ES-profile context with a stencil
// buffer (clip-mask path) and double-buffering (frame present). The actual
// negotiated version is read back via glGetString(GL_VERSION) and exposed
// through gles_major_version() / gles_minor_version().
//
// HasExtension() is O(1) thanks to an unordered_set populated on
// Initialize() via glGetStringi(GL_EXTENSIONS, i).
//
// IsContextLost() honours an internal latch so production code can mark the
// context lost in response to a KHR_robustness reset notification; the
// latch is cleared on RestoreContext(). RestoreContext() tears down the old
// SDL_GLContext and re-runs Initialize.
class Sdl2EGLDisplay final : public GLESDisplay {
 public:
  explicit Sdl2EGLDisplay(SDL_Window* window);
  ~Sdl2EGLDisplay() override;

  Sdl2EGLDisplay(const Sdl2EGLDisplay&) = delete;
  Sdl2EGLDisplay& operator=(const Sdl2EGLDisplay&) = delete;

  vx::Status Initialize() override;
  void Shutdown() override;
  bool IsValid() const override { return gl_context_ != nullptr; }

  bool MakeCurrent() override;
  void DoneCurrent() override;
  void SwapBuffers() override;

  bool IsContextLost() const override;
  vx::Status RestoreContext() override;

  bool HasExtension(const char* name) const override;
  vx::i32 gles_major_version() const override { return gles_major_; }
  vx::i32 gles_minor_version() const override { return gles_minor_; }

 private:
  SDL_Window* window_ = nullptr;
  SDL_GLContext gl_context_ = nullptr;
  mutable bool context_lost_ = false;
  vx::i32 gles_major_ = 3;
  vx::i32 gles_minor_ = 0;
  std::unordered_set<std::string> ext_cache_;
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_SDL2_SDL2_EGL_DISPLAY_H_
