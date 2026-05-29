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

# TASK-20260502-01 A.1.6: Application dual UpdateManager + LoadDevtoolDocument.
# Compiled only when DEVTOOL=ON (these tests exercise LoadDevtoolDocument
# which lives behind #ifdef VX_BUILD_DEVTOOL — A14 zero-byte guard).
if(VX_BUILD_DEVTOOL)
  vx_add_test(application_devtool_test core/application_devtool_test.cc)
  target_link_libraries(application_devtool_test
    PRIVATE vx_core vx_graphics vx_platform vx_text vx_script vx_devtool)
  target_compile_definitions(application_devtool_test PRIVATE VX_BUILD_DEVTOOL)
endif()

# TASK-20260502-01 A.1.7: vx_view_attach_devtool / detach / devtool_loaded
# C API wrapper. Compiled in BOTH ON and OFF builds; OFF path verifies
# the VX_ERROR_INVALID_STATE stub guard via the #else branch test.
vx_add_test(devtool_attach_api_test api/devtool_attach_api_test.cc)
target_link_libraries(devtool_attach_api_test
  PRIVATE vx_foundation vx_api vx_platform)
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(devtool_attach_api_test PRIVATE VX_BUILD_DEVTOOL)
endif()

# TASK-20260502-01 A.2.1 [SECURITY]: vx_inspector_set_redaction_policy
# T3 mitigation policy switch unit tests. ON path covers default-redact /
# NONE-emits / round-trip / invalid-enum; OFF path covers stub guard.
# TASK-20260502-02 B.0.1: vx_view_set_pipeline_hooks C ABI test.
# Public ABI is intentionally available regardless of VX_BUILD_DEVTOOL —
# profiler / tracing embedders may install pipeline observers without
# DevTool. Compiled in BOTH ON and OFF builds.
vx_add_test(perf_hooks_api_test api/perf_hooks_api_test.cc)
target_link_libraries(perf_hooks_api_test
  PRIVATE vx_foundation vx_api vx_platform)

# TASK-20260505-02 A.1 — vx_view_invalidate public C ABI test (B-G4 closure).
# Compiled in BOTH ON and OFF builds (public ABI is independent of DevTool).
vx_add_test(invalidate_api_test api/invalidate_api_test.cc)
target_link_libraries(invalidate_api_test
  PRIVATE vx_foundation vx_api vx_platform)

vx_add_test(devtool_redaction_policy_test api/devtool_redaction_policy_test.cc)
target_link_libraries(devtool_redaction_policy_test
  PRIVATE vx_foundation vx_api vx_platform)
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(devtool_redaction_policy_test PRIVATE VX_BUILD_DEVTOOL)
endif()

# TASK-20260502-01 A.2.4 [A14]: link-closure smoke for the DevTool
# subsystem. Runs nm against libvx_api.a + libvx_core.a and asserts
# OFF builds contain ZERO DevTool internal symbols (RegisterDevtoolBindings,
# kInspectorPanel*, InspectorOverlay, InjectHoverHighlight, InputDispatchSplitter,
# SerializeDocument). ON builds verify the public C ABI stub is present.
# Pure CMake script test — no compiled artifact, runs every ctest.
if(VX_BUILD_DEVTOOL)
  set(_devtool_a14_flag "ON")
else()
  set(_devtool_a14_flag "OFF")
endif()
add_test(NAME devtool_a14_link_closure_smoke
  COMMAND ${CMAKE_COMMAND}
    -DLIB=$<TARGET_FILE:vx_api>
    -DCORELIB=$<TARGET_FILE:vx_core>
    -DDEVTOOL_ON=${_devtool_a14_flag}
    -P ${CMAKE_SOURCE_DIR}/tests/smoke/devtool_a14_link_closure.cmake)

# TASK-20260505-05 G1.1: VX_RENDERER flag validation smoke.
# Validates cmake/VxRenderer.cmake against 4 scenarios in stub-probe sub-configs
# (default + explicit software + explicit gles + invalid-rejected). Stub probe
# is a 3-line CMakeLists.txt that only includes VxRenderer.cmake, so each
# sub-config is sub-second; total smoke runtime ~1-2s. Also enforces the
# top-level CMakeLists.txt actually includes VxRenderer.cmake (drift guard).
add_test(NAME vx_renderer_flag_check_smoke
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DBUILD_DIR_BASE=${CMAKE_BINARY_DIR}
    -P ${CMAKE_SOURCE_DIR}/tests/smoke/vx_renderer_flag_check.cmake)

# TASK-20260502-01 A.1.8: dogfood headless smoke — attach DevTool over
# a real C-API view path, drive one Update frame, and assert that
# inspector_panel.js executes without error AND that
# vx_devtool_get_dom_json reaches the target Document. Compiled only
# when DEVTOOL=ON (entire scenario presupposes the subsystem).
if(VX_BUILD_DEVTOOL)
  vx_add_test(devtool_dogfood_smoke_test integration/devtool_dogfood_smoke_test.cc)
  target_link_libraries(devtool_dogfood_smoke_test
    PRIVATE vx_foundation vx_api vx_core vx_platform vx_script vx_devtool)
  target_compile_definitions(devtool_dogfood_smoke_test PRIVATE VX_BUILD_DEVTOOL)

  # TASK-20260503-04 D.5: end-to-end Console JS REPL smoke. Mirrors
  # devtool_dogfood_smoke_test target shape so the link closure invariant
  # (vx_devtool only when DEVTOOL=ON) is preserved at the integration
  # smoke layer too.
  vx_add_test(devtool_console_dogfood_smoke_test
              integration/devtool_console_dogfood_smoke_test.cc)
  target_link_libraries(devtool_console_dogfood_smoke_test
    PRIVATE vx_foundation vx_api vx_core vx_platform vx_script vx_devtool)
  target_compile_definitions(devtool_console_dogfood_smoke_test
                             PRIVATE VX_BUILD_DEVTOOL)
endif()

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

# TASK-20260503-04 D.4: Console JS REPL public C ABI tests
# (vx_view_eval_console + vx_view_console_log_drain). Mirrors
# devtool_api_test's #ifdef-driven dual-path strategy: ON path exercises
# the real engine via vx_view_attach_devtool + lazy-attach reverse probe;
# OFF path validates the A14 stub guard returns INVALID_STATE without
# touching DevTool symbols.
vx_add_test(console_api_test api/console_api_test.cc)
target_link_libraries(console_api_test PRIVATE vx_api vx_platform)
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(console_api_test PRIVATE VX_BUILD_DEVTOOL)
endif()

vx_add_test(quickjs_engine_test script/quickjs_engine_test.cc)
target_link_libraries(quickjs_engine_test PRIVATE vx_script)

vx_add_test(dom_bindings_test script/dom_bindings_test.cc)
target_link_libraries(dom_bindings_test PRIVATE vx_script vx_core)

# TASK-20260502-01 A.1.4 / TASK-20260502-02 B.2.2: DevTool JS native
# binding test (vx_devtool_get_dom_json + vx_view_get_perf_stats).
# Both ON and OFF builds compile this test; OFF path validates the
# zero-byte stub guard via the typeof === "undefined" branch.
# B.2.2 ON path additionally links vx_devtool (PerfOverlay symbols
# referenced from the new VxViewGetPerfStats tests).
vx_add_test(devtool_bindings_test script/devtool_bindings_test.cc)
target_link_libraries(devtool_bindings_test PRIVATE vx_script vx_core)
if(VX_BUILD_DEVTOOL)
  target_compile_definitions(devtool_bindings_test PRIVATE VX_BUILD_DEVTOOL)
  target_link_libraries(devtool_bindings_test PRIVATE vx_devtool)
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

  # Sdl2EGLDisplay (G1.2): only built when the GLES renderer backend is
  # selected, since the underlying class depends on EGL/GLESv2. The
  # SDL_VIDEODRIVER=offscreen driver routes GL through Mesa swrast EGL so
  # SDL_GL_CreateContext can attach a real GLES 3.0+ context on headless CI.
  if(VX_RENDERER STREQUAL "gles")
    add_executable(sdl2_egl_display_test platform/sdl2_egl_display_test.cc)
    target_link_libraries(sdl2_egl_display_test
      PRIVATE vx_foundation vx_platform_sdl2 GTest::gtest_main)
    gtest_discover_tests(sdl2_egl_display_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # Sdl2GLWindowSurface (G1.3): SDL2 window surface backed by an EGL/GLES
    # context. Shares the offscreen driver env so Mesa swrast can bind a real
    # GLES 3.0+ context on headless CI (same rationale as sdl2_egl_display_test).
    add_executable(sdl2_gl_window_surface_test
                   platform/sdl2_gl_window_surface_test.cc)
    target_link_libraries(sdl2_gl_window_surface_test
      PRIVATE vx_foundation vx_platform_sdl2 GTest::gtest_main)
    gtest_discover_tests(sdl2_gl_window_surface_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # GLESCanvas skeleton (G1.4): canvas-side counterpart to
    # Sdl2GLWindowSurface. Requires vx_platform_sdl2 (for the surface) AND
    # vx_graphics (for GLESCanvas itself, which is gles-config-only via
    # veloxa/graphics/CMakeLists.txt's if-guard).
    add_executable(gles_canvas_skeleton_test
                   graphics/gles/gles_canvas_skeleton_test.cc)
    target_link_libraries(gles_canvas_skeleton_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics GTest::gtest_main)
    gtest_discover_tests(gles_canvas_skeleton_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # Shader injection defense (G1.4 first-evidence security regression).
    # Pure header-only test (compile-time literal verification), no GL
    # context needed — but still gles-guarded because shaders.h only ships
    # in gles builds.
    add_executable(shader_injection_test
                   graphics/gles/shader_injection_test.cc)
    target_link_libraries(shader_injection_test
      PRIVATE vx_foundation vx_graphics GTest::gtest_main)
    gtest_discover_tests(shader_injection_test)

    # GLESCanvas FillRect + FillRoundedRect (G1.5): pixel-level verification
    # of the first real draw methods. Same gles guard + SDL_VIDEODRIVER=offscreen
    # env as gles_canvas_skeleton_test.
    add_executable(gles_canvas_fill_test
                   graphics/gles/gles_canvas_fill_test.cc)
    target_link_libraries(gles_canvas_fill_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics GTest::gtest_main)
    gtest_discover_tests(gles_canvas_fill_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # GLESCanvas FillPath via libtess2 (G1.6).
    add_executable(gles_canvas_path_test
                   graphics/gles/gles_canvas_path_test.cc)
    target_link_libraries(gles_canvas_path_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics GTest::gtest_main)
    gtest_discover_tests(gles_canvas_path_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # GLESCanvas Stroke* via Fill conversion (G1.7).
    add_executable(gles_canvas_stroke_test
                   graphics/gles/gles_canvas_stroke_test.cc)
    target_link_libraries(gles_canvas_stroke_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics GTest::gtest_main)
    gtest_discover_tests(gles_canvas_stroke_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # GlyphAtlas GL_R8 atlas + FT rasterization (G1.8 round 1).
    add_executable(glyph_atlas_test
                   graphics/gles/glyph_atlas_test.cc)
    target_link_libraries(glyph_atlas_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics vx_text
              GTest::gtest_main)
    gtest_discover_tests(glyph_atlas_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # GLESCanvas DrawText via GlyphAtlas (G1.8 round 2).
    add_executable(gles_canvas_text_test
                   graphics/gles/gles_canvas_text_test.cc)
    target_link_libraries(gles_canvas_text_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics vx_text
              GTest::gtest_main)
    gtest_discover_tests(gles_canvas_text_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # ImageTexturePool RGBA8 texture cache (G1.9 round 1).
    add_executable(image_texture_pool_test
                   graphics/gles/image_texture_pool_test.cc)
    target_link_libraries(image_texture_pool_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics GTest::gtest_main)
    gtest_discover_tests(image_texture_pool_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")

    # GLESCanvas DrawImage via ImageTexturePool (G1.9 round 2).
    add_executable(gles_canvas_image_test
                   graphics/gles/gles_canvas_image_test.cc)
    target_link_libraries(gles_canvas_image_test
      PRIVATE vx_foundation vx_platform_sdl2 vx_graphics GTest::gtest_main)
    gtest_discover_tests(gles_canvas_image_test
      PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen")
  endif()

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

  # ---- DevTool 三件套 dogfood smoke ctest 集 ----
  # Phase A Inspector       (TASK-20260502-01) — hello_devtool_smoke
  # Phase B Perf Overlay    (TASK-20260502-02) — hello_devtool_perf_smoke
  # Phase C Hot Reload      (TASK-20260503-01) — hello_devtool_hot_reload_smoke
  # Three-piece DevTool main line; finalize装裱 by TASK-20260503-03 P2.
  if(VX_BUILD_DEVTOOL)
    # Phase A — Inspector smoke (DevTool attach + dogfood UI baseline;
    # TASK-20260502-01 A.3.1). Requires VX_BUILD_DEVTOOL=ON (the example
    # only compiles then). Same SDL2 dummy-driver + auto-quit pattern
    # as hello_sdl2_smoke.
    add_test(
      NAME hello_devtool_smoke
      COMMAND $<TARGET_FILE:hello_devtool>
    )
    set_tests_properties(hello_devtool_smoke PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;VX_HELLO_DEVTOOL_AUTOQUIT_MS=200"
      TIMEOUT 10
    )

    # Phase B — Performance Overlay smoke (PipelineHooks 5 hooks +
    # vx_view_set_pipeline_hooks ABI smoke; TASK-20260502-02 B.3.2).
    # PASS_REGULAR_EXPRESSION asserts the stable "PERF SMOKE: frames=N"
    # output line.
    #
    # TASK-20260505-02 D.1 (B-G4 closure): hello_devtool's on_frame_end
    # hook now calls vx_view_invalidate() force-rearming dirty_ each
    # frame, so the 300ms autoquit @ 60 fps yields 10+ frames stably
    # (实测 frames=18 on dev box). Match >=2 frames to keep resilient
    # on slow CI hardware while still proving multi-frame behavior
    # (TASK-20260503-03 P3 #0 closed; previous ABI-only frames=[1-9][0-9]*
    # smoke replaced).
    add_test(
      NAME hello_devtool_perf_smoke
      COMMAND $<TARGET_FILE:hello_devtool>
    )
    set_tests_properties(hello_devtool_perf_smoke PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;VX_HELLO_DEVTOOL_AUTOQUIT_MS=300"
      PASS_REGULAR_EXPRESSION "PERF SMOKE: frames=([2-9]|[1-9][0-9]+) hud_visible=1"
      TIMEOUT 10
    )

    # Phase C — Hot Reload smoke (inotify + LoadCSS triggered;
    # TASK-20260503-01 C.4.2). Drives hello_devtool with
    # VX_HELLO_DEVTOOL_HOT_RELOAD_TEST=1 so the binary mkdtemps a
    # watch dir, attaches DevTool with hot_reload_dir, rewrites the
    # watched style.css at +100ms, and then prints the stable
    # "HOT RELOAD: triggered count=N" line on exit. Auto-quit is
    # generous (800 ms) so on slow CI the inotify worker has time
    # to: fire IN_CLOSE_WRITE → debounce 50ms → main-thread
    # DrainEvents → LoadCSS → tracked_count++. PASS_REGULAR_EXPRESSION
    # demands at least one tracked invocation.
    add_test(
      NAME hello_devtool_hot_reload_smoke
      COMMAND $<TARGET_FILE:hello_devtool>
    )
    set_tests_properties(hello_devtool_hot_reload_smoke PROPERTIES
      ENVIRONMENT
        "SDL_VIDEODRIVER=dummy;VX_HELLO_DEVTOOL_AUTOQUIT_MS=800;VX_HELLO_DEVTOOL_HOT_RELOAD_TEST=1"
      PASS_REGULAR_EXPRESSION "HOT RELOAD: triggered count=[1-9][0-9]*"
      TIMEOUT 15
    )
  endif()
endif()
