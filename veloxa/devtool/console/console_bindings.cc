#include "veloxa/devtool/console/console_bindings.h"

extern "C" {
#include "quickjs.h"
}

#include <chrono>
#include <cstdio>
#include <cstring>
#include <utility>

namespace vx::devtool::console {

namespace {

constexpr const char* kTruncSuffix = "... [truncated]";
constexpr usize kTruncSuffixLen = 15;  // strlen of kTruncSuffix

// Largest n' ≤ n such that text[0..n') is a valid UTF-8 prefix (does not
// end mid-codepoint). Walks backwards skipping continuation bytes
// (10xxxxxx). When the next lead byte declares a codepoint that would
// extend past n, the partial codepoint is dropped. Defensive: an isolated
// invalid lead byte is treated as length-1 so we never get stuck.
//
// Edge cases:
//   - n == 0                   → 0
//   - n >= text.size()         → text.size()
//   - all-ASCII                → n unchanged
//   - n inside multibyte cp    → start position of that codepoint
usize TruncateAtUtf8Boundary(const std::string& text, usize n) {
  if (n == 0 || n >= text.size()) {
    return n >= text.size() ? text.size() : 0;
  }
  usize i = n;
  while (i > 0) {
    auto b = static_cast<u8>(text[i - 1]);
    if ((b & 0xC0) == 0x80) {  // continuation byte
      i--;
      continue;
    }
    usize lead_pos = i - 1;
    usize expect_len = 1;
    if ((b & 0x80) == 0x00) expect_len = 1;       // 0xxxxxxx ASCII
    else if ((b & 0xE0) == 0xC0) expect_len = 2;  // 110xxxxx
    else if ((b & 0xF0) == 0xE0) expect_len = 3;  // 1110xxxx
    else if ((b & 0xF8) == 0xF0) expect_len = 4;  // 11110xxx
    // else: invalid lead byte — fall through with expect_len=1 (defensive)
    if (lead_pos + expect_len <= n) {
      return n;  // codepoint fits entirely
    }
    return lead_pos;  // drop partial codepoint
  }
  return 0;  // text starts with continuation bytes (invalid UTF-8)
}

void AppendJsonEscaped(const std::string& src, std::string& out) {
  out.reserve(out.size() + src.size() + 8);
  for (unsigned char c : src) {
    switch (c) {
      case '"':  out.append("\\\""); break;
      case '\\': out.append("\\\\"); break;
      case '\n': out.append("\\n"); break;
      case '\r': out.append("\\r"); break;
      case '\t': out.append("\\t"); break;
      case '\b': out.append("\\b"); break;
      case '\f': out.append("\\f"); break;
      default:
        if (c < 0x20) {
          char hex[8];
          std::snprintf(hex, sizeof(hex), "\\u%04x", c);
          out.append(hex);
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
}

u64 NowUnixMillis() {
  return static_cast<u64>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

const char* LevelName(ConsoleLogEntry::Level lvl) {
  switch (lvl) {
    case ConsoleLogEntry::Level::kLog:   return "log";
    case ConsoleLogEntry::Level::kError: return "error";
    case ConsoleLogEntry::Level::kWarn:  return "warn";
  }
  return "log";
}

std::string ToUtf8String(JSContext* ctx, JSValueConst val) {
  const char* s = JS_ToCString(ctx, val);
  if (s == nullptr) return "<error>";
  std::string out(s);
  JS_FreeCString(ctx, s);
  return out;
}

ConsoleLogBuffer* GetBuffer(JSContext* ctx) {
  return static_cast<ConsoleLogBuffer*>(JS_GetContextOpaque(ctx));
}

JSValue ConsoleLogImpl(JSContext* ctx, int argc, JSValueConst* argv,
                       ConsoleLogEntry::Level lvl) {
  auto* buf = GetBuffer(ctx);
  if (buf == nullptr) return JS_UNDEFINED;
  std::string text;
  for (int i = 0; i < argc; ++i) {
    if (i > 0) text.push_back(' ');
    text.append(ToUtf8String(ctx, argv[i]));
  }
  buf->Push(lvl, std::move(text));
  return JS_UNDEFINED;
}

JSValue ConsoleLog(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                   JSValueConst* argv) {
  return ConsoleLogImpl(ctx, argc, argv, ConsoleLogEntry::Level::kLog);
}

JSValue ConsoleError(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                     JSValueConst* argv) {
  return ConsoleLogImpl(ctx, argc, argv, ConsoleLogEntry::Level::kError);
}

JSValue ConsoleWarn(JSContext* ctx, JSValueConst /*this_val*/, int argc,
                    JSValueConst* argv) {
  return ConsoleLogImpl(ctx, argc, argv, ConsoleLogEntry::Level::kWarn);
}

JSValue ConsoleLogDrain(JSContext* ctx, JSValueConst /*this_val*/,
                        int /*argc*/, JSValueConst* /*argv*/) {
  auto* buf = GetBuffer(ctx);
  if (buf == nullptr) {
    static constexpr char kEmpty[] =
        "{\"messages\":[],\"truncated\":false,\"dropped_count\":0}";
    return JS_NewStringLen(ctx, kEmpty, sizeof(kEmpty) - 1);
  }
  std::string env = buf->DrainAsJson();
  return JS_NewStringLen(ctx, env.data(), env.size());
}

}  // namespace

void ConsoleLogBuffer::Push(ConsoleLogEntry::Level lvl, std::string text) {
  // Step 1 — per-text cap (UTF-8 boundary safe).
  if (text.size() > kMaxTextBytes) {
    const usize keep_budget = kMaxTextBytes - kTruncSuffixLen;
    const usize keep = TruncateAtUtf8Boundary(text, keep_budget);
    text.resize(keep);
    text.append(kTruncSuffix, kTruncSuffixLen);
    truncated_ = true;
  }

  // Step 2 — count cap (drop oldest).
  if (entries_.size() >= kMaxCount) {
    entries_.pop_front();
    dropped_count_++;
  }

  // Step 3 — push.
  entries_.push_back(ConsoleLogEntry{lvl, std::move(text), NowUnixMillis()});
}

std::string ConsoleLogBuffer::DrainAsJson() {
  std::string out;
  out.reserve(64 + entries_.size() * 80);
  out.append("{\"messages\":[");
  bool first = true;
  for (const auto& e : entries_) {
    if (!first) out.push_back(',');
    first = false;
    out.append("{\"level\":\"");
    out.append(LevelName(e.level));
    out.append("\",\"text\":\"");
    AppendJsonEscaped(e.text, out);
    out.append("\",\"ts\":");
    char num_buf[32];
    std::snprintf(num_buf, sizeof(num_buf), "%llu",
                  static_cast<unsigned long long>(e.ts_ms));
    out.append(num_buf);
    out.push_back('}');
  }
  out.append("],\"truncated\":");
  out.append(truncated_ ? "true" : "false");
  out.append(",\"dropped_count\":");
  char num_buf[32];
  std::snprintf(num_buf, sizeof(num_buf), "%u", dropped_count_);
  out.append(num_buf);
  out.push_back('}');
  Clear();
  return out;
}

void ConsoleLogBuffer::Clear() {
  entries_.clear();
  dropped_count_ = 0;
  truncated_ = false;
}

void RegisterConsoleBindings(JSContext* ctx, ConsoleLogBuffer* buffer) {
  // Establish the opaque-ptr channel for callback → buffer recovery.
  // Console runtime is independent of devtool_script_engine_ (see
  // creative §C3 path 2 + creative §C2 lifecycle), so this slot is free.
  JS_SetContextOpaque(ctx, buffer);

  JSValue global = JS_GetGlobalObject(ctx);

  // 1. console object with .log / .error / .warn methods.
  JSValue console = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, console, "log",
                    JS_NewCFunction(ctx, ConsoleLog, "log", 1));
  JS_SetPropertyStr(ctx, console, "error",
                    JS_NewCFunction(ctx, ConsoleError, "error", 1));
  JS_SetPropertyStr(ctx, console, "warn",
                    JS_NewCFunction(ctx, ConsoleWarn, "warn", 1));
  JS_SetPropertyStr(ctx, global, "console", console);

  // 2. drain query (called by D.3 console_panel.js poll loop).
  JS_SetPropertyStr(
      ctx, global, "vx_devtool_get_console_log_drain",
      JS_NewCFunction(ctx, ConsoleLogDrain,
                      "vx_devtool_get_console_log_drain", 0));

  // 3. NOTE — vx_console_eval is registered by D.4 alongside the public
  //    C API, since it forwards back into ConsoleEngine::EvalGlobal which
  //    sits at the host layer. Keeping it out of this binding ensures
  //    RegisterConsoleBindings can be unit-tested in isolation without
  //    needing a host eval shim.

  JS_FreeValue(ctx, global);
}

}  // namespace vx::devtool::console
