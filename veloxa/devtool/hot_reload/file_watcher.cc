// TASK-20260503-01 C.0.1: FileWatcher::CreatePlatform() factory.
//
// On Linux returns an internal placeholder (StubWatcher) that satisfies the
// FileWatcher contract without performing any inotify syscalls. C.1.1 will
// replace the Linux branch with a real InotifyFileWatcher (separate header)
// — the StubWatcher exists solely so the abstract base + factory can land
// as an independent commit per plan B6 (one commit per subtask). Non-Linux
// platforms return nullptr; macOS kqueue / Windows ReadDirectoryChangesW
// remain P2 candidates.

#include "veloxa/devtool/hot_reload/file_watcher.h"

#include <utility>

namespace vx::devtool::hot_reload {

#if defined(__linux__)
namespace {

class StubWatcher : public FileWatcher {
 public:
  Status Start(WatchOptions /*opts*/) override {
    return Status(StatusCode::kInternal,
                  "FileWatcher::Start unimplemented (C.0.1 placeholder; real "
                  "InotifyFileWatcher arrives in C.1.1)");
  }
  void Stop() override {}
  bool IsRunning() const override { return false; }
};

}  // namespace
#endif  // defined(__linux__)

std::unique_ptr<FileWatcher> FileWatcher::CreatePlatform() {
#if defined(__linux__)
  return std::make_unique<StubWatcher>();
#else
  return nullptr;
#endif
}

}  // namespace vx::devtool::hot_reload
