add_library(vx_graphics STATIC)

target_sources(vx_graphics PRIVATE
  software/rasterizer.cc
  software/software_path.cc
  software/software_canvas.cc
)

target_include_directories(vx_graphics PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(vx_graphics PUBLIC vx_foundation vx_platform)
target_compile_features(vx_graphics PUBLIC cxx_std_17)
