add_library(vx_core STATIC
  dom/tag.cc
  dom/node.cc
  dom/serializer.cc
  html/tokenizer.cc
  html/parser.cc
)

target_include_directories(vx_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_core PUBLIC vx_foundation)
