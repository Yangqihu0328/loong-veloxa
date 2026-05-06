#include "veloxa/platform/sdl2/sdl2_gl_window_surface.h"

#include <GLES3/gl3.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/log/log.h"
#include "veloxa/platform/sdl2/sdl2_egl_display.h"

namespace vx::platform {

Sdl2GLWindowSurface::Sdl2GLWindowSurface(vx::u32 width, vx::u32 height,
                                         const char* title)
    : width_(width), height_(height) {
  if (width == 0 || height == 0) {
    VX_LOG_WARN(
        "Sdl2GLWindowSurface: zero dimensions (%ux%u), skipping construction",
        width, height);
    return;
  }

  // Refcounted by SDL. Subsequent calls just increment; the matching
  // SDL_QuitSubSystem in dtor only tears down when the count returns to 0.
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    VX_LOG_ERROR("Sdl2GLWindowSurface: SDL_InitSubSystem(VIDEO) failed: %s",
                 SDL_GetError());
    return;
  }

  // Reset any stale GL attributes from a previous surface (test isolation).
  SDL_GL_ResetAttributes();

  window_ = SDL_CreateWindow(
      title ? title : "Veloxa GL",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      static_cast<int>(width), static_cast<int>(height),
      SDL_WINDOW_OPENGL);
  if (window_ == nullptr) {
    VX_LOG_ERROR("Sdl2GLWindowSurface: SDL_CreateWindow failed: %s",
                 SDL_GetError());
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return;
  }

  auto display = std::make_unique<Sdl2EGLDisplay>(window_);
  vx::Status status = display->Initialize();
  if (!status.ok()) {
    VX_LOG_ERROR("Sdl2GLWindowSurface: EGL init failed: %s",
                 status.message().c_str());
    SDL_DestroyWindow(window_);
    window_ = nullptr;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return;
  }

  display_ = std::move(display);
}

Sdl2GLWindowSurface::~Sdl2GLWindowSurface() {
  // Drop the display (and its GL context) BEFORE destroying the SDL_Window.
  // Destroying the window first would leave the GLContext attached to a dead
  // handle, causing undefined behaviour on some drivers.
  display_.reset();

  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
}

void Sdl2GLWindowSurface::Resize(vx::u32 width, vx::u32 height) {
  width_ = width;
  height_ = height;
  if (window_ != nullptr) {
    SDL_SetWindowSize(window_, static_cast<int>(width),
                      static_cast<int>(height));
  }
}

vx::Status Sdl2GLWindowSurface::SavePPM(const char* path) const {
  if (path == nullptr) {
    return vx::Status(vx::StatusCode::kInvalidArgument,
                      "Sdl2GLWindowSurface::SavePPM: path is null");
  }
  if (!valid()) {
    return vx::Status(vx::StatusCode::kInternal,
                      "Sdl2GLWindowSurface::SavePPM: surface not valid");
  }

  if (!display_->MakeCurrent()) {
    return vx::Status(vx::StatusCode::kInternal,
                      "Sdl2GLWindowSurface::SavePPM: MakeCurrent failed");
  }

  // Read RGBA pixels from the default framebuffer. glReadPixels uses
  // bottom-left origin, so we Y-flip the rows below before writing PPM.
  const vx::u32 w = width_;
  const vx::u32 h = height_;
  std::vector<uint8_t> buf(static_cast<size_t>(w) * h * 4);
  glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
               GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

  // Y-flip: GL y=0 is bottom; PPM y=0 is top.
  const size_t row_bytes = static_cast<size_t>(w) * 4;
  std::vector<uint8_t> tmp(row_bytes);
  for (vx::u32 top = 0, bot = h - 1; top < bot; ++top, --bot) {
    uint8_t* row_top = buf.data() + top * row_bytes;
    uint8_t* row_bot = buf.data() + bot * row_bytes;
    std::copy(row_top, row_top + row_bytes, tmp.data());
    std::copy(row_bot, row_bot + row_bytes, row_top);
    std::copy(tmp.begin(), tmp.end(), row_bot);
  }

  // Write P6 PPM (binary RGB, no alpha channel).
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    return vx::Status(vx::StatusCode::kInternal,
                      std::string("Sdl2GLWindowSurface::SavePPM: cannot open "
                                  "output file: ") +
                          path);
  }

  out << "P6\n" << w << " " << h << "\n255\n";
  for (vx::u32 y = 0; y < h; ++y) {
    for (vx::u32 x = 0; x < w; ++x) {
      const uint8_t* px = buf.data() + (y * w + x) * 4;
      out.put(static_cast<char>(px[0]));  // R
      out.put(static_cast<char>(px[1]));  // G
      out.put(static_cast<char>(px[2]));  // B
      // Alpha channel dropped — PPM is RGB-only.
    }
  }

  if (!out.good()) {
    return vx::Status(vx::StatusCode::kInternal,
                      "Sdl2GLWindowSurface::SavePPM: write error");
  }

  return vx::Status::Ok();
}

void Sdl2GLWindowSurface::Present() {
  if (display_ != nullptr) {
    display_->SwapBuffers();
  }
}

}  // namespace vx::platform
