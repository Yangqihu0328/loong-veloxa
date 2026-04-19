# benchmarks/CMakeLists.txt
#
# Foundation + CSS + Layout/Render performance benchmarks. Enabled via
# -DVX_BUILD_BENCHMARKS=ON (see root CMakeLists.txt). Each .cc file becomes
# its own executable; add more with
# vx_add_benchmark(<name> [extra_lib1 extra_lib2 ...]).

# TASK-20260419-09: bench_imagecache writes libpng PNG fixtures to /tmp via
# the libpng C API. vx_core links PNG::PNG PRIVATE-ly, so it does not
# propagate; we must find_package here so the bench can link directly.
find_package(PNG REQUIRED)

# Mark Google Benchmark headers as SYSTEM on every consumer so the strict
# -Werror -Wpedantic from the root CMakeLists.txt does not flag third-party
# code we cannot fix. `benchmark::benchmark` is an ALIAS — operate on the
# real target `benchmark`.
get_target_property(_bench_includes benchmark
                    INTERFACE_INCLUDE_DIRECTORIES)
if(_bench_includes)
  set_target_properties(benchmark PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_bench_includes}")
endif()

# vx_add_benchmark(<name> [extra_lib1 extra_lib2 ...])
#   Adds a benchmark executable from <name>.cc and links vx_foundation +
#   benchmark::benchmark plus any extra libraries passed via ARGN (e.g.
#   vx_core for CSS benches that need parser/tokenizer symbols).
function(vx_add_benchmark name)
  set(extra_libs ${ARGN})
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} PRIVATE
    vx_foundation
    benchmark::benchmark
    ${extra_libs})
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks")
endfunction()

# Foundation benches (TASK-20260419-02): single-arg form is back-compatible.
vx_add_benchmark(bench_allocators)
vx_add_benchmark(bench_containers)
vx_add_benchmark(bench_hash_map)
vx_add_benchmark(bench_strings)

# CSS benches (TASK-20260419-03): need vx_core for tokenizer/parser/property symbols.
vx_add_benchmark(bench_css_tokenizer        vx_core)
vx_add_benchmark(bench_css_parser           vx_core)
vx_add_benchmark(bench_css_property_lookup  vx_core)

# Layout + Render benches (TASK-20260419-05). Replay needs vx_graphics + vx_platform
# for SoftwareCanvas; the others stay on vx_core only.
vx_add_benchmark(bench_layout_buildtree     vx_core)
vx_add_benchmark(bench_layout_flex          vx_core)
vx_add_benchmark(bench_render_record        vx_core)
vx_add_benchmark(bench_render_replay        vx_core vx_graphics vx_platform)

# Replay deep-bench (TASK-20260419-09): bench_drawtext needs vx_text for
# FontManager + GlyphCache (real DrawText path). bench_imagecache also needs
# PNG::PNG to write libpng fixtures into /tmp.
vx_add_benchmark(bench_drawtext   vx_core vx_graphics vx_platform vx_text)
vx_add_benchmark(bench_imagecache vx_core vx_graphics vx_platform vx_text PNG::PNG)
