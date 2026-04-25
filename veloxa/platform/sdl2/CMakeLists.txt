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

add_library(vx_platform_sdl2 STATIC
  sdl2_input_translate.cc
  sdl2_window_surface.cc
  sdl2_event_loop.cc
)

target_include_directories(vx_platform_sdl2 PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(vx_platform_sdl2
  PUBLIC vx_platform SDL2::SDL2
  PRIVATE vx_foundation
)

target_compile_features(vx_platform_sdl2 PUBLIC cxx_std_17)
