find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)

add_library(vx_graphics STATIC)

target_sources(vx_graphics PRIVATE
  software/rasterizer.cc
  software/software_path.cc
  software/software_canvas.cc
)

target_include_directories(vx_graphics PUBLIC
  ${CMAKE_SOURCE_DIR}
)
target_include_directories(vx_graphics PRIVATE ${HARFBUZZ_INCLUDE_DIRS})

target_link_libraries(vx_graphics
  PUBLIC vx_foundation vx_platform
  PRIVATE vx_text Freetype::Freetype ${HARFBUZZ_LIBRARIES}
)
target_compile_features(vx_graphics PUBLIC cxx_std_17)
