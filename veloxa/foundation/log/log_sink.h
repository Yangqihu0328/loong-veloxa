#ifndef VELOXA_FOUNDATION_LOG_LOG_SINK_H_
#define VELOXA_FOUNDATION_LOG_LOG_SINK_H_

#include "veloxa/foundation/base/types.h"

namespace vx {

enum class LogLevel : u8 {
  kDebug = 0,
  kInfo = 1,
  kWarn = 2,
  kError = 3,
  kFatal = 4,
};

inline const char* LogLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug: return "DEBUG";
    case LogLevel::kInfo: return "INFO";
    case LogLevel::kWarn: return "WARN";
    case LogLevel::kError: return "ERROR";
    case LogLevel::kFatal: return "FATAL";
  }
  return "UNKNOWN";
}

class LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void Write(LogLevel level, const char* file, int line,
                     const char* msg) = 0;
};

void SetLogSink(LogSink* sink);
LogSink* GetLogSink();

}  // namespace vx

#endif  // VELOXA_FOUNDATION_LOG_LOG_SINK_H_
