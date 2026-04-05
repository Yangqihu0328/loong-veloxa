// assert.h
#ifndef VELOXA_FOUNDATION_BASE_ASSERT_H_
#define VELOXA_FOUNDATION_BASE_ASSERT_H_

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace vx {
namespace internal {

class CheckFailStream {
 public:
  CheckFailStream(const char* file, int line, const char* expr)
      : file_(file), line_(line), expr_(expr) {}

  [[noreturn]] ~CheckFailStream() {
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
