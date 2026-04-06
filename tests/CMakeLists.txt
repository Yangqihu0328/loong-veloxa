include(GoogleTest)

function(vx_add_test TEST_NAME TEST_SRC)
  add_executable(${TEST_NAME} ${TEST_SRC})
  target_link_libraries(${TEST_NAME} PRIVATE vx_foundation GTest::gtest_main)
  gtest_discover_tests(${TEST_NAME})
endfunction()

vx_add_test(types_test foundation/base/types_test.cc)
vx_add_test(assert_test foundation/base/assert_test.cc)
vx_add_test(span_test foundation/base/span_test.cc)
vx_add_test(status_test foundation/base/status_test.cc)
vx_add_test(malloc_allocator_test foundation/memory/malloc_allocator_test.cc)
vx_add_test(arena_allocator_test foundation/memory/arena_allocator_test.cc)
vx_add_test(pool_allocator_test foundation/memory/pool_allocator_test.cc)
vx_add_test(vector_test foundation/containers/vector_test.cc)
vx_add_test(small_vector_test foundation/containers/small_vector_test.cc)
vx_add_test(intrusive_list_test foundation/containers/intrusive_list_test.cc)
vx_add_test(hash_map_test foundation/containers/hash_map_test.cc)
vx_add_test(log_test foundation/log/log_test.cc)
vx_add_test(string_view_test foundation/strings/string_view_test.cc)
vx_add_test(utf_test foundation/strings/utf_test.cc)
vx_add_test(string_test foundation/strings/string_test.cc)
vx_add_test(interned_string_test foundation/strings/interned_string_test.cc)
vx_add_test(integration_test foundation/integration_test.cc)

vx_add_test(memory_surface_test platform/memory_surface_test.cc)
target_link_libraries(memory_surface_test PRIVATE vx_platform)

vx_add_test(event_loop_test platform/event_loop_test.cc)
target_link_libraries(event_loop_test PRIVATE vx_platform)

vx_add_test(gfx_types_test graphics/types_test.cc)
target_link_libraries(gfx_types_test PRIVATE vx_graphics)

vx_add_test(software_path_test graphics/software_path_test.cc)
target_link_libraries(software_path_test PRIVATE vx_graphics)

vx_add_test(software_canvas_test graphics/software_canvas_test.cc)
target_link_libraries(software_canvas_test PRIVATE vx_graphics)

vx_add_test(gfx_integration_test graphics/integration_test.cc)
target_link_libraries(gfx_integration_test PRIVATE vx_graphics vx_platform)

# Core: DOM + HTML
vx_add_test(tag_test core/dom/tag_test.cc)
target_link_libraries(tag_test PRIVATE vx_core)

vx_add_test(tokenizer_test core/html/tokenizer_test.cc)
target_link_libraries(tokenizer_test PRIVATE vx_core)

vx_add_test(node_test core/dom/node_test.cc)
target_link_libraries(node_test PRIVATE vx_core)

vx_add_test(element_test core/dom/element_test.cc)
target_link_libraries(element_test PRIVATE vx_core)

vx_add_test(document_test core/dom/document_test.cc)
target_link_libraries(document_test PRIVATE vx_core)

vx_add_test(parser_test core/html/parser_test.cc)
target_link_libraries(parser_test PRIVATE vx_core)

vx_add_test(html_integration_test core/html/integration_test.cc)
target_link_libraries(html_integration_test PRIVATE vx_core)

# Core: CSS
vx_add_test(css_property_test core/css/property_test.cc)
target_link_libraries(css_property_test PRIVATE vx_core)

vx_add_test(css_tokenizer_test core/css/tokenizer_test.cc)
target_link_libraries(css_tokenizer_test PRIVATE vx_core)

vx_add_test(css_parser_test core/css/parser_test.cc)
target_link_libraries(css_parser_test PRIVATE vx_core)

vx_add_test(css_selector_matcher_test core/css/selector_matcher_test.cc)
target_link_libraries(css_selector_matcher_test PRIVATE vx_core)

vx_add_test(css_style_resolver_test core/css/style_resolver_test.cc)
target_link_libraries(css_style_resolver_test PRIVATE vx_core)

vx_add_test(css_integration_test core/css/integration_test.cc)
target_link_libraries(css_integration_test PRIVATE vx_core)

# Core: Layout
vx_add_test(layout_box_test core/layout/layout_box_test.cc)
target_link_libraries(layout_box_test PRIVATE vx_core)

vx_add_test(layout_utils_test core/layout/layout_utils_test.cc)
target_link_libraries(layout_utils_test PRIVATE vx_core)

vx_add_test(tree_builder_test core/layout/tree_builder_test.cc)
target_link_libraries(tree_builder_test PRIVATE vx_core)

vx_add_test(block_layout_test core/layout/block_layout_test.cc)
target_link_libraries(block_layout_test PRIVATE vx_core)

vx_add_test(flex_layout_test core/layout/flex_layout_test.cc)
target_link_libraries(flex_layout_test PRIVATE vx_core)

vx_add_test(inline_position_test core/layout/inline_position_test.cc)
target_link_libraries(inline_position_test PRIVATE vx_core)

vx_add_test(layout_integration_test core/layout/integration_test.cc)
target_link_libraries(layout_integration_test PRIVATE vx_core)

# Core: Render
vx_add_test(render_paint_command_test core/render/paint_command_test.cc)
target_link_libraries(render_paint_command_test PRIVATE vx_core)

vx_add_test(render_test core/render/renderer_test.cc)
target_link_libraries(render_test PRIVATE vx_core vx_graphics vx_platform)

vx_add_test(render_integration_test core/render/integration_test.cc)
target_link_libraries(render_integration_test PRIVATE vx_core vx_graphics vx_platform)

# Core: Event
vx_add_test(event_types_test core/event/event_types_test.cc)
target_link_libraries(event_types_test PRIVATE vx_core)

vx_add_test(hit_test_test core/event/hit_test_test.cc)
target_link_libraries(hit_test_test PRIVATE vx_core)

vx_add_test(event_dispatcher_test core/event/event_dispatcher_test.cc)
target_link_libraries(event_dispatcher_test PRIVATE vx_core)

vx_add_test(event_manager_test core/event/event_manager_test.cc)
target_link_libraries(event_manager_test PRIVATE vx_core)

vx_add_test(event_integration_test core/event/integration_test.cc)
target_link_libraries(event_integration_test PRIVATE vx_core vx_graphics vx_platform)
