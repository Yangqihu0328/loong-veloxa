configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/veloxa/api/version.h
  @ONLY)

add_library(vx_api STATIC veloxa_api.cc)
target_include_directories(vx_api
  PUBLIC ${PROJECT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(vx_api PUBLIC vx_core vx_graphics vx_platform vx_script)

if(VX_PLATFORM_SDL2)
  target_compile_definitions(vx_api PUBLIC VX_PLATFORM_SDL2)
  target_link_libraries(vx_api PUBLIC vx_platform_sdl2)
endif()

# TASK-20260502-01 A.0.6: DevTool C API thin wrapper.
# Compiled in when VX_BUILD_DEVTOOL=ON; otherwise the wrapper returns
# VX_ERROR_INVALID_STATE via #ifdef stub (A14 acceptance — zero DevTool
# bytes in OFF-builds).
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(vx_api PUBLIC VX_BUILD_DEVTOOL)
  target_link_libraries(vx_api PRIVATE vx_devtool)
endif()
