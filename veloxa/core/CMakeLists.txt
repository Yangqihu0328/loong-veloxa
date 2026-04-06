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
  layout/text_shaper.cc
  layout/layout_utils.cc
  layout/layout_engine.cc
  layout/flex_layout.cc
  render/renderer.cc
  event/hit_test.cc
  event/event_dispatcher.cc
  event/event_manager.cc
)

target_include_directories(vx_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_core PUBLIC vx_foundation vx_graphics)
