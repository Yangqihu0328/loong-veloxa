// TASK-20260503-01 C.5.1 — T2 path-traversal complete security suite.
//
// Independent dedicated security_test.cc focused on the 8-step T2 guard
// matrix (spec §3.3 + plan §3.3). file_watcher_test.cc already exercises
// each guard inside its broader InotifyFileWatcher behaviour suite (C.1.2,
// 8 tests); this file complements that with a 1:1 dual-probe matrix:
//
//   8 guards × { forward — guard active, attack rejected /
//                          legitimate path accepted }
//             × { reverse — boundary case verifies the guard's behaviour
//                           on the OPPOSITE side of the contract }
//   = 16 tests
//
// The dual-probe pattern locks both sides of each guard so a regression
// (e.g. accidentally widening the inotify mask, or dropping the realpath
// canonicalize step) is loud rather than silent.
//
// All Linux-only; non-Linux platforms skip the suite (Platform factory
// returns nullptr for now per C.0.1 contract).

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

// ---------------------------------------------------------------------------
// Shared helpers — duplicated from file_watcher_test.cc on purpose.
// Extracting to a common header would couple two TUs that intentionally
// keep their own scope; the helpers are ~80 LOC of boilerplate.
// ---------------------------------------------------------------------------

class TempDir {
 public:
  TempDir() {
    char tmpl[] = "/tmp/vx_security_test_XXXXXX";
    char* path = mkdtemp(tmpl);
    if (path != nullptr) path_ = path;
  }
  ~TempDir() {
    if (path_.empty()) return;
    std::string cmd = "rm -rf " + path_ + " 2>/dev/null";
    std::system(cmd.c_str());
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  bool valid() const { return !path_.empty(); }
  const std::string& path() const { return path_; }
  std::string ChildPath(const char* name) const { return path_ + "/" + name; }

  void WriteFile(const char* name, const char* content) const {
    std::ofstream f(ChildPath(name));
    f << content;
  }
  void WriteFileSized(const char* name, size_t bytes, char fill = 'a') const {
    std::ofstream f(ChildPath(name));
    std::string buf(bytes, fill);
    f.write(buf.data(), buf.size());
  }

 private:
  std::string path_;
};

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

// Counts FileChangeEvent invocations and remembers the last path so
// reverse probes can assert "no event for filtered input".
struct EventCounter {
  std::atomic<int> count{0};
  std::mutex mu;
  std::string last_path;

  std::function<void(const FileChangeEvent&)> Callback() {
    return [this](const FileChangeEvent& evt) {
      ++count;
      std::lock_guard<std::mutex> lk(mu);
      last_path = std::string(evt.path.data(), evt.path.size());
    };
  }
};

// Common Start scaffolding for the dual-probe matrix.
WatchOptions MakeOpts(const std::string& root,
                      const std::vector<std::string>& exts,
                      EventCounter* counter) {
  WatchOptions opts;
  opts.root_dir = String(root.c_str());
  for (const auto& e : exts) opts.extensions.push_back(String(e.c_str()));
  opts.callback = counter->Callback();
  return opts;
}

}  // namespace

// =============================================================================
// STEP 1 — Watcher root MUST be an absolute path.
// =============================================================================

TEST(SecurityT2Step1, AbsoluteRootStartsSuccessfully) {
  // Forward — the legitimate input shape (absolute, existing dir) makes
  // it past the allowlist guard and successfully Start()s the watcher.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  EventCounter c;
  InotifyFileWatcher w;
  EXPECT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  EXPECT_TRUE(w.IsRunning());
  w.Stop();
}

TEST(SecurityT2Step1, RelativeRootRejected) {
  // Reverse — non-absolute path attack vector: even if the dir would
  // resolve under cwd, the guard refuses it BEFORE realpath is called
  // (deterministic rejection — embedder must opt in by passing /).
  EventCounter c;
  InotifyFileWatcher w;
  Status s = w.Start(MakeOpts("relative/path", {".css"}, &c));
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kInvalidArgument);
  EXPECT_FALSE(w.IsRunning());
}

// =============================================================================
// STEP 2 — inotify mask is locked to IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO.
// =============================================================================

TEST(SecurityT2Step2, ModifyEventDelivered) {
  // Forward — the locked mask MUST include the modify path that editors
  // produce on save. Sanity check: write triggers the callback.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("a.css", "body{}");
  EventCounter c;
  InotifyFileWatcher w;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  tmp.WriteFile("a.css", "body{color:red}");
  EXPECT_TRUE(WaitUntil([&] { return c.count.load() >= 1; }));
  w.Stop();
}

TEST(SecurityT2Step2, AccessOnlyEventNotDelivered) {
  // Reverse — bare IN_ACCESS (open-for-read) MUST NOT generate events.
  // We can't issue an IN_ACCESS without file content; instead we open
  // the file for READ and verify no callback fires (read events aren't
  // in the locked mask). Catches anyone who later widens the mask to
  // include IN_OPEN/IN_ACCESS by mistake.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("a.css", "body{}");
  EventCounter c;
  InotifyFileWatcher w;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  // Read the file repeatedly — IN_ACCESS would fire ~5 times if mask
  // were widened. With the locked mask none of these should reach the
  // callback.
  for (int i = 0; i < 5; ++i) {
    std::ifstream r(tmp.ChildPath("a.css"));
    std::string buf((std::istreambuf_iterator<char>(r)),
                    std::istreambuf_iterator<char>());
    (void)buf;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  EXPECT_EQ(c.count.load(), 0);
  w.Stop();
}

// =============================================================================
// STEP 3 — realpath() canonicalizes the full path before any boundary check.
// =============================================================================

TEST(SecurityT2Step3, CanonicalizesViaRealpathSucceeds) {
  // Forward — ResolveAndValidate's realpath() returns the canonical
  // absolute form for a regular file inside root.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("style.css", "body{}");
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());

  auto r = w.ResolveAndValidate("style.css");
  ASSERT_TRUE(r.ok());
  // Canonical path lives under the canonicalized tmp dir (mkdtemp's
  // /tmp may itself be a symlink → realpath the root for comparison).
  char* canon_root = ::realpath(tmp.path().c_str(), nullptr);
  ASSERT_NE(canon_root, nullptr);
  EXPECT_EQ(r.value().rfind(canon_root, 0), 0u);
  ::free(canon_root);
  w.Stop();
}

TEST(SecurityT2Step3, NonExistentFileRejectedByRealpath) {
  // Reverse — realpath() fails for a missing path; ResolveAndValidate
  // surfaces this as a non-ok StatusOr instead of forwarding a synthetic
  // path that would skip the boundary check.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());

  auto r = w.ResolveAndValidate("ghost.css");
  EXPECT_FALSE(r.ok());
  w.Stop();
}

// =============================================================================
// STEP 4 — Canonical path MUST start with canonical(root) + "/".
// =============================================================================

TEST(SecurityT2Step4, FileInsideRootPassesBoundaryCheck) {
  // Forward — a regular file directly under root passes boundary check.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("ok.css", "body{}");
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  EXPECT_TRUE(w.ResolveAndValidate("ok.css").ok());
  w.Stop();
}

TEST(SecurityT2Step4, SymlinkPointingOutsideRootRejected) {
  // Reverse — the canonical attack: place a symlink "leak.css" inside
  // the watch root pointing to /etc/passwd. realpath resolves through
  // the symlink → boundary check fails → rejected.
  TempDir tmp;
  TempDir outside;
  ASSERT_TRUE(tmp.valid() && outside.valid());
  outside.WriteFile("secret.css", "body{}");
  // Symlink leak.css → outside/secret.css
  std::string target = outside.ChildPath("secret.css");
  std::string link = tmp.ChildPath("leak.css");
  ASSERT_EQ(::symlink(target.c_str(), link.c_str()), 0)
      << "symlink() failed (errno=" << errno << ")";

  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  auto r = w.ResolveAndValidate("leak.css");
  EXPECT_FALSE(r.ok())
      << "T2 STEP 4 BREACH: symlink escape was NOT rejected (canonical "
         "path = "
      << (r.ok() ? r.value() : std::string("<n/a>")) << ")";
  w.Stop();
}

// =============================================================================
// STEP 5 — max_file_size cap (default 4 MiB).
// =============================================================================

TEST(SecurityT2Step5, FileWithinSizeCapAccepted) {
  // Forward — a small file (1 KiB) is accepted.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFileSized("small.css", 1024);
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  EXPECT_TRUE(w.ResolveAndValidate("small.css").ok());
  w.Stop();
}

TEST(SecurityT2Step5, FileExceedingSizeCapRejected) {
  // Reverse — a 5 MiB file (above the default 4 MiB cap) is rejected.
  // Defends against memory-exhaustion DoS via a runaway CSS file.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFileSized("huge.css", 5 * 1024 * 1024);
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  EXPECT_FALSE(w.ResolveAndValidate("huge.css").ok());
  w.Stop();
}

// =============================================================================
// STEP 6 — extension filter matches WatchOptions::extensions exactly.
// =============================================================================

TEST(SecurityT2Step6, CssExtensionAccepted) {
  // Forward — .css passes the filter (the legitimate Hot Reload input).
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("ok.css", "body{}");
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  EXPECT_TRUE(w.ResolveAndValidate("ok.css").ok());
  w.Stop();
}

TEST(SecurityT2Step6, NonCssExtensionFiltered) {
  // Reverse — .html, .js, .png MUST be filtered. Critical because
  // HotReloadManager's R9 F-025 strict contract relies on this filter
  // to ensure LoadHTML can never be invoked from the watcher path.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  for (const char* name : {"page.html", "app.js", "logo.png"}) {
    tmp.WriteFile(name, "x");
  }
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  EXPECT_FALSE(w.ResolveAndValidate("page.html").ok());
  EXPECT_FALSE(w.ResolveAndValidate("app.js").ok());
  EXPECT_FALSE(w.ResolveAndValidate("logo.png").ok());
  w.Stop();
}

// =============================================================================
// STEP 7 — WARN-level log on every rejection.
// =============================================================================

TEST(SecurityT2Step7, NoWarnOnAcceptedFile) {
  // Forward — a clean accept emits NO WARN. Otherwise routine successful
  // hot reload would spam the embedder's log pipeline.
  CaptureSinkScope sink;
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("good.css", "body{}");
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  tmp.WriteFile("good.css", "body{color:blue}");
  WaitUntil([&] { return c.count.load() >= 1; });
  EXPECT_EQ(sink.sink().CountAtOrAbove(::vx::LogLevel::kWarn), 0u);
  w.Stop();
}

TEST(SecurityT2Step7, WarnEmittedOnRejection) {
  // Reverse — a rejected file (here: oversized) MUST log at WARN so
  // operators can audit the rejection trail.
  CaptureSinkScope sink;
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  // Pre-create a too-large file and trigger via modify so the worker
  // thread runs ResolveAndValidate → boundary fails → STEP 7 logs WARN.
  tmp.WriteFileSized("oversized.css", 5 * 1024 * 1024);
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  // Trigger an event by appending one byte — the modified file is still
  // oversized so STEP 5 rejects → STEP 7 logs WARN.
  {
    std::ofstream f(tmp.ChildPath("oversized.css"), std::ios::app);
    f << 'x';
  }
  // Allow the worker thread time to see the event + log.
  EXPECT_TRUE(WaitUntil(
      [&] { return sink.sink().CountAtOrAbove(::vx::LogLevel::kWarn) >= 1; },
      800));
  EXPECT_EQ(c.count.load(), 0)
      << "Oversized file MUST be rejected (no callback)";
  w.Stop();
}

// =============================================================================
// STEP 8 — 50 ms debounce coalesces rapid-fire events for the same path.
// =============================================================================

TEST(SecurityT2Step8, RapidWritesWithinWindowCoalesced) {
  // Forward — three writes within 50 ms produce ONE callback, not three.
  // Defends LoadCSS from being hammered when an editor flushes a save
  // in multiple syscalls (vim, Sublime Text trample patterns).
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("rapid.css", "body{}");
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  for (int i = 0; i < 3; ++i) {
    tmp.WriteFile("rapid.css", i % 2 == 0 ? "a{}" : "b{}");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  // Wait past the debounce window; counter should be 1, not 3.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(c.count.load(), 1)
      << "Debounce failed — got " << c.count.load() << " callbacks";
  w.Stop();
}

TEST(SecurityT2Step8, WritesBeyondWindowDeliveredSeparately) {
  // Reverse — writes that straddle the 50 ms debounce window MUST yield
  // separate callbacks; otherwise a slow-typing user would see only the
  // first save and silently miss subsequent edits.
  TempDir tmp;
  ASSERT_TRUE(tmp.valid());
  tmp.WriteFile("paced.css", "body{}");
  InotifyFileWatcher w;
  EventCounter c;
  ASSERT_TRUE(w.Start(MakeOpts(tmp.path(), {".css"}, &c)).ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  tmp.WriteFile("paced.css", "body{color:red}");
  WaitUntil([&] { return c.count.load() >= 1; });
  // Sleep well past the 50 ms debounce window.
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  tmp.WriteFile("paced.css", "body{color:blue}");
  EXPECT_TRUE(WaitUntil([&] { return c.count.load() >= 2; }))
      << "Got only " << c.count.load()
      << " callbacks — debounce should have expired";
  w.Stop();
}

#else  // !defined(__linux__)

TEST(SecurityT2Suite, SkippedOnNonLinux) {
  GTEST_SKIP() << "InotifyFileWatcher Linux-only; macOS/Windows tracked as P2";
}

#endif  // defined(__linux__)

}  // namespace vx::devtool::hot_reload
