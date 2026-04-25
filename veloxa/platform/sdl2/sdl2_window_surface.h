#ifndef VELOXA_PLATFORM_SDL2_SDL2_WINDOW_SURFACE_H_
#define VELOXA_PLATFORM_SDL2_SDL2_WINDOW_SURFACE_H_

#include <SDL2/SDL.h>

#include "veloxa/platform/surface.h"

namespace vx::platform {

// Surface backed by an SDL2 window + renderer + streaming texture.
//
// Lifetime: ctor lazily SDL_InitSubSystem(VIDEO) (refcounted), creates a
// window/renderer/texture trio, and allocates a CPU-side pixel buffer that
// the rasterizer writes into. dtor tears down all SDL resources and decrements
// the SDL VIDEO refcount.
//
// If construction fails (driver issue, OOM, etc.), valid() returns false and
// all SDL handles remain nullptr. Lock() returns nullptr in that case.
class Sdl2WindowSurface : public Surface {
 public:
  Sdl2WindowSurface(vx::u32 width, vx::u32 height, const char* title);
  ~Sdl2WindowSurface() override;

  Sdl2WindowSurface(const Sdl2WindowSurface&) = delete;
  Sdl2WindowSurface& operator=(const Sdl2WindowSurface&) = delete;

  vx::u32 width() const override;
  vx::u32 height() const override;
  vx::u32 stride() const override;
  vx::u32* Lock() override;
  void Unlock() override;
  void Resize(vx::u32 width, vx::u32 height) override;
  vx::Status SavePPM(const char* path) const override;
  void Present() override;

  // SDL2-specific accessors used by Sdl2EventLoop and tests.
  bool valid() const { return window_ != nullptr; }
  SDL_Window* window() const { return window_; }
  uint32_t window_id() const;

 private:
  vx::u32 width_;
  vx::u32 height_;
  vx::u32* pixels_;
  bool locked_;
  SDL_Window* window_;
  SDL_Renderer* renderer_;
  SDL_Texture* texture_;
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_SDL2_SDL2_WINDOW_SURFACE_H_
