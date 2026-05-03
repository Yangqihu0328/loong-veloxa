// TASK-20260503-01 C.2.1: HotReloadManager — bridge between FileWatcher
// (worker thread) and Application::LoadCSS (main thread).
//
// Threading contract
//   OnFileChanged() runs on the FileWatcher worker thread; it filters
//   the event by extension (.css only — R9 F-025 strict contract) and
//   pushes onto an internal mutex-guarded queue.
//   DrainEvents() runs on the main thread (called once per frame from
//   Application::Update or equivalent); it drains the queue and invokes
//   Application::LoadCSS for each surviving event. Reading the file +
//   parsing CSS happens on the main thread so we never touch
//   Application internals from the worker.
//
// CSS-only strict contract (R9 / F-025 mitigation)
//   The .css filter inside OnFileChanged is a structural guarantee that
//   LoadHTML is never invoked through this path. C.2.1 ships a reverse
//   probe unit test (HtmlFileChangeDoesNotCallLoadCSSorLoadHTML) that
//   would fail if the filter were dropped.
//
// DOM state retention (B5 = A — YAGNI, merged into C.2.1)
//   creative #2 decision 3 originally specified a DomStateSnapshot /
//   RestoreDomState pair to preserve hover/focus/scroll across CSS
//   reload. plan §0.7 grep evidence found those states are not
//   currently persisted in the codebase, so the restore step would be a
//   no-op. The restore call is therefore omitted; if a future task
//   introduces persistent DOM state, it can layer the snapshot logic
//   onto DrainEvents without changing the public API.

#ifndef VELOXA_DEVTOOL_HOT_RELOAD_HOT_RELOAD_MANAGER_H_
#define VELOXA_DEVTOOL_HOT_RELOAD_HOT_RELOAD_MANAGER_H_

#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "veloxa/devtool/hot_reload/file_watcher.h"
#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/base/types.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx {
class Application;
}

namespace vx::devtool::hot_reload {

class HotReloadManager {
 public:
  explicit HotReloadManager(vx::Application* app);
  ~HotReloadManager();

  HotReloadManager(const HotReloadManager&) = delete;
  HotReloadManager& operator=(const HotReloadManager&) = delete;

  // Start watching `root_dir` for .css changes. Stops any existing watch
  // first. Returns the FileWatcher::Start status (e.g. kInvalidArgument
  // for a relative root, kNotFound for a missing directory, etc.).
  Status Attach(StringView root_dir);

  // Stop the watcher (joins the worker thread) and clear pending events.
  // Safe to call multiple times.
  void Detach();

  // Worker-thread context. Filters by .css extension (R9 contract) and
  // queues the event for the next DrainEvents() call.
  void OnFileChanged(const FileChangeEvent& evt);

  // Main-thread context. Drains the queue, reads each file's contents,
  // and invokes Application::LoadCSS for every surviving event.
  void DrainEvents();

  // Number of LoadCSS invocations made over the manager's lifetime.
  usize tracked_count() const { return tracked_count_; }

  bool IsAttached() const { return watcher_ && watcher_->IsRunning(); }

  // Most recent failure message from DrainEvents (file open / read /
  // parse). Empty when there has been no failure or after Detach().
  std::string last_error() const;

 private:
  vx::Application* app_;  // not owned, lifetime managed by caller
  std::unique_ptr<FileWatcher> watcher_;

  std::mutex queue_mu_;
  std::queue<FileChangeEvent> queue_;

  usize tracked_count_ = 0;

  mutable std::mutex error_mu_;
  std::string last_error_;
};

}  // namespace vx::devtool::hot_reload

#endif  // VELOXA_DEVTOOL_HOT_RELOAD_HOT_RELOAD_MANAGER_H_
