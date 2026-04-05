add_library(vx_platform STATIC)

target_sources(vx_platform PRIVATE
  headless/memory_surface.cc
  headless/headless_event_loop.cc
)

target_include_directories(vx_platform PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_link_libraries(vx_platform PUBLIC vx_foundation)
target_compile_features(vx_platform PUBLIC cxx_std_17)
