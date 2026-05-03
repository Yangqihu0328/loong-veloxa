// TASK-20260503-01 C.0.1 + C.1.1: FileWatcher abstract base + Platform
// factory + InotifyFileWatcher (Linux) basic implementation tests.
//
// C.0.1 covers the cross-platform interface contract: CreatePlatform()
// returns a usable instance on Linux and a nullptr on unsupported
// platforms (P2). IsRunning() must be false before Start().
//
// C.1.1 covers the bare InotifyFileWatcher lifecycle (Start/Stop/restart,
// inotify_add_watch failure mapping, callback invocation on file modify).
// T2 8-step path traversal guards (canonicalize, boundary check, max_size,
// extension filter, debounce, ...) are implemented + tested in C.1.2.

#include "veloxa/devtool/hot_reload/file_watcher.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#if defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace vx::devtool::hot_reload {
namespace {

#if defined(__linux__)

// RAII temporary directory under /tmp. Destructor unlinks any regular
// files created beneath the root and removes the directory itself, so
// each test runs in an isolated sandbox even when run in parallel.
class TempDir {
 public:
  TempDir() {
    char tmpl[] = "/tmp/vx_hot_reload_test_XXXXXX";
    char* path = mkdtemp(tmpl);
    if (path != nullptr) {
      path_ = path;
    }
  }

  ~TempDir() {
    if (path_.empty()) return;
    RemoveChildren();
    rmdir(path_.c_str());
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  bool valid() const { return !path_.empty(); }
  const std::string& path() const { return path_; }

  std::string ChildPath(const char* name) const {
    return path_ + "/" + name;
  }

  void WriteFile(const char* name, const char* content) const {
    std::ofstream f(ChildPath(name));
    f << content;
  }

 private:
  void RemoveChildren() {
    // Tests only create regular files at the immediate root level, so a
    // single shell sweep is sufficient — avoids pulling in <dirent.h>.
    std::string cmd = "rm -f " + path_ + "/* 2>/dev/null";
    std::system(cmd.c_str());
  }

  std::string path_;
};

// Spin-wait helper so async tests don't sleep for the full timeout when
// the callback fires early. Polls the predicate at 5 ms intervals up to
// `timeout_ms` and returns whether it became true.
template <typename Pred>
bool WaitUntil(Pred pred, int timeout_ms = 500) {
  using clock = std::chrono::steady_clock;
  auto deadline = clock::now() + std::chrono::milliseconds(timeout_ms);
  while (clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return pred();
}

#endif  // defined(__linux__)

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

#if defined(__linux__)

TEST(InotifyFileWatcherTest, ModifyFileTriggersCallback) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  ASSERT_NE(watcher.get(), nullptr);

  std::atomic<int> callback_count{0};
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.callback = [&callback_count](const FileChangeEvent& /*evt*/) {
    callback_count.fetch_add(1, std::memory_order_relaxed);
  };
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());

  tmp.WriteFile("style.css", "body { color: red; }");

  EXPECT_TRUE(WaitUntil([&]() { return callback_count.load() > 0; }));
  EXPECT_GE(callback_count.load(), 1);

  watcher->Stop();
}

TEST(InotifyFileWatcherTest, StartStopRoundTrip) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  ASSERT_NE(watcher.get(), nullptr);
  EXPECT_FALSE(watcher->IsRunning());

  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.callback = [](const FileChangeEvent&) {};
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());
  EXPECT_TRUE(watcher->IsRunning());

  watcher->Stop();
  EXPECT_FALSE(watcher->IsRunning());
}

TEST(InotifyFileWatcherTest, RestartSucceeds) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  ASSERT_NE(watcher.get(), nullptr);

  for (int i = 0; i < 2; ++i) {
    WatchOptions opts;
    opts.root_dir = String(tmp.path().c_str());
    opts.callback = [](const FileChangeEvent&) {};
    ASSERT_TRUE(watcher->Start(std::move(opts)).ok())
        << "Start failed on iteration " << i;
    EXPECT_TRUE(watcher->IsRunning());
    watcher->Stop();
    EXPECT_FALSE(watcher->IsRunning());
  }
}

TEST(InotifyFileWatcherTest, StartReturnsErrorOnInvalidRoot) {
  auto watcher = FileWatcher::CreatePlatform();
  ASSERT_NE(watcher.get(), nullptr);

  WatchOptions opts;
  opts.root_dir = String("/tmp/vx_definitely_does_not_exist_42");
  opts.callback = [](const FileChangeEvent&) {};
  Status s = watcher->Start(std::move(opts));
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kNotFound);
  EXPECT_FALSE(watcher->IsRunning());
}

#endif  // defined(__linux__)

}  // namespace
}  // namespace vx::devtool::hot_reload
