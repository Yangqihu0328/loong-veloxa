add_executable(hello_veloxa hello.cc)
target_link_libraries(hello_veloxa PRIVATE vx_api)

if(VX_PLATFORM_SDL2)
  add_executable(hello_sdl2 hello_sdl2.cc)
  # vx_platform_sdl2 carries SDL2 headers + library transitively (PUBLIC).
  target_link_libraries(hello_sdl2 PRIVATE vx_api vx_platform_sdl2)
endif()

# TASK-20260502-01 A.3.1: hello_devtool example (SDL2 + DevTool dogfood).
# Requires both VX_PLATFORM_SDL2 (real window) and VX_BUILD_DEVTOOL (Inspector
# subsystem). Compiled out cleanly when either is OFF.
if(VX_PLATFORM_SDL2 AND VX_BUILD_DEVTOOL)
  add_executable(hello_devtool hello_devtool.cc)
  target_link_libraries(hello_devtool PRIVATE vx_api vx_platform_sdl2)
endif()
