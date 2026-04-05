add_library(vx_core STATIC
  dom/tag.cc
  dom/node.cc
  dom/element.cc
  dom/serializer.cc
  html/tokenizer.cc
  html/parser.cc
  css/property.cc
  css/tokenizer.cc
  css/parser.cc
  css/selector_matcher.cc
  css/style_resolver.cc
)

target_include_directories(vx_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_core PUBLIC vx_foundation)
