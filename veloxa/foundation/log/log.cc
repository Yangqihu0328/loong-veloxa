#include "veloxa/foundation/log/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace vx {

namespace {

class StderrSink : public LogSink {
 public:
  void Write(LogLevel level, const char* file, int line,
             const char* msg) override {
    std::fprintf(stderr, "[%s] %s:%d: %s\n", LogLevelName(level),
                 internal::StripPath(file), line, msg);
  }
};

StderrSink g_default_sink;
LogSink* g_current_sink = &g_default_sink;

}  // namespace

void SetLogSink(LogSink* sink) {
  g_current_sink = sink ? sink : &g_default_sink;
}

LogSink* GetLogSink() { return g_current_sink; }

namespace internal {

void LogMessage(LogLevel level, const char* file, int line, const char* fmt,
                ...) {
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (level == LogLevel::kFatal && g_current_sink != &g_default_sink) {
    std::fprintf(stderr, "%s\n", buf);
  }

  g_current_sink->Write(level, file, line, buf);

  if (level == LogLevel::kFatal) {
    std::abort();
  }
}

}  // namespace internal
}  // namespace vx
