// =============================================================================
// devtool_console_dogfood_smoke_test — TASK-20260503-04 D.5
//
// End-to-end smoke for the DevTool Console JS REPL on a headless surface
// (no SDL2). Validates the full Phase D wiring chain:
//
//   1. vx_view_attach_devtool sets up the dual-engine architecture
//      (devtool_script_engine for inspector_panel.js + console_script_engine
//      for user REPL eval) plus the ConsoleLogBuffer.
//   2. C ABI vx_view_eval_console reaches the Console isolated runtime
//      and round-trips a finite expression to its toString().
//   3. console.log inside the Console scope pushes onto the buffer; the
//      C ABI vx_view_console_log_drain returns the JSON envelope mirror
//      of the same data the panel JS would see via
//      vx_devtool_get_console_log_drain in the devtool ctx.
//   4. T1 mitigation: a runaway script triggers the default-safe budget
//      and surfaces VX_CONSOLE_EVAL_INTERRUPTED at the public ABI layer
//      without crashing the engine; subsequent eval still works
//      (reset-per-eval semantic — D.5 Dim 4).
//   5. Detach + re-attach cycle preserves the engine lifecycle contract
//      (no use-after-free on the buffer/engine pair).
//
// Compiled only when VX_BUILD_DEVTOOL=ON (the entire scenario assumes a
// built DevTool subsystem). Mirrors the existing devtool_dogfood_smoke
// test fixture style for consistency with the Phase A/B/C smoke suite.
// =============================================================================

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "veloxa/api/veloxa_api.h"
#include "veloxa/core/application.h"

namespace {

constexpr uint32_t kW = 800, kH = 600;

class DevtoolConsoleDogfoodSmokeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_ = vx_event_loop_create_headless();
    surface_ = vx_surface_create_memory(kW, kH);
    VxViewConfig cfg{};
    cfg.event_loop = loop_;
    cfg.surface = surface_;
    cfg.target_fps = 60;
    view_ = vx_view_create(&cfg);
    app_ = reinterpret_cast<vx::Application*>(view_);

    static constexpr char kHtml[] =
        "<html><body><div id=\"hello\">Hi</div></body></html>";
    vx_view_load_html(view_, kHtml, sizeof(kHtml) - 1);
  }

  void TearDown() override {
    if (view_) vx_view_destroy(view_);
    if (surface_) vx_surface_destroy(surface_);
    if (loop_) vx_event_loop_destroy(loop_);
  }

  VxEventLoop* loop_ = nullptr;
  VxSurface* surface_ = nullptr;
  VxView* view_ = nullptr;
  vx::Application* app_ = nullptr;
};

// D.5-S1 — full chain: attach → eval finite → eval infinite → drain
// (all via the public C ABI; no direct C++ access).
TEST_F(DevtoolConsoleDogfoodSmokeTest, FullChainAttachEvalDrain) {
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
  ASSERT_EQ(vx_view_devtool_loaded(view_), 1);

  // Step 1 — finite eval (default budget never exhausted).
  char eval_buf[256];
  vx_console_eval_status status = VX_CONSOLE_EVAL_INTERRUPTED;  // poison
  const char src[] = "2 * 3";
  EXPECT_EQ(vx_view_eval_console(view_, src,
                                  static_cast<uint32_t>(sizeof(src) - 1),
                                  eval_buf, sizeof(eval_buf), &status),
            VX_OK);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_OK);
  EXPECT_STREQ(eval_buf, "6");

  // Step 2 — console.log push from JS scope.
  const char push_src[] = "console.log('smoke-payload-A'); 'pushed';";
  status = VX_CONSOLE_EVAL_INTERRUPTED;
  EXPECT_EQ(vx_view_eval_console(view_, push_src,
                                  static_cast<uint32_t>(sizeof(push_src) - 1),
                                  eval_buf, sizeof(eval_buf), &status),
            VX_OK);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_OK);
  EXPECT_STREQ(eval_buf, "pushed");

  // Step 3 — infinite loop trips the budget; engine stays alive.
  const char loop_src[] = "while (true) { Math.random(); }";
  status = VX_CONSOLE_EVAL_OK;
  EXPECT_EQ(vx_view_eval_console(view_, loop_src,
                                  static_cast<uint32_t>(sizeof(loop_src) - 1),
                                  eval_buf, sizeof(eval_buf), &status),
            VX_OK);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_INTERRUPTED);

  // Step 4 — recovery: a finite eval after an INTERRUPTED must succeed
  // (reset-per-eval semantic survives the C ABI trip).
  const char recover_src[] = "console.log('smoke-payload-B'); 'recovered';";
  status = VX_CONSOLE_EVAL_INTERRUPTED;
  EXPECT_EQ(vx_view_eval_console(view_, recover_src,
                                  static_cast<uint32_t>(sizeof(recover_src) - 1),
                                  eval_buf, sizeof(eval_buf), &status),
            VX_OK);
  EXPECT_EQ(status, VX_CONSOLE_EVAL_OK);
  EXPECT_STREQ(eval_buf, "recovered");

  // Step 5 — drain: the JSON envelope must contain BOTH push payloads
  // (the failed/interrupted eval did not push anything).
  char drain_buf[2048];
  uint32_t out_len = 0;
  ASSERT_EQ(vx_view_console_log_drain(view_, drain_buf, sizeof(drain_buf),
                                       &out_len),
            VX_OK);
  EXPECT_NE(std::strstr(drain_buf, "smoke-payload-A"), nullptr);
  EXPECT_NE(std::strstr(drain_buf, "smoke-payload-B"), nullptr);
  EXPECT_NE(std::strstr(drain_buf, "\"level\":\"log\""), nullptr);
  EXPECT_NE(std::strstr(drain_buf, "\"truncated\":false"), nullptr);
  EXPECT_NE(std::strstr(drain_buf, "\"dropped_count\":0"), nullptr);

  // Step 6 — drain is destructive: a second drain returns empty envelope.
  out_len = 0;
  ASSERT_EQ(vx_view_console_log_drain(view_, drain_buf, sizeof(drain_buf),
                                       &out_len),
            VX_OK);
  EXPECT_NE(std::strstr(drain_buf, "\"messages\":[]"), nullptr);
}

// D.5-S2 — Detach + re-attach cycle. The console_script_engine_ +
// console_log_buffer_ unique_ptrs must release in reverse order
// (engine-first to avoid dangling JS callbacks holding the buffer ptr)
// and the next attach must produce a fresh pair.
TEST_F(DevtoolConsoleDogfoodSmokeTest, DetachReattachCyclePreservesContract) {
  for (int cycle = 0; cycle < 3; ++cycle) {
    ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);
    EXPECT_EQ(vx_view_devtool_loaded(view_), 1);

    char eval_buf[64];
    vx_console_eval_status status = VX_CONSOLE_EVAL_INTERRUPTED;
    EXPECT_EQ(vx_view_eval_console(view_, "1+2", 3, eval_buf, sizeof(eval_buf),
                                    &status),
              VX_OK)
        << "cycle " << cycle;
    EXPECT_EQ(status, VX_CONSOLE_EVAL_OK) << "cycle " << cycle;
    EXPECT_STREQ(eval_buf, "3") << "cycle " << cycle;

    EXPECT_EQ(vx_view_detach_devtool(view_), VX_OK);
    EXPECT_EQ(vx_view_devtool_loaded(view_), 0);

    // After detach, eval returns NOT_ATTACHED (lazy-attach reverse probe).
    status = VX_CONSOLE_EVAL_OK;
    EXPECT_EQ(vx_view_eval_console(view_, "1+2", 3, eval_buf, sizeof(eval_buf),
                                    &status),
              VX_ERROR_INVALID_STATE)
        << "cycle " << cycle;
    EXPECT_EQ(status, VX_CONSOLE_EVAL_NOT_ATTACHED) << "cycle " << cycle;
  }
}

// D.5-S3 — Panel JS-side host bindings observable: the
// vx_devtool_get_console_log_drain global registered on the devtool
// ctx by RegisterDevtoolBindings (D.4 plan-fact reconcile #1) must
// return live envelope data when invoked from inspector_panel.js's ctx.
// We simulate panel JS via app_->EvalDevtoolScript so the test catches
// the ctx-mismatch regression that motivated the wiring fix.
TEST_F(DevtoolConsoleDogfoodSmokeTest, PanelCtxSeesHostDrainAndEvalBindings) {
  ASSERT_EQ(vx_view_attach_devtool(view_, nullptr), VX_OK);

  // Probe the bindings exist in the devtool_script_engine ctx (where
  // inspector_panel.js + console_panel.js run). If D.4 wiring
  // regresses, these typeofs flip to "undefined" and panel JS silently
  // drops every drain poll.
  auto probe_drain =
      app_->EvalDevtoolScript(vx::StringView("typeof vx_devtool_get_console_log_drain"),
                              vx::StringView("<probe-drain>"));
  ASSERT_TRUE(probe_drain.ok()) << probe_drain.status().message();
  EXPECT_EQ(probe_drain.value(), "function");

  auto probe_eval =
      app_->EvalDevtoolScript(vx::StringView("typeof vx_console_eval"),
                              vx::StringView("<probe-eval>"));
  ASSERT_TRUE(probe_eval.ok()) << probe_eval.status().message();
  EXPECT_EQ(probe_eval.value(), "function");

  // Round-trip: panel JS calls vx_console_eval(...) which forwards into
  // ConsoleEngine + returns the JSON envelope; envelope status==0 +
  // value=="42" proves the cross-ctx host data channel works.
  auto round_trip = app_->EvalDevtoolScript(
      vx::StringView(
          "var r = vx_console_eval('40 + 2'); var o = JSON.parse(r); "
          "'' + o.status + ',' + o.value + ',' + o.interrupted"),
      vx::StringView("<round-trip>"));
  ASSERT_TRUE(round_trip.ok()) << round_trip.status().message();
  EXPECT_EQ(round_trip.value(), "0,42,false");
}

}  // namespace
