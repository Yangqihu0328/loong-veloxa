add_library(vx_api STATIC veloxa_api.cc)
target_include_directories(vx_api PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_api PUBLIC vx_core vx_graphics vx_platform vx_script)

if(VX_PLATFORM_SDL2)
  target_compile_definitions(vx_api PUBLIC VX_PLATFORM_SDL2)
  target_link_libraries(vx_api PUBLIC vx_platform_sdl2)
endif()
