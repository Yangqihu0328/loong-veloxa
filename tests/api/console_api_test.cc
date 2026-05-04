// TASK-20260503-04 D.4 — Public C ABI tests for vx_view_eval_console +
// vx_view_console_log_drain. Behavior depends on VX_BUILD_DEVTOOL:
//   ON  → real eval / drain, lazy-attach guard before vx_view_attach_devtool
//   OFF → both APIs return VX_ERROR_INVALID_STATE without touching engine
//
// Coverage matrix:
//   D.4-T1  null-param guards (view / source / out_buf / out_len)
//   D.4-T2  lazy-attach: pre-attach call returns NOT_ATTACHED + empty envelope
//   D.4-T3  attached round-trip: eval "1+2" → "3" + status OK
//   D.4-T4  T1 mitigation: while(true){} → INTERRUPTED + WasInterrupted true
//   D.4-T5  drain round-trip: console.log via JS native binding → drain JSON
//   D.4-T6  drain double-call: too-small buffer returns OUT_OF_MEMORY +
//           *out_len reports needed size (matches vx_view_serialize_dom_json
//           T7 protocol).

#include "veloxa/api/veloxa_api.h"

#include <cstdint>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

namespace {

struct ViewFixture {
  VxEventLoop* loop = nullptr;
  VxSurface* surface = nullptr;
  VxView* view = nullptr;

  ViewFixture() {
    loop = vx_event_loop_create_headless();
    surface = vx_surface_create_memory(200, 200);
    VxViewConfig config{loop, surface, 60, 0xFFFFFFFF};
    view = vx_view_create(&config);
    vx_view_load_html(view, "<html><body>x</body></html>",
                      static_cast<uint32_t>(27));
  }
  ~ViewFixture() {
    vx_view_destroy(view);
    vx_surface_destroy(surface);
    vx_event_loop_destroy(loop);
  }
};

// D.4-T1
TEST(ConsoleApiTest, EvalConsoleNullParams) {
  char buf[64];
  vx_console_eval_status status = VX_CONSOLE_EVAL_OK;
  EXPECT_EQ(vx_view_eval_console(nullptr, "1", 1, buf, sizeof(buf), &status),
            VX_ERROR_NULL_PARAM);
  ViewFixture v;
  EXPECT_EQ(vx_view_eval_console(v.view, nullptr, 0, buf, sizeof(buf), &status),
            VX_ERROR_NULL_PARAM);
  EXPECT_EQ(vx_view_eval_console(v.view, "1", 1, nullptr, 0, &status),
            VX_ERROR_NULL_PARAM);
}

TEST(ConsoleApiTest, DrainNullParams) {
  char buf[64];
  uint32_t out_len = 0;
  EXPECT_EQ(vx_view_console_log_drain(nullptr, buf, sizeof(buf), &out_len),
            VX_ERROR_NULL_PARAM);
  ViewFixture v;
  EXPECT_EQ(vx_view_console_log_drain(v.view, nullptr, 0, &out_len),
            VX_ERROR_NULL_PARAM);
  EXPECT_EQ(vx_view_console_log_drain(v.view, buf, sizeof(buf), nullptr),
            VX_ERROR_NULL_PARAM);
}

// D.4-T2 — lazy-attach: BEFORE vx_view_attach_devtool, both APIs return
// INVALID_STATE on DEVTOOL=ON and DEVTOOL=OFF alike. The empty envelope
// + NOT_ATTACHED status is the canonical "no-op" payload so embedders
// can JSON.parse / branch on status uniformly across builds.
TEST(ConsoleApiTest, LazyAttachReturnsNotAttached) {
  ViewFixture v;
  char buf[256];
  vx_console_eval_status status = VX_CONSOLE_EVAL_OK;
  VxResult r = vx_view_eval_console(v.view, "1+2", 3, buf, sizeof(buf),
                                     &status);
  EXPECT_EQ(r, VX_ERROR_INVALID_STATE);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_NOT_ATTACHED);
  EXPECT_STREQ(buf, "");

  uint32_t out_len = 0;
  r = vx_view_console_log_drain(v.view, buf, sizeof(buf), &out_len);
  EXPECT_EQ(r, VX_ERROR_INVALID_STATE);
  // Empty envelope written so JSON.parse never throws on the JS side.
  EXPECT_NE(std::strstr(buf, "\"messages\":[]"), nullptr);
  EXPECT_NE(std::strstr(buf, "\"truncated\":false"), nullptr);
  EXPECT_NE(std::strstr(buf, "\"dropped_count\":0"), nullptr);
}

#ifdef VX_BUILD_DEVTOOL

// D.4-T3 — attached round-trip arithmetic.
TEST(ConsoleApiTest, AttachedEvalRoundTripArithmetic) {
  ViewFixture v;
  ASSERT_EQ(vx_view_attach_devtool(v.view, nullptr), VX_OK);
  ASSERT_EQ(vx_view_devtool_loaded(v.view), 1);

  char buf[256];
  vx_console_eval_status status = VX_CONSOLE_EVAL_INTERRUPTED;  // poison
  VxResult r = vx_view_eval_console(v.view, "1+2", 3, buf, sizeof(buf),
                                     &status);
  EXPECT_EQ(r, VX_OK);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_OK);
  EXPECT_STREQ(buf, "3");
}

// D.4-T4 — T1 mitigation default-safe budget aborts infinite loop.
TEST(ConsoleApiTest, AttachedEvalInterruptedByDefaultBudget) {
  ViewFixture v;
  ASSERT_EQ(vx_view_attach_devtool(v.view, nullptr), VX_OK);

  char buf[256];
  vx_console_eval_status status = VX_CONSOLE_EVAL_OK;  // poison
  const char src[] = "while (true) {}";
  VxResult r = vx_view_eval_console(v.view, src,
                                     static_cast<uint32_t>(sizeof(src) - 1),
                                     buf, sizeof(buf), &status);
  // VxResult is OK because the eval *ran* (and aborted); the rich status
  // surfaces the INTERRUPTED diagnosis.
  EXPECT_EQ(r, VX_OK);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_INTERRUPTED);
  EXPECT_NE(std::strstr(buf, "interrupt"), nullptr);
}

// D.4-T5 — console.log push then drain via the JS-side console.log;
// verifies the host data channel (DomBindings.SetConsoleLogBuffer)
// exposes the same buffer to the C ABI drain that panel JS would see.
TEST(ConsoleApiTest, AttachedDrainPicksUpConsoleLog) {
  ViewFixture v;
  ASSERT_EQ(vx_view_attach_devtool(v.view, nullptr), VX_OK);

  // Push from the C side using vx_view_eval_console — Console scope sees
  // the host-isolated runtime where RegisterConsoleBindings registered
  // console.log → ConsoleLogBuffer push.
  char eval_buf[64];
  vx_console_eval_status status = VX_CONSOLE_EVAL_OK;
  const char src[] = "console.log('round-trip-payload'); 'pushed';";
  VxResult er = vx_view_eval_console(v.view, src,
                                      static_cast<uint32_t>(sizeof(src) - 1),
                                      eval_buf, sizeof(eval_buf), &status);
  EXPECT_EQ(er, VX_OK);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_OK);

  char drain_buf[1024];
  uint32_t out_len = 0;
  VxResult dr = vx_view_console_log_drain(v.view, drain_buf, sizeof(drain_buf),
                                           &out_len);
  EXPECT_EQ(dr, VX_OK);
  EXPECT_GT(out_len, 0u);
  EXPECT_NE(std::strstr(drain_buf, "round-trip-payload"), nullptr);
  EXPECT_NE(std::strstr(drain_buf, "\"level\":\"log\""), nullptr);

  // Second drain immediately after the first: the buffer must have been
  // cleared (atomic drain semantic — see D.2-T5).
  uint32_t out_len2 = 0;
  VxResult dr2 = vx_view_console_log_drain(v.view, drain_buf,
                                            sizeof(drain_buf), &out_len2);
  EXPECT_EQ(dr2, VX_OK);
  EXPECT_NE(std::strstr(drain_buf, "\"messages\":[]"), nullptr);
}

// D.4-T6 — drain double-call: tiny buffer reports needed size.
TEST(ConsoleApiTest, AttachedDrainTooSmallReportsNeededSize) {
  ViewFixture v;
  ASSERT_EQ(vx_view_attach_devtool(v.view, nullptr), VX_OK);
  // Push a single message so the envelope is > 50 bytes (empty envelope
  // is exactly 53 bytes; with one message it grows by ~80 bytes).
  char eval_buf[64];
  vx_console_eval_status status = VX_CONSOLE_EVAL_OK;
  const char src[] = "console.log('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'); 0;";
  ASSERT_EQ(vx_view_eval_console(v.view, src,
                                  static_cast<uint32_t>(sizeof(src) - 1),
                                  eval_buf, sizeof(eval_buf), &status),
            VX_OK);

  char tiny[16];
  uint32_t needed = 0;
  VxResult r = vx_view_console_log_drain(v.view, tiny, sizeof(tiny), &needed);
  EXPECT_EQ(r, VX_ERROR_OUT_OF_MEMORY);
  EXPECT_GT(needed, sizeof(tiny));
  EXPECT_STREQ(tiny, "");  // empty NUL-terminated string on overflow
}

#else  // VX_BUILD_DEVTOOL OFF

// D.4-T-OFF — A14 stub guard: even with a fully-set-up view the OFF stub
// returns INVALID_STATE without touching DevTool symbols.
TEST(ConsoleApiTest, OffStubReturnsInvalidState) {
  ViewFixture v;
  char buf[256];
  vx_console_eval_status status = VX_CONSOLE_EVAL_OK;
  VxResult r = vx_view_eval_console(v.view, "1+2", 3, buf, sizeof(buf),
                                     &status);
  EXPECT_EQ(r, VX_ERROR_INVALID_STATE);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_NOT_ATTACHED);
}

#endif  // VX_BUILD_DEVTOOL

}  // namespace
