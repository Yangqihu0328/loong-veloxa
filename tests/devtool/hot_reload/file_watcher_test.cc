// TASK-20260503-01 C.0.1: FileWatcher abstract base + Platform factory unit
// tests. Validates the cross-platform interface contract: CreatePlatform()
// returns a usable instance on Linux (inotify-backed in C.1.1; placeholder
// stub in C.0.1) and a nullptr on unsupported platforms (P2 candidate for
// macOS kqueue / Windows ReadDirectoryChangesW). IsRunning() must return
// false before Start() is invoked.

#include "veloxa/devtool/hot_reload/file_watcher.h"

#include <gtest/gtest.h>

namespace vx::devtool::hot_reload {
namespace {

TEST(FileWatcherTest, CreatePlatformReturnsLinuxImplOnLinux) {
  auto watcher = FileWatcher::CreatePlatform();
#if defined(__linux__)
  EXPECT_NE(watcher.get(), nullptr);
#else
  EXPECT_EQ(watcher.get(), nullptr);
#endif
}

TEST(FileWatcherTest, AbstractBaseExposesIsRunningFalseBeforeStart) {
  auto watcher = FileWatcher::CreatePlatform();
#if defined(__linux__)
  ASSERT_NE(watcher.get(), nullptr);
  EXPECT_FALSE(watcher->IsRunning());
#else
  GTEST_SKIP() << "FileWatcher not implemented on this platform.";
#endif
}

}  // namespace
}  // namespace vx::devtool::hot_reload
