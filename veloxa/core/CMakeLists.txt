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

# TASK-20260502-01 A.1.6: when DevTool is enabled, Application::LoadDevtoolDocument
# reads the embedded inspector_panel.{html,css,js} resources from
# vx_devtool_resources. Conditional link + compile-define keeps the OFF
# binary untouched (A14 zero-byte guard).
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(vx_core PUBLIC VX_BUILD_DEVTOOL)
  target_link_libraries(vx_core PRIVATE vx_devtool_resources)
endif()
