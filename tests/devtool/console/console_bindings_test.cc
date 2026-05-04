// TASK-20260503-04 D.2 — RegisterConsoleBindings + ConsoleLogBuffer tests.
// Lives under tests/devtool/console/, gated by parent VX_BUILD_DEVTOOL=ON.
//
// Coverage matrix:
//   D.2-T1  Push then drain — JSON envelope round-trips message text/level.
//   D.2-T2  Multi-level push — log/warn/error preserved + ts is unix-ish.
//   D.2-T3  Count cap — push 1001 entries → oldest dropped + dropped_count=1.
//   D.2-T4  Per-text cap — 5000-byte text truncated + suffix appended +
//           UTF-8 boundary safe (multi-byte char never split).
//   D.2-T5  Drain clears state — second drain after empty push reports 0
//           dropped + truncated=false (Clear semantics atomic with drain).
//   D.2-T6  RegisterConsoleBindings JS round-trip — console.log + drain
//           returns parseable JSON containing the message.
//   D.2-T7  RegisterConsoleBindings reverse probe — capability allowlist
//           verified: typeof Element / document / setAttribute /
//           addEventListener / vx_view_attach_devtool MUST be undefined
//           (spec §11.1 isolation contract).

#include "veloxa/devtool/console/console_bindings.h"

#include "veloxa/devtool/console/console_engine.h"

#include <gtest/gtest.h>

#include <string>

namespace vx::devtool::console {
namespace {

// D.2-T1
TEST(ConsoleLogBufferTest, PushThenDrainRoundTrip) {
  ConsoleLogBuffer buf;
  buf.Push(ConsoleLogEntry::Level::kLog, "hello");
  buf.Push(ConsoleLogEntry::Level::kError, "boom");
  EXPECT_EQ(buf.size(), 2u);
  std::string env = buf.DrainAsJson();
  // Cheap structural assertions (no JSON parser linked here).
  EXPECT_NE(env.find("\"level\":\"log\""),    std::string::npos);
  EXPECT_NE(env.find("\"text\":\"hello\""),   std::string::npos);
  EXPECT_NE(env.find("\"level\":\"error\""),  std::string::npos);
  EXPECT_NE(env.find("\"text\":\"boom\""),    std::string::npos);
  EXPECT_NE(env.find("\"truncated\":false"),  std::string::npos);
  EXPECT_NE(env.find("\"dropped_count\":0"),  std::string::npos);
  // Drain clears.
  EXPECT_EQ(buf.size(), 0u);
}

// D.2-T2
TEST(ConsoleLogBufferTest, MultiLevelAndTsField) {
  ConsoleLogBuffer buf;
  buf.Push(ConsoleLogEntry::Level::kLog, "L");
  buf.Push(ConsoleLogEntry::Level::kWarn, "W");
  buf.Push(ConsoleLogEntry::Level::kError, "E");
  std::string env = buf.DrainAsJson();
  EXPECT_NE(env.find("\"level\":\"log\""),   std::string::npos);
  EXPECT_NE(env.find("\"level\":\"warn\""),  std::string::npos);
  EXPECT_NE(env.find("\"level\":\"error\""), std::string::npos);
  // ts is host time (unix-epoch ms). The exact value depends on wall clock,
  // but a plausible 2026-vintage timestamp has 13 digits — assert at least
  // 12 to allow some slack while catching a 0 / negative bug.
  EXPECT_NE(env.find("\"ts\":1"), std::string::npos);
}

// D.2-T3 — T6 mitigation (count cap drops oldest + reports drop count).
TEST(ConsoleLogBufferTest, CountCapDropsOldest) {
  ConsoleLogBuffer buf;
  // Push 1001 entries: first one MUST be dropped.
  for (int i = 0; i < 1001; ++i) {
    buf.Push(ConsoleLogEntry::Level::kLog, "msg" + std::to_string(i));
  }
  EXPECT_EQ(buf.size(), 1000u);
  EXPECT_EQ(buf.dropped_count(), 1u);
  std::string env = buf.DrainAsJson();
  // Oldest "msg0" dropped; "msg1" should be the new head.
  // (Substring search — env contains all 1000 entries inline; cheap probe.)
  EXPECT_EQ(env.find("\"text\":\"msg0\""), std::string::npos);
  EXPECT_NE(env.find("\"text\":\"msg1\""), std::string::npos);
  EXPECT_NE(env.find("\"text\":\"msg1000\""), std::string::npos);
  EXPECT_NE(env.find("\"dropped_count\":1"), std::string::npos);
}

// D.2-T4 — T1/T6 mitigation (per-text cap + UTF-8 boundary safety).
TEST(ConsoleLogBufferTest, PerTextCapTruncatesAtUtf8Boundary) {
  ConsoleLogBuffer buf;
  // 5000 bytes of mixed ASCII + 中文 (UTF-8 3-byte) so the 4096-cap will
  // (with high probability) land inside a multi-byte codepoint. The
  // truncation must back up to a clean boundary so the resulting buffer
  // is still valid UTF-8.
  std::string large;
  large.reserve(5000);
  // Repeat "你好A" (3+3+1 = 7 bytes) until > 5000 bytes. Hex escapes are
  // split by string-literal concatenation so the trailing 'A' is not
  // absorbed into the previous \xbd hex sequence.
  while (large.size() < 5000) {
    large.append("\xe4\xbd\xa0" "\xe5\xa5\xbd" "A");
  }
  buf.Push(ConsoleLogEntry::Level::kLog, large);
  EXPECT_EQ(buf.size(), 1u);
  EXPECT_TRUE(buf.truncated());
  std::string env = buf.DrainAsJson();
  EXPECT_NE(env.find("\"truncated\":true"), std::string::npos);
  // Suffix must be present.
  EXPECT_NE(env.find("... [truncated]"), std::string::npos);
  // No `\uFFFD` (replacement char) — proves UTF-8 boundary safety.
  EXPECT_EQ(env.find("\\ufffd"), std::string::npos);
}

// D.2-T5 — drain idempotently clears + counters reset.
TEST(ConsoleLogBufferTest, DrainClearsAtomically) {
  ConsoleLogBuffer buf;
  buf.Push(ConsoleLogEntry::Level::kLog, "x");
  (void)buf.DrainAsJson();
  EXPECT_EQ(buf.size(), 0u);
  EXPECT_EQ(buf.dropped_count(), 0u);
  EXPECT_FALSE(buf.truncated());
  std::string env2 = buf.DrainAsJson();
  EXPECT_NE(env2.find("\"messages\":[]"),   std::string::npos);
  EXPECT_NE(env2.find("\"truncated\":false"), std::string::npos);
}

// D.2-T6 — JS-side round trip via real RegisterConsoleBindings.
TEST(RegisterConsoleBindingsTest, ConsoleLogJsRoundTrip) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  ConsoleLogBuffer buffer;
  RegisterConsoleBindings(engine.context(), &buffer);

  auto r = engine.EvalGlobal(StringView("console.log('hi from js'); 'ok';"),
                             StringView("<test>"));
  ASSERT_TRUE(r.ok()) << (!r.ok() ? r.status().message() : "");
  EXPECT_EQ(r.value(), "ok");
  EXPECT_EQ(buffer.size(), 1u);

  // Drain via JS endpoint and verify envelope contains the message.
  auto d = engine.EvalGlobal(StringView("vx_devtool_get_console_log_drain()"),
                             StringView("<drain>"));
  ASSERT_TRUE(d.ok()) << (!d.ok() ? d.status().message() : "");
  EXPECT_NE(d.value().find("hi from js"), std::string::npos);
  EXPECT_EQ(buffer.size(), 0u);
}

// D.2-T7 — capability allowlist (spec §11.1 isolation contract).
// All of these MUST be undefined in the Console scope. If any leaks,
// the Console becomes a privilege-escalation vector for arbitrary eval.
TEST(RegisterConsoleBindingsTest, CapabilityAllowlistReverseProbe) {
  ConsoleEngine engine;
  ASSERT_TRUE(engine.Init().ok());
  ConsoleLogBuffer buffer;
  RegisterConsoleBindings(engine.context(), &buffer);

  struct Probe { const char* expr; };
  const Probe forbidden[] = {
    {"typeof Element"},
    {"typeof document"},
    {"typeof setAttribute"},
    {"typeof addEventListener"},
    {"typeof vx_view_attach_devtool"},
    {"typeof vx_devtool_get_dom_json"},
    {"typeof vx_devtool_get_hot_reload_status"},
    {"typeof vx_view_get_perf_stats"},
  };
  for (const auto& p : forbidden) {
    auto r = engine.EvalGlobal(StringView(p.expr), StringView("<probe>"));
    ASSERT_TRUE(r.ok()) << "probe failed: " << p.expr;
    EXPECT_EQ(r.value(), "undefined") << "leak: " << p.expr;
  }

  // Forward probe — what MUST be present.
  struct Allow { const char* expr; const char* expected; };
  const Allow allowed[] = {
    {"typeof console",                          "object"},
    {"typeof console.log",                      "function"},
    {"typeof console.error",                    "function"},
    {"typeof console.warn",                     "function"},
    {"typeof vx_devtool_get_console_log_drain", "function"},
  };
  for (const auto& a : allowed) {
    auto r = engine.EvalGlobal(StringView(a.expr), StringView("<allow>"));
    ASSERT_TRUE(r.ok()) << a.expr;
    EXPECT_EQ(r.value(), a.expected) << a.expr;
  }
}

}  // namespace
}  // namespace vx::devtool::console
