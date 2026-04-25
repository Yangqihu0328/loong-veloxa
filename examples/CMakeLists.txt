add_executable(hello_veloxa hello.cc)
target_link_libraries(hello_veloxa PRIVATE vx_api)

if(VX_PLATFORM_SDL2)
  add_executable(hello_sdl2 hello_sdl2.cc)
  # vx_platform_sdl2 carries SDL2 headers + library transitively (PUBLIC).
  target_link_libraries(hello_sdl2 PRIVATE vx_api vx_platform_sdl2)
endif()
