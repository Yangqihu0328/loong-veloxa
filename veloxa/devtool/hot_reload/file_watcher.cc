// TASK-20260503-01 C.0.1 + C.1.1: FileWatcher::CreatePlatform() factory.
//
// On Linux returns a real InotifyFileWatcher (sys/inotify.h + std::thread
// worker, see file_watcher_inotify.h) — the C.0.1 StubWatcher placeholder
// has been retired in C.1.1 now that the real backend exists. Non-Linux
// platforms still return nullptr; macOS kqueue / Windows
// ReadDirectoryChangesW remain P2 candidates.

#include "veloxa/devtool/hot_reload/file_watcher.h"

#if defined(__linux__)
#include "veloxa/devtool/hot_reload/file_watcher_inotify.h"
#endif

namespace vx::devtool::hot_reload {

std::unique_ptr<FileWatcher> FileWatcher::CreatePlatform() {
#if defined(__linux__)
  return std::make_unique<InotifyFileWatcher>();
#else
  return nullptr;
#endif
}

}  // namespace vx::devtool::hot_reload
