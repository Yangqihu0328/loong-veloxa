// TASK-20260503-01 C.1.1: InotifyFileWatcher — Linux backend for the
// FileWatcher abstract interface.
//
// Wraps an inotify file descriptor + a worker std::thread that reads
// inotify_event records and forwards them as FileChangeEvent to the
// caller-provided WatchOptions::callback. The watch mask is locked to
// IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO so that atomic-write editors
// (vim, VS Code) trigger a single callback per real edit.
//
// Threading: callback is invoked on the internal worker thread. Stop()
// joins the thread before returning, which guarantees no callback fires
// after Stop() returns.
//
// C.1.1 added the bare lifecycle (init / add_watch / event loop / fd
// cleanup). C.1.2 layers the eight T2 path-traversal guards on top of
// the lifecycle:
//   1. absolute-path allowlist on Start()
//   2. locked inotify mask (already from C.1.1)
//   3. realpath() canonicalization of root_dir + per-event filename
//   4. boundary check that the canonical path stays under the root
//   5. max_file_size cap via stat()
//   6. extension filter against opts.extensions
//   7. WARN log on rejection
//   8. 50 ms per-path debounce
// The shared core of guards 3-6 lives in ResolveAndValidate(), which is
// public so unit tests can drive it directly without depending on
// inotify's symlink-target event semantics.

#ifndef VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_INOTIFY_H_
#define VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_INOTIFY_H_

#if defined(__linux__)

#include <atomic>
#include <string>
#include <thread>
#include <unordered_map>

#include "veloxa/devtool/hot_reload/file_watcher.h"
#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"

namespace vx::devtool::hot_reload {

class InotifyFileWatcher : public FileWatcher {
 public:
  InotifyFileWatcher() = default;
  ~InotifyFileWatcher() override;

  InotifyFileWatcher(const InotifyFileWatcher&) = delete;
  InotifyFileWatcher& operator=(const InotifyFileWatcher&) = delete;

  Status Start(WatchOptions opts) override;
  void Stop() override;

  bool IsRunning() const override {
    return running_.load(std::memory_order_acquire);
  }

  // Public for unit-test access: applies T2 guards 3-6 (realpath, boundary
  // check, max_file_size, extension filter) to a directory-entry name
  // emitted by inotify and returns the canonical path on success or a
  // descriptive Status on rejection. The watcher must be running.
  StatusOr<std::string> ResolveAndValidate(const char* filename) const;

 private:
  void WatchLoop();
  void ProcessBuffer(const char* buf, isize len);

  int inotify_fd_ = -1;
  int watch_descriptor_ = -1;
  std::thread watch_thread_;
  std::atomic<bool> running_{false};
  WatchOptions opts_;
  // 50 ms debounce — last event timestamp keyed by canonical path. Map is
  // touched only from the worker thread, no locking needed. One-time
  // growth bound is the number of distinct files inside the watched root,
  // which is tiny in DevTool usage; periodic GC is a P3 candidate.
  std::unordered_map<std::string, u64> last_event_ms_;
};

}  // namespace vx::devtool::hot_reload

#endif  // defined(__linux__)
#endif  // VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_INOTIFY_H_
