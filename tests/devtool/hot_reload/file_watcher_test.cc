// TASK-20260503-01 C.0.1 + C.1.1 + C.1.2: FileWatcher abstract base +
// Platform factory + InotifyFileWatcher (Linux) backend tests.
//
// C.0.1 covers the cross-platform interface contract: CreatePlatform()
// returns a usable instance on Linux and a nullptr on unsupported
// platforms (P2). IsRunning() must be false before Start().
//
// C.1.1 covers the bare InotifyFileWatcher lifecycle (Start/Stop/restart,
// inotify_add_watch failure mapping, callback invocation on file modify).
//
// C.1.2 covers the eight T2 path-traversal guards mapped 1:1 to the
// spec §3.3 table (absolute root + locked mask + realpath canonicalize +
// boundary check + max_file_size + extension filter + WARN log + 50 ms
// debounce). The full security_test.cc with 16 entries (8 guards + 8
// reverse probes) lands in C.5.1.

#include "veloxa/devtool/hot_reload/file_watcher.h"

#if defined(__linux__)
#include "veloxa/devtool/hot_reload/file_watcher_inotify.h"
#endif

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "veloxa/foundation/log/log_sink.h"

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

// Thread-safe LogSink for capturing VX_LOG_WARN emitted by the watcher
// thread during T2 guard tests. Only retains the level + message text.
class CaptureSink : public ::vx::LogSink {
 public:
  void Write(::vx::LogLevel level, const char* /*file*/, int /*line*/,
             const char* msg) override {
    std::lock_guard<std::mutex> lk(mu_);
    entries_.push_back({level, std::string(msg)});
  }

  size_t CountAtOrAbove(::vx::LogLevel min_level) {
    std::lock_guard<std::mutex> lk(mu_);
    size_t n = 0;
    for (const auto& e : entries_) {
      if (static_cast<int>(e.first) >= static_cast<int>(min_level)) ++n;
    }
    return n;
  }

  bool MessageContains(const std::string& needle) {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& e : entries_) {
      if (e.second.find(needle) != std::string::npos) return true;
    }
    return false;
  }

 private:
  std::mutex mu_;
  std::vector<std::pair<::vx::LogLevel, std::string>> entries_;
};

class CaptureSinkScope {
 public:
  CaptureSinkScope() { ::vx::SetLogSink(&sink_); }
  ~CaptureSinkScope() { ::vx::SetLogSink(nullptr); }
  CaptureSink& sink() { return sink_; }

 private:
  CaptureSink sink_;
};

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

// ============================================================================
// C.1.2 — T2 path-traversal 8-step guards (spec §3.3 1:1)
// ============================================================================

// Step 1: watcher root must be an absolute path (allowlist).
TEST(InotifyFileWatcherT2Test, RelativeRootRejected) {
  auto watcher = FileWatcher::CreatePlatform();
  ASSERT_NE(watcher.get(), nullptr);

  WatchOptions opts;
  opts.root_dir = String("relative/path");
  opts.callback = [](const FileChangeEvent&) {};
  Status s = watcher->Start(std::move(opts));
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kInvalidArgument);
  EXPECT_FALSE(watcher->IsRunning());
}

// Step 2: locked inotify mask (IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO).
// Indirect coverage: mkdir under root triggers IN_CREATE which is NOT in
// the mask, so the callback must remain idle. Acts as a regression guard
// against accidentally widening the mask.
TEST(InotifyFileWatcherT2Test, OnlyExpectedMaskRegistered) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  std::atomic<int> count{0};
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.callback = [&count](const FileChangeEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  };
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());

  std::string subdir = tmp.path() + "/newdir";
  ASSERT_EQ(0, mkdir(subdir.c_str(), 0755));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  EXPECT_EQ(count.load(), 0);

  rmdir(subdir.c_str());
  watcher->Stop();
}

// Step 3: realpath() canonicalizes the watcher root so a symlinked root
// dir resolves to its target before any boundary check runs. Verified via
// the public ResolveAndValidate() helper to avoid fragile inotify-event
// timing on symlink targets (event delivery is inode-based and does not
// follow symlinks across watch points).
TEST(InotifyFileWatcherT2Test, CanonicalizesSymlink) {
  TempDir target;
  ASSERT_TRUE(target.valid());
  target.WriteFile("real.css", "body{}");

  std::string link_path =
      "/tmp/vx_hot_reload_symlink_" + std::to_string(::getpid());
  ::unlink(link_path.c_str());
  ASSERT_EQ(0, ::symlink(target.path().c_str(), link_path.c_str()));

  InotifyFileWatcher watcher;
  WatchOptions opts;
  opts.root_dir = String(link_path.c_str());
  opts.extensions.push_back(String(".css"));
  opts.callback = [](const FileChangeEvent&) {};
  ASSERT_TRUE(watcher.Start(std::move(opts)).ok());

  auto resolved = watcher.ResolveAndValidate("real.css");
  ASSERT_TRUE(resolved.ok()) << "resolve failed: " << resolved.status().message();
  EXPECT_NE(resolved.value().find(target.path()), std::string::npos)
      << "canonical path should reference target real path, got: " << resolved.value();
  EXPECT_EQ(resolved.value().find(link_path), std::string::npos)
      << "canonical path should NOT contain the symlink path, got: " << resolved.value();

  watcher.Stop();
  ::unlink(link_path.c_str());
}

// Step 4: boundary check rejects canonical paths that escape the root.
// Setup: place a symlink inside root that points to an external file;
// ResolveAndValidate() should fail because realpath resolves outside the
// watch root. Avoids relying on inotify events for the symlink target
// (which the kernel does not deliver into the root's watch).
TEST(InotifyFileWatcherT2Test, SymlinkEscapeRejected) {
  TempDir root;
  TempDir outside;
  ASSERT_TRUE(root.valid() && outside.valid());
  outside.WriteFile("target.css", "body{}");

  std::string link_path = root.path() + "/escape.css";
  ASSERT_EQ(0, ::symlink((outside.path() + "/target.css").c_str(),
                         link_path.c_str()));

  InotifyFileWatcher watcher;
  WatchOptions opts;
  opts.root_dir = String(root.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.callback = [](const FileChangeEvent&) {};
  ASSERT_TRUE(watcher.Start(std::move(opts)).ok());

  auto resolved = watcher.ResolveAndValidate("escape.css");
  EXPECT_FALSE(resolved.ok())
      << "should reject path that resolves outside root, got: "
      << (resolved.ok() ? resolved.value() : std::string());
  EXPECT_EQ(resolved.status().code(), StatusCode::kInvalidArgument);

  watcher.Stop();
  ::unlink(link_path.c_str());
}

// Step 5: max_file_size guard rejects oversize files.
TEST(InotifyFileWatcherT2Test, MaxFileSizeExceededRejected) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  std::atomic<int> count{0};
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.max_file_size = 1024;  // 1 KiB cap to keep the test fast
  opts.callback = [&count](const FileChangeEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  };
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());

  std::ofstream f(tmp.path() + "/big.css");
  f << std::string(5 * 1024, 'x');  // 5 KiB > 1 KiB
  f.close();

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  EXPECT_EQ(count.load(), 0);

  watcher->Stop();
}

// Step 6: extension filter rejects non-matching extensions.
TEST(InotifyFileWatcherT2Test, NonCssExtensionFiltered) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  std::atomic<int> css_count{0};
  std::atomic<int> other_count{0};
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.callback = [&css_count, &other_count](const FileChangeEvent& evt) {
    StringView path = evt.path;
    bool is_css =
        path.size() >= 4 &&
        ::memcmp(path.data() + path.size() - 4, ".css", 4) == 0;
    if (is_css) {
      css_count.fetch_add(1, std::memory_order_relaxed);
    } else {
      other_count.fetch_add(1, std::memory_order_relaxed);
    }
  };
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());

  tmp.WriteFile("doc.html", "<html></html>");
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  EXPECT_EQ(other_count.load(), 0);

  tmp.WriteFile("style.css", "body{}");
  EXPECT_TRUE(WaitUntil([&]() { return css_count.load() > 0; }));
  EXPECT_GE(css_count.load(), 1);
  EXPECT_EQ(other_count.load(), 0);

  watcher->Stop();
}

// Step 7: rejection emits VX_LOG_WARN. Captured via a thread-safe LogSink
// since the rejection happens on the watcher worker thread.
TEST(InotifyFileWatcherT2Test, RejectionLoggedAtWarn) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  CaptureSinkScope log_scope;

  auto watcher = FileWatcher::CreatePlatform();
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.max_file_size = 256;  // tiny cap to provoke rejection
  opts.callback = [](const FileChangeEvent&) {};
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());

  std::ofstream f(tmp.path() + "/big.css");
  f << std::string(2048, 'x');  // 2 KiB > 256 B
  f.close();

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  watcher->Stop();

  EXPECT_GE(log_scope.sink().CountAtOrAbove(::vx::LogLevel::kWarn), 1u);
  EXPECT_TRUE(log_scope.sink().MessageContains("Hot Reload"));
}

// Step 8: 50 ms debounce coalesces a rapid burst of writes into a single
// callback (matches creative #2 last_event_ms_ design).
TEST(InotifyFileWatcherT2Test, DebounceCoalescesRapidEvents) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  std::atomic<int> count{0};
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.callback = [&count](const FileChangeEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  };
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());

  for (int i = 0; i < 5; ++i) {
    std::ofstream f(tmp.path() + "/style.css");
    f << "body{ /* " << i << " */ }";
    f.close();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  int actual = count.load();
  EXPECT_GE(actual, 1);
  // 5 writes inside ~25 ms should collapse to 1 callback. Allow a small
  // upper bound (≤ 2) to absorb inotify timestamp jitter when the burst
  // straddles the 50 ms debounce boundary.
  EXPECT_LE(actual, 2)
      << "Debounce should coalesce a burst of 5 writes; got " << actual;

  watcher->Stop();
}

// ============================================================================
// C.1.3 — Lifecycle + edge case coverage (4 additional tests)
// ============================================================================

// vim / VS Code atomic write pattern: mkstemp -> write -> rename to the
// final name. The .swp file does not match the .css extension filter, so
// only the IN_MOVED_TO for "style.css" reaches the callback. Debounce
// keeps the count to a single callback even if inotify delivers
// IN_MOVED_TO + IN_CLOSE_WRITE back-to-back for the moved-into entry.
TEST(InotifyFileWatcherT2Test, AtomicWriteCoalesced) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  std::atomic<int> count{0};
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.callback = [&count](const FileChangeEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  };
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());

  std::string swp_path = tmp.path() + "/.style.css.swp";
  std::string final_path = tmp.path() + "/style.css";
  {
    std::ofstream f(swp_path);
    f << "body{ color: red; }";
  }
  ASSERT_EQ(0, ::rename(swp_path.c_str(), final_path.c_str()));

  EXPECT_TRUE(WaitUntil([&]() { return count.load() > 0; }));
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  EXPECT_LE(count.load(), 2)
      << "Atomic write should yield <= 2 callbacks (debounce); got "
      << count.load();

  watcher->Stop();
}

// Empty options (zero-length root_dir) hit the size==0 branch of Step 1.
// Distinct from RelativeRootRejected, which exercises the
// "non-absolute non-empty" branch.
TEST(InotifyFileWatcherT2Test, KInvalidArgumentOnEmptyOptions) {
  auto watcher = FileWatcher::CreatePlatform();
  ASSERT_NE(watcher.get(), nullptr);

  WatchOptions opts;  // root_dir default-constructed = empty
  opts.callback = [](const FileChangeEvent&) {};
  Status s = watcher->Start(std::move(opts));
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kInvalidArgument);
  EXPECT_FALSE(watcher->IsRunning());
}

// Two distinct files under the same root must each emit their own
// callback. The debounce map is keyed by canonical path so independent
// files never collide regardless of how close their writes land in time.
TEST(InotifyFileWatcherT2Test, MultipleFilesIndependentEvents) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  std::mutex paths_mu;
  std::vector<std::string> seen_paths;
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.callback = [&paths_mu, &seen_paths](const FileChangeEvent& evt) {
    std::lock_guard<std::mutex> lk(paths_mu);
    seen_paths.emplace_back(evt.path.data(), evt.path.size());
  };
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());

  tmp.WriteFile("a.css", "body{}");
  tmp.WriteFile("b.css", "p{}");

  EXPECT_TRUE(WaitUntil([&]() {
    std::lock_guard<std::mutex> lk(paths_mu);
    return seen_paths.size() >= 2;
  }));

  watcher->Stop();

  std::lock_guard<std::mutex> lk(paths_mu);
  bool saw_a = false, saw_b = false;
  for (const auto& p : seen_paths) {
    if (p.find("/a.css") != std::string::npos) saw_a = true;
    if (p.find("/b.css") != std::string::npos) saw_b = true;
  }
  EXPECT_TRUE(saw_a) << "expected a callback for /a.css";
  EXPECT_TRUE(saw_b) << "expected a callback for /b.css";
}

// Non-blocking inotify fd returns EAGAIN whenever no events are queued.
// WatchLoop must sleep ~50 ms and continue rather than busy-spin or
// exit. Verified indirectly: idle the watcher for 200 ms with no
// callback fires, no IsRunning() flip, and no thread crash on Stop.
TEST(InotifyFileWatcherT2Test, ReadEAGAINSleepFallback) {
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());

  auto watcher = FileWatcher::CreatePlatform();
  std::atomic<int> count{0};
  WatchOptions opts;
  opts.root_dir = String(tmp.path().c_str());
  opts.extensions.push_back(String(".css"));
  opts.callback = [&count](const FileChangeEvent&) {
    count.fetch_add(1, std::memory_order_relaxed);
  };
  ASSERT_TRUE(watcher->Start(std::move(opts)).ok());
  EXPECT_TRUE(watcher->IsRunning());

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  EXPECT_TRUE(watcher->IsRunning())
      << "watcher thread should survive an idle period (EAGAIN sleep path)";
  EXPECT_EQ(count.load(), 0)
      << "no events should fire when no files were touched";

  watcher->Stop();
  EXPECT_FALSE(watcher->IsRunning());
}

#endif  // defined(__linux__)

}  // namespace
}  // namespace vx::devtool::hot_reload
