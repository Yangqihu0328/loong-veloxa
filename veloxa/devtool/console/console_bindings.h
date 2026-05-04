#ifndef VELOXA_DEVTOOL_CONSOLE_CONSOLE_BINDINGS_H_
#define VELOXA_DEVTOOL_CONSOLE_CONSOLE_BINDINGS_H_

#include <deque>
#include <string>

#include "veloxa/foundation/base/types.h"

struct JSContext;

namespace vx::devtool::console {

// TASK-20260503-04 D.2 — One log entry pushed by Console JS via
// console.log/error/warn. ts_ms is host time (std::chrono::system_clock
// unix-epoch milliseconds), chosen for cross-correlation with OS logs;
// see creative §C4 ts_ms time source rationale.
struct ConsoleLogEntry {
  enum class Level : u8 { kLog = 0, kError = 1, kWarn = 2 };
  Level level = Level::kLog;
  std::string text;
  u64 ts_ms = 0;
};

// Bounded ring of console log entries with two independent caps:
//
//   kMaxCount      — at most 1000 in-flight entries; on overflow the
//                    oldest is dropped + dropped_count_ increments. The
//                    counter is reported in the next drain envelope and
//                    reset by Clear() (called from DrainAsJson).
//   kMaxTextBytes  — at most 4096 bytes per text; longer payloads are
//                    truncated at the closest UTF-8 boundary ≤ cap-suffix
//                    and "... [truncated]" is appended. A single drain's
//                    `truncated` flag flips true if any entry currently in
//                    the deque was truncated at push time.
//
// Both caps mitigate T6 (buffer growth) and serve as belt-and-braces for
// T1 (untrusted Console eval emitting megabytes of console.log spam).
//
// Single-threaded contract: Push runs on the main thread inside the JS
// callback registered by RegisterConsoleBindings; Drain runs on the main
// thread inside the JS poll callback (D.3). No mutex needed.
class ConsoleLogBuffer {
 public:
  static constexpr usize kMaxCount = 1000;
  static constexpr usize kMaxTextBytes = 4096;

  void Push(ConsoleLogEntry::Level lvl, std::string text);

  // Serialise all pending entries into the JSON envelope (creative §C4)
  // and clear the buffer + counters atomically (single-threaded → no
  // observable interleaving with concurrent Push). Returns the JSON.
  std::string DrainAsJson();

  // Reset entries + counters without producing JSON. Mainly for tests.
  void Clear();

  usize size() const { return entries_.size(); }
  u32 dropped_count() const { return dropped_count_; }
  bool truncated() const { return truncated_; }

 private:
  std::deque<ConsoleLogEntry> entries_;
  u32 dropped_count_ = 0;
  bool truncated_ = false;
};

// Install the Console capability allowlist on `ctx`:
//
//   globalThis.console.log/error/warn   — push to *buffer
//   globalThis.vx_devtool_get_console_log_drain() — return JSON envelope
//
// Pre-condition: caller has NOT used JS_SetContextOpaque on ctx for any
// other purpose; we use that slot to recover *buffer inside the callbacks
// (creative §C3 path 2 — Console runtime does not share opaque with
// devtool_engine's DomBindings). RegisterConsoleBindings calls
// JS_SetContextOpaque(ctx, buffer) on entry.
//
// What this MUST NOT install (capability allowlist — spec §11.1):
//   - Element / document / DOM mutators (no DomBindings::Bind here)
//   - vx_view_attach_devtool / hot_reload_dir setter (no Hot Reload trigger)
//   - file / network bindings (auto-satisfied — js_std_add_helpers not used)
void RegisterConsoleBindings(JSContext* ctx, ConsoleLogBuffer* buffer);

}  // namespace vx::devtool::console

#endif  // VELOXA_DEVTOOL_CONSOLE_CONSOLE_BINDINGS_H_
