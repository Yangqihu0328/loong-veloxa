#include "veloxa/platform/sdl2/sdl2_egl_display.h"

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <cstdio>
#include <string>

namespace vx::platform {

namespace {

// GL_CONTEXT_LOST_KHR is defined in <GLES2/gl2ext.h> as 0x0507. We pin the
// numeric constant here to keep IsContextLost portable across systems that
// only ship the core GLES headers.
constexpr GLenum kContextLostKhr = 0x0507;

}  // namespace

Sdl2EGLDisplay::Sdl2EGLDisplay(SDL_Window* window) : window_(window) {}

Sdl2EGLDisplay::~Sdl2EGLDisplay() { Shutdown(); }

vx::Status Sdl2EGLDisplay::Initialize() {
  if (window_ == nullptr) {
    return vx::Status(vx::StatusCode::kInvalidArgument,
                      "Sdl2EGLDisplay: window is null");
  }
  if (gl_context_ != nullptr) {
    // Already initialized — caller probably wants RestoreContext.
    return vx::Status::Ok();
  }

  // Request an OpenGL ES 3.0+ context. Stencil is required for the clip-mask
  // path (G1.5+); depth is unused for 2D rendering.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  gl_context_ = SDL_GL_CreateContext(window_);
  if (gl_context_ == nullptr) {
    return vx::Status(
        vx::StatusCode::kInternal,
        std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
  }

  if (SDL_GL_MakeCurrent(window_, gl_context_) != 0) {
    std::string err = SDL_GetError();
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
    return vx::Status(vx::StatusCode::kInternal,
                      std::string("SDL_GL_MakeCurrent failed: ") + err);
  }

  // Read back the negotiated GLES version from glGetString. Mesa returns
  // strings like "OpenGL ES 3.0 Mesa 24.x".
  const char* version_str =
      reinterpret_cast<const char*>(glGetString(GL_VERSION));
  if (version_str != nullptr) {
    int major = 3;
    int minor = 0;
    if (std::sscanf(version_str, "OpenGL ES %d.%d", &major, &minor) == 2) {
      gles_major_ = static_cast<vx::i32>(major);
      gles_minor_ = static_cast<vx::i32>(minor);
    }
  }

  // Eager-populate the extension cache (D3=B). For ES 3.0+ we use the
  // indexed glGetStringi path; the legacy glGetString(GL_EXTENSIONS) is
  // deprecated and may return a truncated string.
  ext_cache_.clear();
  GLint num_ext = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &num_ext);
  if (num_ext > 0) {
    ext_cache_.reserve(static_cast<size_t>(num_ext));
    for (GLint i = 0; i < num_ext; ++i) {
      const char* ext = reinterpret_cast<const char*>(
          glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
      if (ext != nullptr) {
        ext_cache_.emplace(ext);
      }
    }
  }

  context_lost_ = false;
  return vx::Status::Ok();
}

void Sdl2EGLDisplay::Shutdown() {
  if (gl_context_ != nullptr) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
  }
  ext_cache_.clear();
  context_lost_ = false;
}

bool Sdl2EGLDisplay::MakeCurrent() {
  if (gl_context_ == nullptr || window_ == nullptr) return false;
  return SDL_GL_MakeCurrent(window_, gl_context_) == 0;
}

void Sdl2EGLDisplay::DoneCurrent() {
  if (window_ != nullptr) {
    SDL_GL_MakeCurrent(window_, nullptr);
  }
}

void Sdl2EGLDisplay::SwapBuffers() {
  if (window_ != nullptr) {
    SDL_GL_SwapWindow(window_);
  }
}

bool Sdl2EGLDisplay::IsContextLost() const {
  if (context_lost_) return true;
  if (gl_context_ == nullptr) return false;

  // Drain glGetError; if the driver flags KHR_robustness context-loss the
  // next error in the queue will be GL_CONTEXT_LOST_KHR. Latch on first
  // sighting so callers don't miss the signal between SwapBuffers calls.
  GLenum err = glGetError();
  if (err == kContextLostKhr) {
    context_lost_ = true;
    return true;
  }
  return false;
}

vx::Status Sdl2EGLDisplay::RestoreContext() {
  // Tear down the existing context (if any) and re-run Initialize. The
  // window itself is preserved so the surface (and its size) stays valid
  // across the restore.
  Shutdown();
  return Initialize();
}

bool Sdl2EGLDisplay::HasExtension(const char* name) const {
  if (name == nullptr) return false;
  return ext_cache_.find(name) != ext_cache_.end();
}

}  // namespace vx::platform
