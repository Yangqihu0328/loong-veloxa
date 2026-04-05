# Foundation 基础库 实现计划

**目标：** 为 Veloxa 引擎构建生产级的 Foundation 基础库，包含内存管理、容器、字符串、日志四大子系统。

**架构：** 零外部依赖的底层库，所有上层模块（Graphics HAL、Core Engine、Script 等）都将依赖它。通过 Allocator 模板参数实现内存策略可插拔，通过编译宏实现模块可裁剪。

**技术栈：** C++17 / CMake / Google Test / Google Benchmark / Google C++ Style

**复杂度级别：** Level 4（子系统级实现）

---

## 文件结构

### 将创建的文件

| 文件路径 | 职责 |
|---------|------|
| `CMakeLists.txt` | 根 CMake 配置 |
| `.clang-format` | Google C++ 格式化配置 |
| `.clang-tidy` | 静态分析配置 |
| `.gitignore` | Git 忽略规则 |
| `veloxa/foundation/CMakeLists.txt` | Foundation 库构建 |
| `veloxa/foundation/base/types.h` | 基本类型别名 |
| `veloxa/foundation/base/assert.h` | 断言宏 |
| `veloxa/foundation/base/span.h` | Span 非拥有视图 |
| `veloxa/foundation/base/status.h` | Status 错误类型 |
| `veloxa/foundation/memory/allocator.h` | Allocator 概念 |
| `veloxa/foundation/memory/memory_stats.h` | 内存统计 |
| `veloxa/foundation/memory/malloc_allocator.h` | malloc 封装 |
| `veloxa/foundation/memory/malloc_allocator.cc` | 实现 |
| `veloxa/foundation/memory/arena_allocator.h` | Arena 分配器 |
| `veloxa/foundation/memory/arena_allocator.cc` | 实现 |
| `veloxa/foundation/memory/pool_allocator.h` | Pool 分配器 |
| `veloxa/foundation/memory/pool_allocator.cc` | 实现 |
| `veloxa/foundation/containers/vector.h` | Vector 容器 |
| `veloxa/foundation/containers/small_vector.h` | SmallVector 容器 |
| `veloxa/foundation/containers/intrusive_list.h` | 侵入式链表 |
| `veloxa/foundation/containers/hash_map.h` | HashMap 容器 |
| `veloxa/foundation/strings/string_view.h` | StringView |
| `veloxa/foundation/strings/utf.h` | UTF 编解码 |
| `veloxa/foundation/strings/utf.cc` | 实现 |
| `veloxa/foundation/strings/string.h` | String (SSO) |
| `veloxa/foundation/strings/string.cc` | 实现 |
| `veloxa/foundation/strings/interned_string.h` | InternedString |
| `veloxa/foundation/strings/interned_string.cc` | 实现 |
| `veloxa/foundation/log/log.h` | 日志宏与级别 |
| `veloxa/foundation/log/log_sink.h` | 日志 Sink 接口 |
| `veloxa/foundation/log/log.cc` | 日志实现 |
| `tests/foundation/base/types_test.cc` | 类型测试 |
| `tests/foundation/base/assert_test.cc` | 断言测试 |
| `tests/foundation/base/span_test.cc` | Span 测试 |
| `tests/foundation/base/status_test.cc` | Status 测试 |
| `tests/foundation/memory/malloc_allocator_test.cc` | malloc 封装测试 |
| `tests/foundation/memory/arena_allocator_test.cc` | Arena 测试 |
| `tests/foundation/memory/pool_allocator_test.cc` | Pool 测试 |
| `tests/foundation/containers/vector_test.cc` | Vector 测试 |
| `tests/foundation/containers/small_vector_test.cc` | SmallVector 测试 |
| `tests/foundation/containers/intrusive_list_test.cc` | IntrusiveList 测试 |
| `tests/foundation/containers/hash_map_test.cc` | HashMap 测试 |
| `tests/foundation/strings/string_view_test.cc` | StringView 测试 |
| `tests/foundation/strings/utf_test.cc` | UTF 测试 |
| `tests/foundation/strings/string_test.cc` | String 测试 |
| `tests/foundation/strings/interned_string_test.cc` | InternedString 测试 |
| `tests/foundation/log/log_test.cc` | 日志测试 |
| `tests/CMakeLists.txt` | 测试构建配置 |
| `benchmarks/foundation/CMakeLists.txt` | Benchmark 构建 |
| `benchmarks/foundation/containers_bench.cc` | 容器 benchmark |
| `benchmarks/foundation/strings_bench.cc` | 字符串 benchmark |

---

## Phase 1：项目脚手架

### 任务 1.1：根 CMake 与目录结构 [TDD]

**文件：**
- 创建：`CMakeLists.txt`
- 创建：`.gitignore`
- 创建：`.clang-format`
- 创建：`.clang-tidy`

- [ ] **步骤 1：创建根 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(veloxa VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VX_BUILD_TESTS "Build tests" ON)
option(VX_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(VX_LOG_LEVEL "Minimum log level (0=DEBUG,1=INFO,2=WARN,3=ERROR,4=FATAL)" 0)

add_compile_definitions(VX_MIN_LOG_LEVEL=${VX_LOG_LEVEL})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter)
endif()

add_subdirectory(veloxa/foundation)

if(VX_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )
  FetchContent_MakeAvailable(googletest)
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
```

- [ ] **步骤 2：创建 .gitignore**

```
build/
cmake-build-*/
.cache/
compile_commands.json
*.o
*.a
*.so
*.dylib
.idea/
.vscode/
!.vscode/settings.json
```

- [ ] **步骤 3：创建 .clang-format**

```yaml
BasedOnStyle: Google
ColumnLimit: 100
IndentWidth: 2
PointerAlignment: Left
DerivePointerAlignment: false
AllowShortFunctionsOnASingleLine: Inline
```

- [ ] **步骤 4：创建 .clang-tidy**

```yaml
Checks: >
  -*,
  bugprone-*,
  clang-analyzer-*,
  google-*,
  misc-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -readability-magic-numbers
WarningsAsErrors: ''
HeaderFilterRegex: 'veloxa/.*'
```

- [ ] **步骤 5：创建 Foundation CMakeLists.txt**

```cmake
# veloxa/foundation/CMakeLists.txt
add_library(vx_foundation STATIC)

target_sources(vx_foundation PRIVATE
  memory/malloc_allocator.cc
  memory/arena_allocator.cc
  memory/pool_allocator.cc
  strings/utf.cc
  strings/string.cc
  strings/interned_string.cc
  log/log.cc
)

target_include_directories(vx_foundation PUBLIC
  ${CMAKE_SOURCE_DIR}
)

target_compile_features(vx_foundation PUBLIC cxx_std_17)
```

- [ ] **步骤 6：创建最小测试 CMakeLists.txt**

```cmake
# tests/CMakeLists.txt
include(GoogleTest)

function(vx_add_test TEST_NAME TEST_SRC)
  add_executable(${TEST_NAME} ${TEST_SRC})
  target_link_libraries(${TEST_NAME} PRIVATE vx_foundation GTest::gtest_main)
  gtest_discover_tests(${TEST_NAME})
endfunction()
```

- [ ] **步骤 7：创建占位源文件，验证构建**

创建最小的 `veloxa/foundation/base/types.h`（仅 include guard），和空的 `.cc` 占位文件。

运行：`cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build`
预期：编译成功

- [ ] **步骤 8：提交**

`git add -A && git commit -m "chore: project scaffolding with CMake, clang-format, clang-tidy"`

---

## Phase 2：基础类型 (base/)

### 任务 2.1：types.h [TDD]

**文件：**
- 创建：`veloxa/foundation/base/types.h`
- 测试：`tests/foundation/base/types_test.cc`

- [ ] **步骤 1：编写测试**

```cpp
// tests/foundation/base/types_test.cc
#include "veloxa/foundation/base/types.h"

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(TypesTest, SizeGuarantees) {
  EXPECT_EQ(sizeof(u8), 1);
  EXPECT_EQ(sizeof(u16), 2);
  EXPECT_EQ(sizeof(u32), 4);
  EXPECT_EQ(sizeof(u64), 8);
  EXPECT_EQ(sizeof(i8), 1);
  EXPECT_EQ(sizeof(i16), 2);
  EXPECT_EQ(sizeof(i32), 4);
  EXPECT_EQ(sizeof(i64), 8);
  EXPECT_EQ(sizeof(f32), 4);
  EXPECT_EQ(sizeof(f64), 8);
}

TEST(TypesTest, Signedness) {
  EXPECT_FALSE(std::is_signed_v<u8>);
  EXPECT_FALSE(std::is_signed_v<u32>);
  EXPECT_TRUE(std::is_signed_v<i32>);
  EXPECT_TRUE(std::is_signed_v<f32>);
}

}  // namespace
}  // namespace vx
```

- [ ] **步骤 2：运行测试验证失败**
  运行：`cmake --build build && ctest --test-dir build`
  预期：编译失败（types.h 未实现）

- [ ] **步骤 3：实现**

```cpp
// veloxa/foundation/base/types.h
#ifndef VELOXA_FOUNDATION_BASE_TYPES_H_
#define VELOXA_FOUNDATION_BASE_TYPES_H_

#include <cstddef>
#include <cstdint>

namespace vx {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using usize = size_t;
using isize = ptrdiff_t;
using byte = uint8_t;

}  // namespace vx

#endif  // VELOXA_FOUNDATION_BASE_TYPES_H_
```

- [ ] **步骤 4：运行测试验证通过**
  运行：`cmake --build build && ctest --test-dir build`
  预期：PASS

- [ ] **步骤 5：提交**

### 任务 2.2：assert.h [TDD]

**文件：**
- 创建：`veloxa/foundation/base/assert.h`
- 测试：`tests/foundation/base/assert_test.cc`

- [ ] **步骤 1：编写测试**

```cpp
// tests/foundation/base/assert_test.cc
#include "veloxa/foundation/base/assert.h"

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(AssertTest, CheckPassesOnTrue) {
  VX_CHECK(true);
  VX_CHECK(1 == 1);
}

TEST(AssertTest, CheckFailsOnFalse) {
  EXPECT_DEATH(VX_CHECK(false), "");
}

TEST(AssertTest, DcheckInDebug) {
#ifndef NDEBUG
  EXPECT_DEATH(VX_DCHECK(false), "");
#else
  VX_DCHECK(false);  // should be no-op in release
#endif
}

TEST(AssertTest, CheckWithMessage) {
  EXPECT_DEATH(VX_CHECK(false) << "custom message", "custom message");
}

}  // namespace
}  // namespace vx
```

- [ ] **步骤 2：运行测试验证失败** → 预期：编译失败

- [ ] **步骤 3：实现**

```cpp
// veloxa/foundation/base/assert.h
#ifndef VELOXA_FOUNDATION_BASE_ASSERT_H_
#define VELOXA_FOUNDATION_BASE_ASSERT_H_

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace vx {
namespace internal {

class CheckFailStream {
 public:
  CheckFailStream(const char* file, int line, const char* expr)
      : file_(file), line_(line), expr_(expr) {}

  ~CheckFailStream() {
    std::fprintf(stderr, "CHECK failed: %s at %s:%d %s\n", expr_, file_, line_,
                 stream_.str().c_str());
    std::abort();
  }

  template <typename T>
  CheckFailStream& operator<<(const T& val) {
    stream_ << val;
    return *this;
  }

 private:
  const char* file_;
  int line_;
  const char* expr_;
  std::ostringstream stream_;
};

class NullStream {
 public:
  template <typename T>
  NullStream& operator<<(const T&) {
    return *this;
  }
};

}  // namespace internal
}  // namespace vx

#define VX_CHECK(cond)                                                 \
  if (!(cond))                                                         \
  ::vx::internal::CheckFailStream(__FILE__, __LINE__, #cond)

#ifndef NDEBUG
#define VX_DCHECK(cond) VX_CHECK(cond)
#else
#define VX_DCHECK(cond) \
  if (false) ::vx::internal::NullStream()
#endif

#endif  // VELOXA_FOUNDATION_BASE_ASSERT_H_
```

- [ ] **步骤 4：运行测试验证通过** → 预期：PASS
- [ ] **步骤 5：提交**

### 任务 2.3：span.h [TDD]

**文件：**
- 创建：`veloxa/foundation/base/span.h`
- 测试：`tests/foundation/base/span_test.cc`

核心接口：

```cpp
template <typename T>
class Span {
 public:
  Span() : data_(nullptr), size_(0) {}
  Span(T* data, usize size) : data_(data), size_(size) {}
  template <usize N>
  Span(T (&arr)[N]) : data_(arr), size_(N) {}

  T* data() const;
  usize size() const;
  bool empty() const;
  T& operator[](usize i) const;
  Span subspan(usize offset, usize count = npos) const;
  T* begin() const;
  T* end() const;
};
```

测试要点：构造、索引、subspan 边界、空 span、迭代器、const 正确性。

- [ ] **步骤 1-5：** TDD 循环（同上模式）
- [ ] **步骤 6：提交**

### 任务 2.4：status.h [TDD]

**文件：**
- 创建：`veloxa/foundation/base/status.h`
- 测试：`tests/foundation/base/status_test.cc`

核心接口：

```cpp
enum class StatusCode : u8 {
  kOk = 0,
  kInvalidArgument,
  kOutOfMemory,
  kNotFound,
  kAlreadyExists,
  kInternal,
};

class Status {
 public:
  Status() = default;  // Ok
  Status(StatusCode code, StringView message);
  bool ok() const;
  StatusCode code() const;
  StringView message() const;
  static Status Ok();
};

template <typename T>
class StatusOr {
 public:
  StatusOr(const T& value);
  StatusOr(T&& value);
  StatusOr(Status status);
  bool ok() const;
  const T& value() const;
  T& value();
  const Status& status() const;
};
```

测试要点：Ok/Error 构造、StatusOr 值/错误访问、CHECK 失败在错误时取 value。

> 注意：Status 内部 message 暂用 `std::string` 存储，后续 String 完成后可替换。

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(foundation): add base types, assert, span, status"`

---

## Phase 3：内存管理子系统 (memory/)

### 任务 3.1：Allocator 概念与 MemoryStats [TDD]

**文件：**
- 创建：`veloxa/foundation/memory/allocator.h`
- 创建：`veloxa/foundation/memory/memory_stats.h`

```cpp
// allocator.h — Allocator concept
namespace vx {

template <typename A>
concept Allocator = requires(A a, void* p, usize size, usize align) {
  { a.Allocate(size, align) } -> std::same_as<void*>;
  { a.Deallocate(p, size) } -> std::same_as<void>;
};

}  // namespace vx
```

```cpp
// memory_stats.h
namespace vx {

struct MemoryStats {
  std::atomic<i64> current_bytes{0};
  std::atomic<i64> peak_bytes{0};
  std::atomic<i64> total_allocations{0};

  void RecordAlloc(usize bytes);
  void RecordDealloc(usize bytes);
  void Reset();
};

MemoryStats& GlobalMemoryStats();

}  // namespace vx
```

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 3.2：MallocAllocator [TDD]

**文件：**
- 创建：`veloxa/foundation/memory/malloc_allocator.h`
- 创建：`veloxa/foundation/memory/malloc_allocator.cc`
- 测试：`tests/foundation/memory/malloc_allocator_test.cc`

核心实现：

```cpp
class MallocAllocator {
 public:
  void* Allocate(usize size, usize align = alignof(std::max_align_t));
  void Deallocate(void* ptr, usize size);
};
```

测试要点：
- 基本分配/释放
- 对齐保证（align=16, 32, 64）
- 统计正确（allocate 增、deallocate 减、peak 只增不减）
- OOM 回调触发（可 mock）
- 零大小分配行为

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 3.3：ArenaAllocator [TDD]

**文件：**
- 创建：`veloxa/foundation/memory/arena_allocator.h`
- 创建：`veloxa/foundation/memory/arena_allocator.cc`
- 测试：`tests/foundation/memory/arena_allocator_test.cc`

核心设计：

```cpp
class ArenaAllocator {
 public:
  explicit ArenaAllocator(usize block_size = 4096);
  ~ArenaAllocator();

  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;

  void* Allocate(usize size, usize align = alignof(std::max_align_t));
  void Deallocate(void* ptr, usize size);  // no-op
  void Reset();  // 释放所有 block 除首块

  usize bytes_allocated() const;

 private:
  struct Block {
    Block* next;
    usize size;
    usize used;
    // data follows
  };
  Block* current_;
  usize default_block_size_;
};
```

测试要点：
- 连续小分配在同一 block
- 超出 block 自动分配新 block
- Reset 后可复用
- Deallocate 为 no-op（不崩溃）
- 对齐保证
- 超大单次分配（>block_size）

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 3.4：PoolAllocator [TDD]

**文件：**
- 创建：`veloxa/foundation/memory/pool_allocator.h`
- 创建：`veloxa/foundation/memory/pool_allocator.cc`
- 测试：`tests/foundation/memory/pool_allocator_test.cc`

核心设计：

```cpp
class PoolAllocator {
 public:
  explicit PoolAllocator(usize block_size, usize blocks_per_chunk = 64);
  ~PoolAllocator();

  void* Allocate(usize size, usize align = alignof(std::max_align_t));
  void Deallocate(void* ptr, usize size);

 private:
  struct FreeNode { FreeNode* next; };
  struct Chunk { Chunk* next; };

  void AllocateChunk();

  usize block_size_;
  usize blocks_per_chunk_;
  FreeNode* free_list_;
  Chunk* chunks_;
};
```

测试要点：
- 分配/释放/重用（释放后再分配返回同一内存）
- 批量分配耗尽 chunk 后自动扩展
- 大小不匹配时的行为（VX_CHECK）
- 对齐保证

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(foundation): memory subsystem - malloc/arena/pool allocators"`

---

## Phase 4：日志子系统 (log/)

### 任务 4.1：日志系统 [TDD]

**文件：**
- 创建：`veloxa/foundation/log/log.h`
- 创建：`veloxa/foundation/log/log_sink.h`
- 创建：`veloxa/foundation/log/log.cc`
- 测试：`tests/foundation/log/log_test.cc`

核心接口：

```cpp
// log_sink.h
namespace vx {

enum class LogLevel : u8 {
  kDebug = 0,
  kInfo = 1,
  kWarn = 2,
  kError = 3,
  kFatal = 4,
};

class LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void Write(LogLevel level, const char* file, int line,
                     const char* msg) = 0;
};

void SetLogSink(LogSink* sink);
LogSink* GetLogSink();

}  // namespace vx
```

```cpp
// log.h — 宏定义
#define VX_LOG(level, ...)                                          \
  do {                                                               \
    if (static_cast<int>(::vx::LogLevel::k##level) >=               \
        VX_MIN_LOG_LEVEL) {                                          \
      ::vx::internal::LogMessage(::vx::LogLevel::k##level,          \
                                  __FILE__, __LINE__, __VA_ARGS__);  \
    }                                                                \
  } while (false)

#define VX_LOG_DEBUG(...) VX_LOG(Debug, __VA_ARGS__)
#define VX_LOG_INFO(...) VX_LOG(Info, __VA_ARGS__)
#define VX_LOG_WARN(...) VX_LOG(Warn, __VA_ARGS__)
#define VX_LOG_ERROR(...) VX_LOG(Error, __VA_ARGS__)
#define VX_LOG_FATAL(...) VX_LOG(Fatal, __VA_ARGS__)
```

测试要点：
- 自定义 Sink 捕获日志消息
- 不同级别正确路由
- Fatal 触发 abort（EXPECT_DEATH）
- 编译时裁剪验证（通过 mock Sink 验证低级别日志未调用）

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(foundation): log subsystem with compile-time level stripping"`

---

## Phase 5：容器库 (containers/)

### 任务 5.1：Vector [TDD]

**文件：**
- 创建：`veloxa/foundation/containers/vector.h`
- 测试：`tests/foundation/containers/vector_test.cc`

核心接口：

```cpp
template <typename T, typename Alloc = MallocAllocator>
class Vector {
 public:
  Vector();
  explicit Vector(Alloc& alloc);
  explicit Vector(usize count, const T& value = T());
  Vector(std::initializer_list<T> init);
  Vector(const Vector& other);
  Vector(Vector&& other) noexcept;
  Vector& operator=(const Vector& other);
  Vector& operator=(Vector&& other) noexcept;
  ~Vector();

  T& operator[](usize i);
  const T& operator[](usize i) const;
  T& front();
  T& back();
  T* data();

  T* begin();
  T* end();

  bool empty() const;
  usize size() const;
  usize capacity() const;
  void reserve(usize new_cap);
  void resize(usize count);

  void push_back(const T& value);
  void push_back(T&& value);
  template <typename... Args>
  T& emplace_back(Args&&... args);
  void pop_back();
  void clear();

  void insert(T* pos, const T& value);
  void erase(T* pos);
  void erase(T* first, T* last);

 private:
  T* data_;
  usize size_;
  usize capacity_;
  Alloc* alloc_;  // 指针或 compressed pair
};
```

测试要点：
- 空构造、初始化列表、拷贝/移动构造与赋值
- push_back 增长（验证 capacity 按 1.5x 增长）
- emplace_back 就地构造
- 非平凡类型（析构计数验证无泄漏）
- 与 ArenaAllocator 配合使用
- 边界：空 vector pop_back（DCHECK 失败）

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 5.2：SmallVector [TDD]

**文件：**
- 创建：`veloxa/foundation/containers/small_vector.h`
- 测试：`tests/foundation/containers/small_vector_test.cc`

核心设计：继承 Vector 接口，内嵌 `alignas(T) char inline_storage_[N * sizeof(T)]`。`size <= N` 时用 inline_storage_，溢出后切换到堆分配。

测试要点：
- N 以内不分配（配合 mock allocator 验证 Allocate 未调用）
- 溢出后正确迁移到堆
- 缩小后不回到 inline（简化实现）
- 移动语义正确性

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 5.3：IntrusiveList [TDD]

**文件：**
- 创建：`veloxa/foundation/containers/intrusive_list.h`
- 测试：`tests/foundation/containers/intrusive_list_test.cc`

核心接口：

```cpp
struct IntrusiveListNode {
  IntrusiveListNode* prev = nullptr;
  IntrusiveListNode* next = nullptr;
};

template <typename T, IntrusiveListNode T::*NodeMember>
class IntrusiveList {
 public:
  void push_front(T* item);
  void push_back(T* item);
  void remove(T* item);
  T* front() const;
  T* back() const;
  bool empty() const;
  usize size() const;

  class Iterator { /* ... */ };
  Iterator begin() const;
  Iterator end() const;
};
```

测试要点：
- 插入/删除/遍历
- 双向遍历
- 移除中间/头/尾元素
- 空列表行为
- 元素生命周期不受列表管理

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 5.4：HashMap [TDD]

**文件：**
- 创建：`veloxa/foundation/containers/hash_map.h`
- 测试：`tests/foundation/containers/hash_map_test.cc`

核心设计：

```cpp
template <typename K, typename V,
          typename Hash = std::hash<K>,
          typename Eq = std::equal_to<K>,
          typename Alloc = MallocAllocator>
class HashMap {
 public:
  HashMap();
  ~HashMap();

  V& operator[](const K& key);
  V* Find(const K& key);
  const V* Find(const K& key) const;
  bool Contains(const K& key) const;

  std::pair<V*, bool> Insert(const K& key, const V& value);
  std::pair<V*, bool> Insert(const K& key, V&& value);
  bool Erase(const K& key);

  usize size() const;
  bool empty() const;
  void clear();
  void reserve(usize count);

  // 迭代器
  class Iterator { /* ... */ };
  Iterator begin();
  Iterator end();

 private:
  // Robin Hood open addressing
  // ctrl_: u8 数组，Empty=0x80, Deleted=0xFE, 否则为 H2(hash) 低 7 位
  // slots_: 存储 key-value pair
  u8* ctrl_;
  struct Slot { K key; V value; };
  Slot* slots_;
  usize size_;
  usize capacity_;
};
```

测试要点：
- 基本 CRUD（Insert/Find/Erase）
- operator[] 自动插入
- 碰撞处理（构造碰撞键验证）
- rehash 后数据完整性
- 大量元素（10000+）性能不退化
- 与自定义 Hash/Eq 配合
- 与 ArenaAllocator 配合

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(foundation): containers - vector, small_vector, intrusive_list, hash_map"`

---

## Phase 6：字符串子系统 (strings/)

### 任务 6.1：StringView [TDD]

**文件：**
- 创建：`veloxa/foundation/strings/string_view.h`
- 测试：`tests/foundation/strings/string_view_test.cc`

核心接口：

```cpp
class StringView {
 public:
  static constexpr usize npos = static_cast<usize>(-1);

  constexpr StringView() : data_(nullptr), size_(0) {}
  constexpr StringView(const char* str);  // strlen
  constexpr StringView(const char* data, usize size);

  constexpr const char* data() const;
  constexpr usize size() const;
  constexpr bool empty() const;
  constexpr char operator[](usize i) const;

  constexpr StringView substr(usize pos, usize count = npos) const;
  constexpr usize find(char c, usize pos = 0) const;
  constexpr usize find(StringView s, usize pos = 0) const;
  constexpr bool starts_with(StringView prefix) const;
  constexpr bool ends_with(StringView suffix) const;

  constexpr int compare(StringView other) const;
  constexpr bool operator==(StringView other) const;
  constexpr bool operator!=(StringView other) const;
  constexpr bool operator<(StringView other) const;

  const char* begin() const;
  const char* end() const;
};
```

测试要点：构造、比较、find/substr、starts_with/ends_with、空 view、nullptr 安全。

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 6.2：UTF 编解码 [TDD]

**文件：**
- 创建：`veloxa/foundation/strings/utf.h`
- 创建：`veloxa/foundation/strings/utf.cc`
- 测试：`tests/foundation/strings/utf_test.cc`

核心接口：

```cpp
namespace vx::utf {

int EncodeUtf8(char32_t codepoint, char* out);  // 返回写入字节数
char32_t DecodeUtf8(const char* data, usize len, usize* bytes_consumed);
bool IsValidUtf8(const char* data, usize len);

usize Utf8ToUtf16(const char* src, usize src_len,
                  char16_t* dst, usize dst_cap);
usize Utf16ToUtf8(const char16_t* src, usize src_len,
                  char* dst, usize dst_cap);

class Utf8Iterator {
 public:
  Utf8Iterator(const char* data, usize size);
  char32_t operator*() const;
  Utf8Iterator& operator++();
  bool operator!=(const Utf8Iterator& other) const;
};

}  // namespace vx::utf
```

测试要点：
- ASCII、BMP（中文/日文）、Supplementary（emoji 🎉 = U+1F389）
- 边界码点（U+0000, U+007F, U+0080, U+07FF, U+0800, U+FFFF, U+10000, U+10FFFF）
- 非法序列（截断、overlong、超范围、代理对）
- UTF-8 ↔ UTF-16 往返正确性
- Utf8Iterator 遍历

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 6.3：String (SSO) [TDD]

**文件：**
- 创建：`veloxa/foundation/strings/string.h`
- 创建：`veloxa/foundation/strings/string.cc`
- 测试：`tests/foundation/strings/string_test.cc`

核心设计：

```cpp
template <typename Alloc = MallocAllocator>
class BasicString {
 public:
  BasicString();
  BasicString(const char* str);
  BasicString(StringView sv);
  BasicString(const char* data, usize size);
  BasicString(const BasicString& other);
  BasicString(BasicString&& other) noexcept;
  BasicString& operator=(const BasicString& other);
  BasicString& operator=(BasicString&& other) noexcept;
  ~BasicString();

  const char* c_str() const;
  const char* data() const;
  usize size() const;
  usize capacity() const;
  bool empty() const;

  void reserve(usize new_cap);
  void append(StringView sv);
  void append(char c);
  void clear();

  StringView view() const;
  operator StringView() const;

  char& operator[](usize i);
  char operator[](usize i) const;

  bool operator==(StringView other) const;
  bool operator!=(StringView other) const;
  bool operator<(StringView other) const;

 private:
  static constexpr usize kSSOCapacity = 22;
  // SSO layout: 最后字节存 (kSSOCapacity - size)，为 0 时表示堆模式
  union {
    struct {
      char* ptr;
      usize size;
      usize cap;
    } heap_;
    char sso_[kSSOCapacity + 1 + 1];  // +1 null +1 flag
  };
  bool IsSSO() const;
};

using String = BasicString<MallocAllocator>;
```

测试要点：
- SSO：<=22 字节不分配堆（mock allocator 验证）
- SSO → 堆过渡（23 字节触发堆分配）
- 拷贝/移动语义
- append 增长
- 与 StringView 互转
- c_str() null 终止保证
- 与 ArenaAllocator 配合

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 6.4：InternedString [TDD]

**文件：**
- 创建：`veloxa/foundation/strings/interned_string.h`
- 创建：`veloxa/foundation/strings/interned_string.cc`
- 测试：`tests/foundation/strings/interned_string_test.cc`

核心设计：

```cpp
class InternedString {
 public:
  InternedString() : data_(nullptr), size_(0) {}

  static InternedString Intern(StringView sv);

  const char* data() const { return data_; }
  usize size() const { return size_; }
  StringView view() const { return {data_, size_}; }

  bool operator==(InternedString other) const { return data_ == other.data_; }
  bool operator!=(InternedString other) const { return data_ != other.data_; }

 private:
  InternedString(const char* data, usize size) : data_(data), size_(size) {}
  const char* data_;
  usize size_;
};

// 全局 intern 表管理
void ClearInternedStrings();
```

测试要点：
- 相同字符串 Intern 返回相同指针
- 不同字符串不相等
- 比较退化为指针比较（benchmark 验证）
- ClearInternedStrings 后不持有悬挂指针（新 Intern 可用）

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交** `git commit -m "feat(foundation): strings subsystem - string_view, utf, string (SSO), interned_string"`

---

## Phase 7：集成验证与 Benchmark

### 任务 7.1：跨子系统集成测试 [TDD]

**文件：**
- 创建：`tests/foundation/integration_test.cc`

测试场景：
1. ArenaAllocator + Vector：大量 push_back 后 Arena Reset 验证无泄漏
2. PoolAllocator + 自定义类型：分配/释放循环
3. HashMap<InternedString, String>：模拟 CSS 属性表
4. String + UTF：中文字符串的构造、StringView 切片、UTF-16 转换

- [ ] **步骤 1-5：** TDD 循环
- [ ] **步骤 6：提交**

### 任务 7.2：Benchmark [覆盖补充]

**文件：**
- 创建：`benchmarks/foundation/CMakeLists.txt`
- 创建：`benchmarks/foundation/containers_bench.cc`
- 创建：`benchmarks/foundation/strings_bench.cc`

Benchmark 场景：
- `vx::Vector::push_back` vs `std::vector::push_back`（10, 100, 10000 元素）
- `vx::HashMap::Insert/Find` vs `std::unordered_map`（1000, 10000 键）
- `vx::String` SSO 构造 vs `std::string`
- `InternedString` 比较 vs `std::string` 比较
- `ArenaAllocator` 连续分配 vs `malloc` 连续分配

- [ ] **步骤 1-4：** 创建 benchmark，运行验证
- [ ] **步骤 5：提交** `git commit -m "feat(foundation): integration tests and benchmarks"`

---

## 任务总览

| Phase | 任务 | 文件数 | 预估时间 |
|-------|------|--------|---------|
| 1 | 项目脚手架 | 6 | 30 min |
| 2 | Base 类型 (types/assert/span/status) | 8 | 2 h |
| 3 | 内存管理 (allocator/malloc/arena/pool) | 10 | 3 h |
| 4 | 日志系统 | 4 | 1 h |
| 5 | 容器 (vector/small_vector/list/hashmap) | 8 | 4 h |
| 6 | 字符串 (view/utf/string/interned) | 10 | 4 h |
| 7 | 集成测试 + Benchmark | 4 | 1.5 h |
| **总计** | **17 个任务** | **~50 个文件** | **~15.5 h** |

## 创意设计决策（已完成）

以下组件已在 `/creative` 阶段完成深度设计：

1. **HashMap** — 选定 Swiss Table（Abseil 风格），SIMD 分组探测 + 标量回退。详见 `memory-bank/creative/creative-hashmap.md`
2. **String SSO** — 选定 fbstring 风格，sizeof=24，SSO 22 字节，cap 最低位标志。详见 `memory-bank/creative/creative-string-sso.md`
