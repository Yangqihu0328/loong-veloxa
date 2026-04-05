add_library(vx_foundation STATIC)

target_sources(vx_foundation PRIVATE
  memory/malloc_allocator.cc
  memory/arena_allocator.cc
  memory/pool_allocator.cc
  strings/utf.cc
  strings/string.cc
  strings/interned_string.cc
  log/log.cc
)

target_include_directories(vx_foundation PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_compile_features(vx_foundation PUBLIC cxx_std_17)
