#include "veloxa/core/event/event_types.h"

#include <gtest/gtest.h>

namespace {

using namespace vx::event;

TEST(EventTypesTest, InputEventDefaultConstruction) {
  InputEvent e{};
  e.type = EventType::kPointerDown;
  EXPECT_EQ(e.x, 0.0f);
  EXPECT_EQ(e.y, 0.0f);
  EXPECT_EQ(e.button, 0);
  EXPECT_EQ(e.key_code, 0u);
  EXPECT_EQ(e.modifiers, 0u);
  EXPECT_EQ(e.timestamp_ms, 0u);
}

TEST(EventTypesTest, InputEventWithCoordinates) {
  InputEvent e{};
  e.type = EventType::kPointerMove;
  e.x = 100.5f;
  e.y = 200.0f;
  EXPECT_FLOAT_EQ(e.x, 100.5f);
  EXPECT_FLOAT_EQ(e.y, 200.0f);
}

TEST(EventTypesTest, InputEventKeyboard) {
  InputEvent e{};
  e.type = EventType::kKeyDown;
  e.key_code = 0x41;
  e.modifiers = 1;
  EXPECT_EQ(e.key_code, 0x41u);
  EXPECT_EQ(e.modifiers, 1u);
}

TEST(EventTypesTest, InputEventTouch) {
  InputEvent e{};
  e.type = EventType::kTouchStart;
  e.touch_id = 1;
  e.x = 50.0f;
  e.y = 60.0f;
  EXPECT_EQ(e.touch_id, 1u);
}

TEST(EventTypesTest, DOMEventDefaultConstruction) {
  DOMEvent de{};
  EXPECT_EQ(de.target, nullptr);
  EXPECT_EQ(de.current_target, nullptr);
  EXPECT_EQ(de.phase, EventPhase::kNone);
  EXPECT_FALSE(de.propagation_stopped);
  EXPECT_FALSE(de.default_prevented);
}

TEST(EventTypesTest, DOMEventStopPropagation) {
  DOMEvent de{};
  EXPECT_FALSE(de.propagation_stopped);
  de.StopPropagation();
  EXPECT_TRUE(de.propagation_stopped);
}

TEST(EventTypesTest, DOMEventPreventDefault) {
  DOMEvent de{};
  EXPECT_FALSE(de.default_prevented);
  de.PreventDefault();
  EXPECT_TRUE(de.default_prevented);
}

TEST(EventTypesTest, EventPhaseValues) {
  EXPECT_NE(EventPhase::kNone, EventPhase::kCapture);
  EXPECT_NE(EventPhase::kCapture, EventPhase::kTarget);
  EXPECT_NE(EventPhase::kTarget, EventPhase::kBubble);
}

}  // namespace
