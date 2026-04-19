# benchmarks/CMakeLists.txt
#
# Foundation performance benchmarks. Enabled via -DVX_BUILD_BENCHMARKS=ON
# (see root CMakeLists.txt). Each .cc file becomes its own executable; add
# more with vx_add_benchmark(<name>).

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

function(vx_add_benchmark name)
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name} PRIVATE vx_foundation benchmark::benchmark)
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks")
endfunction()

vx_add_benchmark(bench_allocators)
vx_add_benchmark(bench_containers)
vx_add_benchmark(bench_hash_map)
vx_add_benchmark(bench_strings)
