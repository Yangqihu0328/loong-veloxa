# SDL2 platform backend — opt-in via -DVX_PLATFORM_SDL2=ON.
#
# Dual-track SDL2 lookup: prefer modern CMake config (SDL2 >= 2.0.12), fall
# back to pkg-config for older distros. Either path produces a SDL2::SDL2
# imported target.

find_package(SDL2 CONFIG QUIET)
if(NOT TARGET SDL2::SDL2)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(VX_SDL2_PC REQUIRED IMPORTED_TARGET sdl2)
  add_library(SDL2::SDL2 ALIAS PkgConfig::VX_SDL2_PC)
  message(STATUS "SDL2 found via pkg-config (version ${VX_SDL2_PC_VERSION})")
else()
  message(STATUS "SDL2 found via CMake config (target SDL2::SDL2)")
endif()

# EGL + GLESv2 dependencies — only needed when the GLES renderer is
# selected, since Sdl2EGLDisplay (the only consumer in this target) is
# compiled-in unconditionally but its symbols are only linked through when
# downstream code instantiates it. We declare the dep at sdl2/ scope so
# software-only builds still pay the link cost (negligible) but no other
# subdirectory pulls EGL/GLES into its compile/link line. See
# cmake/VxRenderer.cmake for the VX_RENDERER flag plumbing.
find_package(PkgConfig REQUIRED)
pkg_check_modules(VX_EGL REQUIRED IMPORTED_TARGET egl)
pkg_check_modules(VX_GLESV2 REQUIRED IMPORTED_TARGET glesv2)

add_library(vx_platform_sdl2 STATIC
  sdl2_input_translate.cc
  sdl2_window_surface.cc
  sdl2_event_loop.cc
  sdl2_egl_display.cc
  sdl2_gl_window_surface.cc   # G1.3: GLES-backed SDL2 window surface
)

target_include_directories(vx_platform_sdl2 PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(vx_platform_sdl2
  PUBLIC vx_platform SDL2::SDL2
  PRIVATE vx_foundation PkgConfig::VX_EGL PkgConfig::VX_GLESV2
)

target_compile_features(vx_platform_sdl2 PUBLIC cxx_std_17)
