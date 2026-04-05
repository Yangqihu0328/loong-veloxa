#include "veloxa/foundation/log/log.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace vx {
namespace {

struct LogEntry {
  LogLevel level;
  std::string file;
  int line;
  std::string msg;
};

class CaptureSink : public LogSink {
 public:
  void Write(LogLevel level, const char* file, int line,
             const char* msg) override {
    entries.push_back({level, file, line, msg});
  }
  std::vector<LogEntry> entries;
};

class LogTest : public ::testing::Test {
 protected:
  void SetUp() override { SetLogSink(&sink_); }
  void TearDown() override { SetLogSink(nullptr); }
  CaptureSink sink_;
};

TEST_F(LogTest, InfoLevel) {
  VX_LOG_INFO("hello %s", "world");
  ASSERT_EQ(sink_.entries.size(), 1u);
  EXPECT_EQ(sink_.entries[0].level, LogLevel::kInfo);
  EXPECT_EQ(sink_.entries[0].msg, "hello world");
}

TEST_F(LogTest, DebugLevel) {
  VX_LOG_DEBUG("debug %d", 42);
  // With VX_MIN_LOG_LEVEL=0, debug should be captured
#if VX_MIN_LOG_LEVEL <= 0
  ASSERT_EQ(sink_.entries.size(), 1u);
  EXPECT_EQ(sink_.entries[0].level, LogLevel::kDebug);
  EXPECT_EQ(sink_.entries[0].msg, "debug 42");
#endif
}

TEST_F(LogTest, ErrorLevel) {
  VX_LOG_ERROR("error: %s", "bad thing");
  ASSERT_EQ(sink_.entries.size(), 1u);
  EXPECT_EQ(sink_.entries[0].level, LogLevel::kError);
}

TEST_F(LogTest, WarnLevel) {
  VX_LOG_WARN("warning %d", 1);
  ASSERT_EQ(sink_.entries.size(), 1u);
  EXPECT_EQ(sink_.entries[0].level, LogLevel::kWarn);
}

TEST_F(LogTest, FatalAborts) {
  EXPECT_DEATH(VX_LOG_FATAL("fatal error"), "fatal error");
}

TEST_F(LogTest, MultipleLogs) {
  VX_LOG_INFO("first");
  VX_LOG_INFO("second");
  VX_LOG_WARN("third");
  EXPECT_EQ(sink_.entries.size(), 3u);
}

TEST_F(LogTest, FormatStrings) {
  VX_LOG_INFO("int=%d float=%.1f str=%s", 42, 3.14, "test");
  ASSERT_EQ(sink_.entries.size(), 1u);
  EXPECT_EQ(sink_.entries[0].msg, "int=42 float=3.1 str=test");
}

TEST_F(LogTest, LogLevelName) {
  EXPECT_STREQ(LogLevelName(LogLevel::kDebug), "DEBUG");
  EXPECT_STREQ(LogLevelName(LogLevel::kInfo), "INFO");
  EXPECT_STREQ(LogLevelName(LogLevel::kWarn), "WARN");
  EXPECT_STREQ(LogLevelName(LogLevel::kError), "ERROR");
  EXPECT_STREQ(LogLevelName(LogLevel::kFatal), "FATAL");
}

TEST_F(LogTest, ResetToDefaultSink) {
  SetLogSink(nullptr);
  // Should not crash with default sink
  VX_LOG_INFO("test default sink");
}

}  // namespace
}  // namespace vx
