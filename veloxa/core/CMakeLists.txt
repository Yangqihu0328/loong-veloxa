find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

add_library(vx_core STATIC
  dom/tag.cc
  dom/node.cc
  dom/element.cc
  dom/serializer.cc
  html/tokenizer.cc
  html/parser.cc
  css/property.cc
  css/enum_serialization.cc
  css/tokenizer.cc
  css/parser.cc
  css/selector_matcher.cc
  css/style_resolver.cc
  css/transition.cc
  layout/text_shaper.cc
  layout/layout_utils.cc
  layout/layout_engine.cc
  layout/layout_box.cc
  layout/flex_layout.cc
  render/renderer.cc
  image/image_decoder.cc
  image/image_cache.cc
  event/hit_test.cc
  event/event_dispatcher.cc
  event/event_manager.cc
  update_manager.cc
  application.cc
)

target_include_directories(vx_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(vx_core PUBLIC vx_foundation vx_graphics PRIVATE vx_text vx_script PNG::PNG JPEG::JPEG)
