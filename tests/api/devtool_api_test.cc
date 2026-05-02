#include "veloxa/api/veloxa_api.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

// Test fixture: standard headless view with a simple HTML body.
struct ViewFixture {
  VxEventLoop* loop = nullptr;
  VxSurface* surface = nullptr;
  VxView* view = nullptr;

  ViewFixture() {
    loop = vx_event_loop_create_headless();
    surface = vx_surface_create_memory(100, 100);
    VxViewConfig config{loop, surface, 60, 0xFFFFFFFF};
    view = vx_view_create(&config);
  }

  ~ViewFixture() {
    vx_view_destroy(view);
    vx_surface_destroy(surface);
    vx_event_loop_destroy(loop);
  }

  void LoadHtml(const std::string& html) {
    vx_view_load_html(view, html.c_str(), static_cast<uint32_t>(html.size()));
  }
};

// =============================================================================
// vx_view_serialize_dom_json — DevTool C API thin wrapper (A.0.6).
// Behavior depends on VX_BUILD_DEVTOOL build flag:
//   ON  → real serialization with T7 max_size guard + double-call protocol
//   OFF → returns VX_ERROR_INVALID_STATE (stub path; A14 acceptance)
// =============================================================================

TEST(DevtoolApiTest, SerializeDomJsonNullViewReturnsError) {
  uint32_t needed = 0;
  VxResult r = vx_view_serialize_dom_json(nullptr, nullptr, &needed,
                                           16 * 1024 * 1024);
  EXPECT_EQ(r, VX_ERROR_NULL_PARAM);
}

TEST(DevtoolApiTest, SerializeDomJsonNullOutLenReturnsError) {
  ViewFixture fx;
  VxResult r = vx_view_serialize_dom_json(fx.view, nullptr, nullptr,
                                           16 * 1024 * 1024);
  EXPECT_EQ(r, VX_ERROR_NULL_PARAM);
}

#ifdef VX_BUILD_DEVTOOL

// Double-call protocol (T7 mitigation): first call with out_buf=NULL returns
// the required size in *out_len; caller allocates a buffer of that size and
// invokes again with out_buf set.
TEST(DevtoolApiTest, SerializeDomJsonDoubleCallProtocol) {
  ViewFixture fx;
  fx.LoadHtml("<html><body><div id=\"root\">hello</div></body></html>");

  uint32_t needed = 0;
  VxResult r = vx_view_serialize_dom_json(fx.view, nullptr, &needed,
                                           16 * 1024 * 1024);
  EXPECT_EQ(r, VX_OK);
  EXPECT_GT(needed, 0u);

  std::vector<char> buf(needed);
  uint32_t written = needed;
  r = vx_view_serialize_dom_json(fx.view, buf.data(), &written,
                                  16 * 1024 * 1024);
  EXPECT_EQ(r, VX_OK);
  EXPECT_GT(written, 0u);
  std::string out(buf.data(), written);
  EXPECT_NE(out.find("\"tag\":\"div\""), std::string::npos);
}

TEST(DevtoolApiTest, SerializeDomJsonNoDocumentReturnsZeroSize) {
  ViewFixture fx;
  uint32_t needed = 999;  // sentinel — must be overwritten to 0
  VxResult r = vx_view_serialize_dom_json(fx.view, nullptr, &needed,
                                           16 * 1024 * 1024);
  EXPECT_EQ(r, VX_OK);
  EXPECT_EQ(needed, 0u);
}

// T7 mitigation: hard upper bound on serialization size. Caller-provided
// max_size must be honored to prevent uncontrolled allocation in DevTool
// when target document is malicious / extremely deep.
TEST(DevtoolApiTest, SerializeDomJsonMaxSizeExceededReturnsOutOfMemory) {
  ViewFixture fx;
  std::string html = "<html><body>";
  for (int i = 0; i < 5000; ++i) {
    html += "<div id=\"d";
    html += std::to_string(i);
    html += "\"></div>";
  }
  html += "</body></html>";
  fx.LoadHtml(html);

  uint32_t needed = 0;
  // Tight 1 KiB cap — guaranteed below the actual JSON size.
  VxResult r = vx_view_serialize_dom_json(fx.view, nullptr, &needed, 1024);
  EXPECT_EQ(r, VX_ERROR_OUT_OF_MEMORY);
}

// Guard against undersized caller buffer (separate from max_size which is the
// platform-policy cap). out_len < json size → return OUT_OF_MEMORY without
// writing.
TEST(DevtoolApiTest, SerializeDomJsonCallerBufferTooSmallReturnsOutOfMemory) {
  ViewFixture fx;
  fx.LoadHtml("<html><body><div>x</div></body></html>");

  uint32_t needed = 0;
  ASSERT_EQ(vx_view_serialize_dom_json(fx.view, nullptr, &needed,
                                        16 * 1024 * 1024),
            VX_OK);
  ASSERT_GT(needed, 1u);

  std::vector<char> buf(1);
  uint32_t written = 1;  // intentionally too small
  VxResult r = vx_view_serialize_dom_json(fx.view, buf.data(), &written,
                                           16 * 1024 * 1024);
  EXPECT_EQ(r, VX_ERROR_OUT_OF_MEMORY);
}

// =============================================================================
// T7 boundary cases (TASK-20260502-01 A.2.3 [SECURITY])
//
// A.0.6 already covered the happy path + over-cap rejection. A.2.3 adds
// the corner cases that distinguish "policy cap" from "buffer length"
// confusion at the API edge.
// =============================================================================

// max_size = 0 must be rejected even for an empty JSON envelope, because
// allowing 0 effectively disables the platform-level guard (caller has
// no protection against a future payload growing past their buffer).
TEST(DevtoolApiTest, SerializeDomJsonZeroMaxSizeRejects) {
  ViewFixture fx;
  fx.LoadHtml("<html><body><div>hello</div></body></html>");
  uint32_t needed = 0;
  EXPECT_EQ(vx_view_serialize_dom_json(fx.view, nullptr, &needed, 0),
            VX_ERROR_OUT_OF_MEMORY);
}

// max_size = UINT32_MAX (the documented upper bound, ~4 GiB) must NOT
// be rejected — embedders that compute max_size from a runtime policy
// can hit this value when the policy is "no practical cap".
TEST(DevtoolApiTest, SerializeDomJsonMaxUint32SizeAllowed) {
  ViewFixture fx;
  fx.LoadHtml("<html><body><div>x</div></body></html>");
  uint32_t needed = 0;
  VxResult r = vx_view_serialize_dom_json(
      fx.view, nullptr, &needed, static_cast<uint32_t>(0xFFFFFFFFu));
  EXPECT_EQ(r, VX_OK);
  EXPECT_GT(needed, 0u);
}

// max_size exactly equal to the JSON envelope length must be accepted
// (boundary inclusive — needed > max_size rejects, needed == max_size
// passes). This is the tight-fit caller scenario.
TEST(DevtoolApiTest, SerializeDomJsonMaxSizeAtExactNeededAccepts) {
  ViewFixture fx;
  fx.LoadHtml("<html><body><div>x</div></body></html>");
  uint32_t needed = 0;
  ASSERT_EQ(vx_view_serialize_dom_json(fx.view, nullptr, &needed,
                                        16 * 1024 * 1024),
            VX_OK);
  ASSERT_GT(needed, 0u);

  // First call with cap = needed → OK, returns required size again.
  uint32_t probe = 0;
  ASSERT_EQ(vx_view_serialize_dom_json(fx.view, nullptr, &probe, needed),
            VX_OK);
  EXPECT_EQ(probe, needed);

  // And the actual write at cap = needed succeeds with exact buffer.
  std::vector<char> buf(needed);
  uint32_t written = needed;
  EXPECT_EQ(vx_view_serialize_dom_json(fx.view, buf.data(), &written, needed),
            VX_OK);
  EXPECT_EQ(written, needed);
}

// max_size one byte below needed must be rejected. Pairs with the
// at-exact test above to fence the boundary from both sides.
TEST(DevtoolApiTest, SerializeDomJsonMaxSizeOneBelowNeededRejects) {
  ViewFixture fx;
  fx.LoadHtml("<html><body><div>x</div></body></html>");
  uint32_t needed = 0;
  ASSERT_EQ(vx_view_serialize_dom_json(fx.view, nullptr, &needed,
                                        16 * 1024 * 1024),
            VX_OK);
  ASSERT_GT(needed, 1u);

  uint32_t probe = 0;
  EXPECT_EQ(
      vx_view_serialize_dom_json(fx.view, nullptr, &probe, needed - 1),
      VX_ERROR_OUT_OF_MEMORY);
}

// T3 propagation through the public API: password values redacted.
TEST(DevtoolApiTest, SerializeDomJsonRedactsPasswordValues) {
  ViewFixture fx;
  fx.LoadHtml(
      "<html><body><input type=\"password\" value=\"hunter2\"></body></html>");

  uint32_t needed = 0;
  ASSERT_EQ(vx_view_serialize_dom_json(fx.view, nullptr, &needed,
                                        16 * 1024 * 1024),
            VX_OK);
  std::vector<char> buf(needed);
  uint32_t written = needed;
  ASSERT_EQ(vx_view_serialize_dom_json(fx.view, buf.data(), &written,
                                        16 * 1024 * 1024),
            VX_OK);

  std::string out(buf.data(), written);
  EXPECT_EQ(out.find("hunter2"), std::string::npos)
      << "Password value MUST NOT leak through the public C API";
  EXPECT_NE(out.find("[REDACTED]"), std::string::npos);
}

#else  // !VX_BUILD_DEVTOOL — A14 acceptance: stub returns INVALID_STATE.

TEST(DevtoolApiTest, SerializeDomJsonReturnsInvalidStateWhenDevtoolDisabled) {
  ViewFixture fx;
  uint32_t needed = 0;
  VxResult r = vx_view_serialize_dom_json(fx.view, nullptr, &needed,
                                           16 * 1024 * 1024);
  EXPECT_EQ(r, VX_ERROR_INVALID_STATE);
}

#endif  // VX_BUILD_DEVTOOL

}  // namespace
