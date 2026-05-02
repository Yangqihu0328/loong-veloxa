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

vx_add_test(pixel_blend_test graphics/pixel_blend_test.cc)
target_link_libraries(pixel_blend_test PRIVATE vx_foundation)

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

vx_add_test(dom_serializer_test core/dom/serializer_test.cc)
target_link_libraries(dom_serializer_test PRIVATE vx_core)

vx_add_test(parser_test core/html/parser_test.cc)
target_link_libraries(parser_test PRIVATE vx_core)

vx_add_test(inline_style_test core/html/inline_style_test.cc)
target_link_libraries(inline_style_test PRIVATE vx_core)

vx_add_test(html_integration_test core/html/integration_test.cc)
target_link_libraries(html_integration_test PRIVATE vx_core)

# Core: CSS
vx_add_test(css_property_test core/css/property_test.cc)
target_link_libraries(css_property_test PRIVATE vx_core)

vx_add_test(css_enum_serialization_test core/css/enum_serialization_test.cc)
target_link_libraries(css_enum_serialization_test PRIVATE vx_core)

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

# Core: CSS Transitions
vx_add_test(css_transition_test core/css/transition_test.cc)
target_link_libraries(css_transition_test PRIVATE vx_core)

vx_add_test(css_transition_parse_test core/css/transition_parse_test.cc)
target_link_libraries(css_transition_parse_test PRIVATE vx_core)

vx_add_test(transition_integration_test core/transition_integration_test.cc)
target_link_libraries(transition_integration_test PRIVATE vx_core vx_graphics vx_platform)

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

vx_add_test(margin_collapse_test core/layout/margin_collapse_test.cc)
target_link_libraries(margin_collapse_test PRIVATE vx_core)

vx_add_test(text_shaper_test core/layout/text_shaper_test.cc)
target_link_libraries(text_shaper_test PRIVATE vx_core)

vx_add_test(line_box_test core/layout/line_box_test.cc)
target_link_libraries(line_box_test PRIVATE vx_core)

vx_add_test(wpt_layout_test integration/wpt_layout_test.cc)
target_link_libraries(wpt_layout_test PRIVATE vx_core)

vx_add_test(layout_integration_test core/layout/integration_test.cc)
target_link_libraries(layout_integration_test PRIVATE vx_core)

# Core: Render
vx_add_test(render_paint_command_test core/render/paint_command_test.cc)
target_link_libraries(render_paint_command_test PRIVATE vx_core)

vx_add_test(render_test core/render/renderer_test.cc)
target_link_libraries(render_test PRIVATE vx_core vx_graphics vx_platform)

vx_add_test(render_integration_test core/render/integration_test.cc)
target_link_libraries(render_integration_test PRIVATE vx_core vx_graphics vx_platform)

vx_add_test(border_radius_test core/render/border_radius_test.cc)
target_link_libraries(border_radius_test PRIVATE vx_core vx_graphics vx_platform)

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

# Core: Update Manager
vx_add_test(update_manager_test core/update_manager_test.cc)
target_link_libraries(update_manager_test PRIVATE vx_core vx_graphics vx_platform)

vx_add_test(update_integration_test core/update_integration_test.cc)
target_link_libraries(update_integration_test PRIVATE vx_core vx_graphics vx_platform)

# Core: Application
vx_add_test(application_test core/application_test.cc)
target_link_libraries(application_test PRIVATE vx_core vx_graphics vx_platform vx_text vx_script)

vx_add_test(application_integration_test core/application_integration_test.cc)
target_link_libraries(application_integration_test PRIVATE vx_core vx_graphics vx_platform vx_script)

# API: C API
add_executable(api_test api/api_test.cc)
target_link_libraries(api_test PRIVATE vx_foundation vx_api vx_platform GTest::gtest_main)
# SDL_VIDEODRIVER=dummy is harmless when VX_PLATFORM_SDL2=OFF (SDL2 cases
# are #ifdef'd out) and lets the SDL2 cases run on headless CI.
gtest_discover_tests(api_test PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

vx_add_test(api_integration_test api/api_integration_test.cc)
target_link_libraries(api_integration_test PRIVATE vx_api vx_platform)

# TASK-20260502-01 A.0.6: DevTool C API thin wrapper tests. The test source
# uses #ifdef VX_BUILD_DEVTOOL to switch between real-behavior assertions
# (T7 max_size, double-call, T3 redaction) and the A14 stub assertion
# (INVALID_STATE). Always built; always linked; behavior diverges by macro.
vx_add_test(devtool_api_test api/devtool_api_test.cc)
target_link_libraries(devtool_api_test PRIVATE vx_api vx_platform)
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(devtool_api_test PRIVATE VX_BUILD_DEVTOOL)
endif()

vx_add_test(quickjs_engine_test script/quickjs_engine_test.cc)
target_link_libraries(quickjs_engine_test PRIVATE vx_script)

vx_add_test(dom_bindings_test script/dom_bindings_test.cc)
target_link_libraries(dom_bindings_test PRIVATE vx_script vx_core)

# TASK-20260502-01 A.1.4: DevTool JS native binding test.
# Both ON and OFF builds compile this test; OFF path validates the
# zero-byte stub guard via the typeof === "undefined" branch.
vx_add_test(devtool_bindings_test script/devtool_bindings_test.cc)
target_link_libraries(devtool_bindings_test PRIVATE vx_script vx_core)
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(devtool_bindings_test PRIVATE VX_BUILD_DEVTOOL)
endif()

# Core: Image
find_package(PNG REQUIRED)

vx_add_test(image_decoder_test core/image/image_decoder_test.cc)
target_link_libraries(image_decoder_test PRIVATE vx_core PNG::PNG)

vx_add_test(image_cache_test core/image/image_cache_test.cc)
target_link_libraries(image_cache_test PRIVATE vx_core PNG::PNG)

vx_add_test(font_manager_test text/font_manager_test.cc)
target_link_libraries(font_manager_test PRIVATE vx_text)

vx_add_test(glyph_cache_test text/glyph_cache_test.cc)
target_link_libraries(glyph_cache_test PRIVATE vx_text)

vx_add_test(freetype_shaper_test text/freetype_shaper_test.cc)
target_link_libraries(freetype_shaper_test PRIVATE vx_text)

# TASK-20260424-04: ShapeCache (P1 HashBytesU64 + P2 cache contract)
vx_add_test(shape_cache_test text/shape_cache_test.cc)
target_link_libraries(shape_cache_test PRIVATE vx_text)

# TASK-20260424-04: DrawText hb_shape cache end-to-end (P3)
vx_add_test(drawtext_shape_cache_test graphics/drawtext_shape_cache_test.cc)
target_link_libraries(drawtext_shape_cache_test PRIVATE vx_graphics vx_text)

# TASK-20260502-01: DevTool subsystem (opt-in, requires VX_BUILD_DEVTOOL).
# Mirrors veloxa/devtool/ subtree; A14 acceptance ensures DevTool-OFF baseline
# (1085 tests) is unchanged.
if(VX_BUILD_DEVTOOL)
  add_subdirectory(devtool)
endif()

# TASK-20260425-01: SDL2 platform backend (opt-in, requires VX_PLATFORM_SDL2)
if(VX_PLATFORM_SDL2)
  add_executable(sdl2_input_translate_test platform/sdl2_input_translate_test.cc)
  target_link_libraries(sdl2_input_translate_test
    PRIVATE vx_foundation vx_platform_sdl2 GTest::gtest_main)
  # SDL_VIDEODRIVER=dummy ensures these tests can run on headless CI without
  # an actual display server.
  gtest_discover_tests(sdl2_input_translate_test
    PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  add_executable(sdl2_window_surface_test platform/sdl2_window_surface_test.cc)
  target_link_libraries(sdl2_window_surface_test
    PRIVATE vx_foundation vx_platform_sdl2 GTest::gtest_main)
  gtest_discover_tests(sdl2_window_surface_test
    PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  add_executable(sdl2_event_loop_test platform/sdl2_event_loop_test.cc)
  target_link_libraries(sdl2_event_loop_test
    PRIVATE vx_foundation vx_platform_sdl2 GTest::gtest_main)
  gtest_discover_tests(sdl2_event_loop_test
    PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

  # Smoke test: drive the hello_sdl2 example end-to-end under dummy driver
  # with auto-quit so it self-terminates. Exercises the full create / load /
  # run / quit / destroy path against a real SDL2 backend.
  add_test(
    NAME hello_sdl2_smoke
    COMMAND $<TARGET_FILE:hello_sdl2>
  )
  set_tests_properties(hello_sdl2_smoke PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;VX_HELLO_SDL2_AUTOQUIT_MS=200"
    TIMEOUT 10
  )
endif()
