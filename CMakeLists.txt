cmake_minimum_required(VERSION 3.20)
project(veloxa VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VX_BUILD_TESTS "Build tests" ON)
option(VX_BUILD_BENCHMARKS "Build benchmarks" OFF)
set(VX_LOG_LEVEL 0 CACHE STRING "Minimum log level (0=DEBUG,1=INFO,2=WARN,3=ERROR,4=FATAL)")

add_compile_definitions(VX_MIN_LOG_LEVEL=${VX_LOG_LEVEL})

# Apply strict warnings only to C++ — FetchContent C deps (e.g. QuickJS) are
# not authored under Veloxa -Wpedantic -Werror.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wextra>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Werror>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-type-limits>")
endif()

add_subdirectory(veloxa/foundation)
add_subdirectory(veloxa/platform)
add_subdirectory(veloxa/graphics)
add_subdirectory(veloxa/core)
add_subdirectory(veloxa/text)
add_subdirectory(veloxa/script)
add_subdirectory(veloxa/api)

add_subdirectory(examples)

if(VX_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  add_subdirectory(tests)
endif()

if(VX_BUILD_BENCHMARKS)
  include(FetchContent)
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
  )
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(benchmark)
  add_subdirectory(benchmarks)
endif()
