#include "veloxa/platform/sdl2/sdl2_window_surface.h"

#include <cstdio>
#include <cstring>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/log/log.h"
#include "veloxa/foundation/memory/malloc_allocator.h"

namespace vx::platform {

namespace {

// Allocate a zero-filled pixel buffer of width*height u32s, or nullptr if
// width or height is zero.
vx::u32* AllocPixels(vx::u32 width, vx::u32 height) {
  vx::usize total = static_cast<vx::usize>(width) * height;
  if (total == 0) return nullptr;
  auto* p = static_cast<vx::u32*>(
      vx::DefaultAllocator().Allocate(total * sizeof(vx::u32)));
  std::memset(p, 0, total * sizeof(vx::u32));
  return p;
}

void FreePixels(vx::u32* p, vx::u32 width, vx::u32 height) {
  if (!p) return;
  vx::usize total = static_cast<vx::usize>(width) * height;
  vx::DefaultAllocator().Deallocate(p, total * sizeof(vx::u32));
}

}  // namespace

Sdl2WindowSurface::Sdl2WindowSurface(vx::u32 width, vx::u32 height,
                                     const char* title)
    : width_(width),
      height_(height),
      pixels_(nullptr),
      locked_(false),
      window_(nullptr),
      renderer_(nullptr),
      texture_(nullptr) {
  if (width == 0 || height == 0) {
    VX_LOG_WARN("Sdl2WindowSurface: zero dimensions, skipping window creation");
    return;
  }

  // Refcounted. Subsequent calls just increment the refcount; the matching
  // SDL_QuitSubSystem in dtor only tears down when the count returns to 0.
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    VX_LOG_ERROR("Sdl2WindowSurface: SDL_InitSubSystem(VIDEO) failed: %s",
                 SDL_GetError());
    return;
  }

  window_ = SDL_CreateWindow(
      title ? title : "Veloxa",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      static_cast<int>(width), static_cast<int>(height),
      SDL_WINDOW_SHOWN);
  if (!window_) {
    VX_LOG_ERROR("Sdl2WindowSurface: SDL_CreateWindow failed: %s",
                 SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return;
  }

  renderer_ = SDL_CreateRenderer(window_, -1, 0);
  if (!renderer_) {
    VX_LOG_ERROR("Sdl2WindowSurface: SDL_CreateRenderer failed: %s",
                 SDL_GetError());
    SDL_DestroyWindow(window_);
    window_ = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return;
  }

  texture_ = SDL_CreateTexture(
      renderer_, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
      static_cast<int>(width), static_cast<int>(height));
  if (!texture_) {
    VX_LOG_ERROR("Sdl2WindowSurface: SDL_CreateTexture failed: %s",
                 SDL_GetError());
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    renderer_ = nullptr;
    window_ = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return;
  }

  pixels_ = AllocPixels(width, height);
}

Sdl2WindowSurface::~Sdl2WindowSurface() {
  FreePixels(pixels_, width_, height_);
  if (texture_)  SDL_DestroyTexture(texture_);
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) {
    SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
}

vx::u32 Sdl2WindowSurface::width() const  { return width_; }
vx::u32 Sdl2WindowSurface::height() const { return height_; }
vx::u32 Sdl2WindowSurface::stride() const { return width_ * sizeof(vx::u32); }

vx::u32* Sdl2WindowSurface::Lock() {
  VX_DCHECK(!locked_);
  locked_ = true;
  return pixels_;
}

void Sdl2WindowSurface::Unlock() {
  VX_DCHECK(locked_);
  locked_ = false;
}

void Sdl2WindowSurface::Resize(vx::u32 /*w*/, vx::u32 /*h*/) {
  VX_LOG_WARN("Sdl2WindowSurface::Resize not yet implemented");
}

vx::Status Sdl2WindowSurface::SavePPM(const char* /*path*/) const {
  return vx::Status(vx::StatusCode::kInternal,
                    "Sdl2WindowSurface::SavePPM not implemented");
}

void Sdl2WindowSurface::Present() {
  // Implemented in P2.2.
}

uint32_t Sdl2WindowSurface::window_id() const {
  return window_ ? SDL_GetWindowID(window_) : 0;
}

}  // namespace vx::platform
