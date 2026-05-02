#include "veloxa/devtool/input_dispatch_splitter.h"

#include <gtest/gtest.h>

namespace vx::devtool {
namespace {

// =============================================================================
// input_dispatch_splitter_test — TASK-20260502-01 A.1.5
//
// Pure state-machine tests for the splitter dock input router. The router
// decides which Document (target / DevTool / splitter handle) receives a
// pointer event based on:
//   1. mouse_x position relative to the splitter handle X (static hit area)
//   2. drag capture state (sticky routing during drag)
//
// Plan-locked default: splitter at x=530 (window 800 - DevTool dock 270),
// hit handle half-width 4px → handle hit area is [526, 534].
// =============================================================================

TEST(InputDispatchSplitter, RoutesToTargetWhenLeftOfSplitter) {
  InputDispatchSplitter s;
  // Default splitter_x_ = 530, half-width 4 → target zone is x < 526.
  EXPECT_EQ(s.RouteToDocument(/*mouse_x=*/100.0f, /*mouse_y=*/100.0f,
                              /*is_pointer_down=*/false,
                              /*is_pointer_up=*/false),
            DispatchTarget::kTarget);
  EXPECT_EQ(s.RouteToDocument(525.0f, 100.0f, false, false),
            DispatchTarget::kTarget);
}

TEST(InputDispatchSplitter, RoutesToDevToolWhenRightOfSplitter) {
  InputDispatchSplitter s;
  // DevTool zone is x > 534.
  EXPECT_EQ(s.RouteToDocument(700.0f, 100.0f, false, false),
            DispatchTarget::kDevTool);
  EXPECT_EQ(s.RouteToDocument(535.0f, 100.0f, false, false),
            DispatchTarget::kDevTool);
}

TEST(InputDispatchSplitter, RoutesToHandleWhenInsideHitArea) {
  InputDispatchSplitter s;
  // Handle hit area: [526, 534].
  EXPECT_EQ(s.RouteToDocument(530.0f, 100.0f, false, false),
            DispatchTarget::kSplitterHandle);
  EXPECT_EQ(s.RouteToDocument(526.0f, 100.0f, false, false),
            DispatchTarget::kSplitterHandle);
  EXPECT_EQ(s.RouteToDocument(534.0f, 100.0f, false, false),
            DispatchTarget::kSplitterHandle);
}

TEST(InputDispatchSplitter,
     DragCaptureKeepsRoutingToHandleAcrossBoundary) {
  // Drag capture: pointer down on the handle should sticky-route ALL
  // subsequent events (even those over target/devtool zones) to the
  // handle until pointer up. This is the splitter drag protocol — without
  // capture, fast drag could "lose" the handle when mouse leaves the
  // ±4px hit area between frames.
  InputDispatchSplitter s;
  EXPECT_FALSE(s.dragging());

  // Press on the handle starts drag capture.
  EXPECT_EQ(s.RouteToDocument(530.0f, 100.0f, /*is_pointer_down=*/true,
                              /*is_pointer_up=*/false),
            DispatchTarget::kSplitterHandle);
  EXPECT_TRUE(s.dragging());

  // Drag moves mouse far into target zone — must STILL route to handle.
  EXPECT_EQ(s.RouteToDocument(50.0f, 100.0f, false, false),
            DispatchTarget::kSplitterHandle);
  EXPECT_TRUE(s.dragging());

  // Drag moves mouse far into devtool zone — must STILL route to handle.
  EXPECT_EQ(s.RouteToDocument(750.0f, 100.0f, false, false),
            DispatchTarget::kSplitterHandle);
  EXPECT_TRUE(s.dragging());
}

TEST(InputDispatchSplitter, PointerUpReleasesDragCapture) {
  InputDispatchSplitter s;
  s.RouteToDocument(530.0f, 100.0f, true, false);  // capture
  ASSERT_TRUE(s.dragging());

  // Pointer up while in target zone — releases capture, current event
  // still routes to handle (terminating the drag), then state clears.
  s.RouteToDocument(50.0f, 100.0f, false, /*is_pointer_up=*/true);
  EXPECT_FALSE(s.dragging());

  // Subsequent move in target zone now routes to target normally.
  EXPECT_EQ(s.RouteToDocument(50.0f, 100.0f, false, false),
            DispatchTarget::kTarget);
}

TEST(InputDispatchSplitter, PressInTargetZoneDoesNotStartDragCapture) {
  // R1 mitigation: drag capture must ONLY engage when the press lands on
  // the splitter handle — otherwise normal click in the target Document
  // would erroneously sticky-route to handle until mouseup.
  InputDispatchSplitter s;
  EXPECT_EQ(s.RouteToDocument(100.0f, 100.0f, /*is_pointer_down=*/true,
                              /*is_pointer_up=*/false),
            DispatchTarget::kTarget);
  EXPECT_FALSE(s.dragging());

  // Move into devtool zone — should route to devtool, NOT sticky to target.
  EXPECT_EQ(s.RouteToDocument(700.0f, 100.0f, false, false),
            DispatchTarget::kDevTool);
}

TEST(InputDispatchSplitter, SetSplitterXShiftsHitArea) {
  InputDispatchSplitter s;
  s.SetSplitterX(200.0f);
  // New hit area [196, 204].
  EXPECT_EQ(s.RouteToDocument(200.0f, 100.0f, false, false),
            DispatchTarget::kSplitterHandle);
  EXPECT_EQ(s.RouteToDocument(150.0f, 100.0f, false, false),
            DispatchTarget::kTarget);
  EXPECT_EQ(s.RouteToDocument(300.0f, 100.0f, false, false),
            DispatchTarget::kDevTool);
}

}  // namespace
}  // namespace vx::devtool
