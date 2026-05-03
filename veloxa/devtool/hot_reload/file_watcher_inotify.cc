// TASK-20260503-01 C.1.1: InotifyFileWatcher Linux backend.
//
// Bare lifecycle implementation — Start() opens an inotify fd in
// non-blocking + close-on-exec mode, registers the root with the locked
// IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO mask, and spawns a worker
// thread running WatchLoop(). Stop() flips the running_ flag, joins the
// thread, removes the watch and closes the fd. WatchLoop() drains the
// inotify buffer with read() and dispatches each event to ProcessBuffer
// which constructs root_dir/<name> and invokes the user callback.
//
// What this commit does NOT do (deferred to C.1.2):
//   - absolute-path enforcement on opts.root_dir
//   - realpath() canonicalization and symlink-escape rejection
//   - boundary check that resolved paths stay under the canonical root
//   - max_file_size / extension-filter / debounce / security log
//
// Those eight T2 path-traversal guards arrive together in C.1.2 with
// 1:1 unit-test coverage (plan §3.3 table) plus reverse probes.

#if defined(__linux__)

#include "veloxa/devtool/hot_reload/file_watcher_inotify.h"

#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <utility>

#include "veloxa/foundation/strings/string.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::devtool::hot_reload {
namespace {

constexpr u32 kInotifyMask = IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO;

u64 NowMillis() {
  using namespace std::chrono;
  return static_cast<u64>(
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
          .count());
}

}  // namespace

InotifyFileWatcher::~InotifyFileWatcher() { Stop(); }

Status InotifyFileWatcher::Start(WatchOptions opts) {
  if (running_.load(std::memory_order_acquire)) {
    return Status(StatusCode::kAlreadyExists,
                  "InotifyFileWatcher already running");
  }

  inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (inotify_fd_ < 0) {
    return Status(StatusCode::kInternal, "inotify_init1 failed");
  }

  watch_descriptor_ =
      inotify_add_watch(inotify_fd_, opts.root_dir.c_str(), kInotifyMask);
  if (watch_descriptor_ < 0) {
    int err = errno;
    close(inotify_fd_);
    inotify_fd_ = -1;
    StatusCode code = (err == ENOENT || err == ENOTDIR)
                          ? StatusCode::kNotFound
                          : StatusCode::kInternal;
    return Status(code, "inotify_add_watch failed");
  }

  opts_ = std::move(opts);
  running_.store(true, std::memory_order_release);
  watch_thread_ = std::thread([this]() { WatchLoop(); });
  return Status::Ok();
}

void InotifyFileWatcher::Stop() {
  // exchange returns the previous value; if it was already false we're
  // not running and there is no thread / fd / wd to clean up.
  if (!running_.exchange(false, std::memory_order_acq_rel)) {
    return;
  }
  if (watch_thread_.joinable()) {
    watch_thread_.join();
  }
  if (watch_descriptor_ >= 0) {
    inotify_rm_watch(inotify_fd_, watch_descriptor_);
    watch_descriptor_ = -1;
  }
  if (inotify_fd_ >= 0) {
    close(inotify_fd_);
    inotify_fd_ = -1;
  }
  opts_ = WatchOptions{};
}

void InotifyFileWatcher::WatchLoop() {
  alignas(struct inotify_event) char buf[4096];
  while (running_.load(std::memory_order_acquire)) {
    isize len = read(inotify_fd_, buf, sizeof(buf));
    if (len <= 0) {
      // EAGAIN/EWOULDBLOCK on a non-blocking fd just means "no events
      // ready"; sleep briefly and retry so the thread doesn't spin.
      // Other errors (e.g. EBADF after Stop closes the fd) terminate
      // the loop — the running_ flag will already be false in that path.
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      break;
    }
    ProcessBuffer(buf, len);
  }
}

void InotifyFileWatcher::ProcessBuffer(const char* buf, isize len) {
  if (!opts_.callback) return;
  isize offset = 0;
  while (offset + static_cast<isize>(sizeof(struct inotify_event)) <= len) {
    const auto* event =
        reinterpret_cast<const struct inotify_event*>(buf + offset);
    isize record_len =
        static_cast<isize>(sizeof(struct inotify_event)) + event->len;
    if (offset + record_len > len) break;

    if (event->len > 0) {
      String full_path(opts_.root_dir);
      full_path += StringView("/");
      full_path += StringView(event->name);

      FileChangeEvent fce;
      fce.path = std::move(full_path);
      fce.kind = FileChangeKind::kModified;
      fce.timestamp_ms = NowMillis();
      opts_.callback(fce);
    }

    offset += record_len;
  }
}

}  // namespace vx::devtool::hot_reload

#endif  // defined(__linux__)
