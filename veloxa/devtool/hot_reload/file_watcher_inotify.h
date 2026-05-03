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
// Scope: this commit (C.1.1) implements only the bare lifecycle
// (init / add_watch / event loop / fd cleanup). T2 path-traversal guards
// (absolute-path check, realpath canonicalization, boundary check, max
// file size, extension filter, debounce, security log) land in C.1.2.

#ifndef VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_INOTIFY_H_
#define VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_INOTIFY_H_

#if defined(__linux__)

#include <atomic>
#include <thread>

#include "veloxa/devtool/hot_reload/file_watcher.h"
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

 private:
  void WatchLoop();
  void ProcessBuffer(const char* buf, isize len);

  int inotify_fd_ = -1;
  int watch_descriptor_ = -1;
  std::thread watch_thread_;
  std::atomic<bool> running_{false};
  WatchOptions opts_;
};

}  // namespace vx::devtool::hot_reload

#endif  // defined(__linux__)
#endif  // VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_INOTIFY_H_
