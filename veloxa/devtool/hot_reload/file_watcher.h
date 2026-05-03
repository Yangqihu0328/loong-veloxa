// TASK-20260503-01 C.0.1: FileWatcher abstract base + Platform factory.
// Cross-platform interface for incremental file system change monitoring.
// Linux backend (InotifyFileWatcher, C.1.1) uses inotify; macOS (kqueue) and
// Windows (ReadDirectoryChangesW) are P2 candidates that currently return
// nullptr from CreatePlatform() to keep DevTool Hot Reload Linux-only in v1.
//
// Threading contract: WatchOptions::callback is invoked on the watcher's
// internal worker thread (per InotifyFileWatcher in C.1.1). Callers must
// marshal events back to the main thread via a thread-safe queue (see
// HotReloadManager in C.2.1).

#ifndef VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_H_
#define VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_H_

#include <functional>
#include <memory>

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/strings/string.h"

namespace vx::devtool::hot_reload {

enum class FileChangeKind : u8 {
  kModified,
  kRenamed,
  kDeleted,
};

struct FileChangeEvent {
  String path;
  FileChangeKind kind = FileChangeKind::kModified;
  u64 timestamp_ms = 0;
};

struct WatchOptions {
  String root_dir;
  Vector<String> extensions;
  usize max_file_size = 4 * 1024 * 1024;
  std::function<void(const FileChangeEvent&)> callback;
};

class FileWatcher {
 public:
  virtual ~FileWatcher() = default;

  virtual Status Start(WatchOptions opts) = 0;
  virtual void Stop() = 0;
  virtual bool IsRunning() const = 0;

  static std::unique_ptr<FileWatcher> CreatePlatform();
};

}  // namespace vx::devtool::hot_reload

#endif  // VELOXA_DEVTOOL_HOT_RELOAD_FILE_WATCHER_H_
