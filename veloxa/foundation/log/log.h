#ifndef VELOXA_FOUNDATION_LOG_LOG_H_
#define VELOXA_FOUNDATION_LOG_LOG_H_

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "veloxa/foundation/log/log_sink.h"

#ifndef VX_MIN_LOG_LEVEL
#define VX_MIN_LOG_LEVEL 0
#endif

namespace vx {
namespace internal {

void LogMessage(LogLevel level, const char* file, int line, const char* fmt, ...);

inline const char* StripPath(const char* path) {
  const char* last = std::strrchr(path, '/');
  return last ? last + 1 : path;
}

}  // namespace internal
}  // namespace vx

#define VX_LOG(level, ...)                                            \
  do {                                                                 \
    if (static_cast<int>(::vx::LogLevel::k##level) >=                 \
        VX_MIN_LOG_LEVEL) {                                            \
      ::vx::internal::LogMessage(::vx::LogLevel::k##level,            \
                                  __FILE__, __LINE__, __VA_ARGS__);    \
    }                                                                  \
  } while (false)

#define VX_LOG_DEBUG(...) VX_LOG(Debug, __VA_ARGS__)
#define VX_LOG_INFO(...) VX_LOG(Info, __VA_ARGS__)
#define VX_LOG_WARN(...) VX_LOG(Warn, __VA_ARGS__)
#define VX_LOG_ERROR(...) VX_LOG(Error, __VA_ARGS__)
#define VX_LOG_FATAL(...) VX_LOG(Fatal, __VA_ARGS__)

#endif  // VELOXA_FOUNDATION_LOG_LOG_H_
