// TASK-20260503-01 C.1.1 + C.1.2: InotifyFileWatcher Linux backend.
//
// Lifecycle (C.1.1)
//   Start() opens an inotify fd in non-blocking + close-on-exec mode,
//   registers the root with the locked
//   IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO mask, and spawns a worker
//   thread running WatchLoop(). Stop() flips the running_ flag, joins
//   the thread, removes the watch, and closes the fd. WatchLoop() drains
//   the inotify buffer with read() and dispatches each event through
//   ProcessBuffer() and ResolveAndValidate().
//
// T2 path-traversal guards (C.1.2; spec §3.3 1:1)
//   1. Start() rejects non-absolute root_dir.
//   2. inotify mask locked to IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO
//      (kInotifyMask).
//   3. Start() canonicalizes root_dir via realpath(); per-event
//      ResolveAndValidate() canonicalizes root_dir + "/" + filename.
//   4. ResolveAndValidate() asserts the canonical path stays under the
//      canonical root with a strict prefix match.
//   5. ResolveAndValidate() rejects files larger than opts.max_file_size
//      (default 4 MiB; smaller in tests).
//   6. ResolveAndValidate() rejects extensions not present in
//      opts.extensions (empty list = match all).
//   7. ProcessBuffer() emits VX_LOG_WARN on rejection (silent skip only
//      when the file disappeared between the inotify event and our
//      stat/realpath, since that is an expected race).
//   8. ProcessBuffer() coalesces events on the same canonical path
//      within kDebounceMs (50 ms) into a single callback.

#if defined(__linux__)

#include "veloxa/devtool/hot_reload/file_watcher_inotify.h"

#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include "veloxa/foundation/log/log.h"
#include "veloxa/foundation/strings/string.h"
#include "veloxa/foundation/strings/string_view.h"

namespace vx::devtool::hot_reload {
namespace {

constexpr u32 kInotifyMask = IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO;
constexpr u64 kDebounceMs = 50;

u64 NowMillis() {
  using namespace std::chrono;
  return static_cast<u64>(
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
          .count());
}

// RAII for realpath()'s malloc'd return string. POSIX guarantees the
// caller owns the buffer; pairing it with std::unique_ptr<&::free> keeps
// the cleanup explicit even on error paths.
using CanonicalPath = std::unique_ptr<char, decltype(&::free)>;

CanonicalPath RealPath(const char* path) {
  return CanonicalPath(::realpath(path, nullptr), &::free);
}

bool MatchAnyExtension(const char* name,
                       const Vector<String>& extensions) {
  if (extensions.size() == 0) return true;  // empty = match all
  size_t name_len = ::strlen(name);
  for (usize i = 0; i < extensions.size(); ++i) {
    const auto& ext = extensions[i];
    if (name_len >= ext.size() &&
        ::memcmp(name + name_len - ext.size(), ext.data(), ext.size()) ==
            0) {
      return true;
    }
  }
  return false;
}

}  // namespace

InotifyFileWatcher::~InotifyFileWatcher() { Stop(); }

Status InotifyFileWatcher::Start(WatchOptions opts) {
  if (running_.load(std::memory_order_acquire)) {
    return Status(StatusCode::kAlreadyExists,
                  "InotifyFileWatcher already running");
  }

  // STEP 1 — absolute path allowlist.
  if (opts.root_dir.size() == 0 || opts.root_dir.data()[0] != '/') {
    return Status(StatusCode::kInvalidArgument,
                  "watcher root must be an absolute path");
  }

  // STEP 3 — canonicalize root_dir so subsequent boundary checks have a
  // stable, symlink-free reference. realpath() also surfaces ENOENT for
  // a missing root before we touch any inotify resources.
  CanonicalPath canonical_root = RealPath(opts.root_dir.c_str());
  if (!canonical_root) {
    int err = errno;
    return Status(err == ENOENT ? StatusCode::kNotFound : StatusCode::kInternal,
                  "realpath failed on watcher root");
  }
  opts.root_dir = String(canonical_root.get());

  inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (inotify_fd_ < 0) {
    return Status(StatusCode::kInternal, "inotify_init1 failed");
  }

  // STEP 2 — locked inotify mask, see kInotifyMask above.
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
  last_event_ms_.clear();
  running_.store(true, std::memory_order_release);
  watch_thread_ = std::thread([this]() { WatchLoop(); });
  return Status::Ok();
}

void InotifyFileWatcher::Stop() {
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
  last_event_ms_.clear();
}

StatusOr<std::string> InotifyFileWatcher::ResolveAndValidate(
    const char* filename) const {
  if (filename == nullptr || filename[0] == '\0') {
    return Status(StatusCode::kInvalidArgument, "empty filename");
  }

  // STEP 6 — extension filter (cheap, runs first).
  if (!MatchAnyExtension(filename, opts_.extensions)) {
    return Status(StatusCode::kInvalidArgument,
                  "filename does not match any allowed extension");
  }

  // Build root_dir + "/" + filename; tolerate root_dir already ending
  // with '/' so canonical paths from realpath at "/" don't double-up.
  std::string full_path(opts_.root_dir.c_str(), opts_.root_dir.size());
  if (full_path.empty() || full_path.back() != '/') full_path.push_back('/');
  full_path.append(filename);

  // STEP 3 — canonicalize the full path via realpath().
  CanonicalPath canonical = RealPath(full_path.c_str());
  if (!canonical) {
    return Status(StatusCode::kNotFound, "realpath failed (file missing)");
  }

  // STEP 4 — boundary check: canonical path must equal the canonical
  // root or live strictly under it. Compare on a "root + '/'" prefix to
  // avoid the classic `/foo` vs `/foobar` confusion.
  std::string canonical_str(canonical.get());
  std::string root_with_slash(opts_.root_dir.c_str(), opts_.root_dir.size());
  if (root_with_slash.empty() || root_with_slash.back() != '/') {
    root_with_slash.push_back('/');
  }
  if (canonical_str.size() < root_with_slash.size() ||
      canonical_str.compare(0, root_with_slash.size(), root_with_slash) != 0) {
    return Status(StatusCode::kInvalidArgument,
                  "canonical path escapes watcher root");
  }

  // STEP 5 — max_file_size cap.
  struct stat st;
  if (::stat(canonical_str.c_str(), &st) != 0) {
    return Status(StatusCode::kNotFound, "stat failed (file missing)");
  }
  if (static_cast<usize>(st.st_size) > opts_.max_file_size) {
    return Status(StatusCode::kInvalidArgument,
                  "file size exceeds max_file_size");
  }

  return canonical_str;
}

void InotifyFileWatcher::WatchLoop() {
  alignas(struct inotify_event) char buf[4096];
  while (running_.load(std::memory_order_acquire)) {
    isize len = read(inotify_fd_, buf, sizeof(buf));
    if (len <= 0) {
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
      auto resolved = ResolveAndValidate(event->name);
      if (!resolved.ok()) {
        // STEP 7 — log rejections at WARN, but stay quiet on the expected
        // "file already gone by the time we ran realpath/stat" race so
        // editor save patterns (e.g. .swp churn) don't spam the log.
        if (resolved.status().code() != StatusCode::kNotFound) {
          VX_LOG_WARN("Hot Reload: rejected '%s/%s': %s",
                      opts_.root_dir.c_str(), event->name,
                      resolved.status().message().c_str());
        }
      } else {
        const std::string& canonical = resolved.value();
        u64 now_ms = NowMillis();

        // STEP 8 — 50 ms debounce per canonical path.
        auto it = last_event_ms_.find(canonical);
        if (it == last_event_ms_.end() || (now_ms - it->second) >= kDebounceMs) {
          last_event_ms_[canonical] = now_ms;

          FileChangeEvent fce;
          fce.path = String(canonical.c_str());
          fce.kind = FileChangeKind::kModified;
          fce.timestamp_ms = now_ms;
          opts_.callback(fce);
        }
      }
    }

    offset += record_len;
  }
}

}  // namespace vx::devtool::hot_reload

#endif  // defined(__linux__)
