#include "veloxa/core/event/event_dispatcher.h"

#include <string>
#include <vector>

#include "veloxa/core/dom/document.h"

#include <gtest/gtest.h>

namespace {

using vx::dom::Document;
using vx::dom::TagId;
using vx::event::DOMEvent;
using vx::event::EventDispatcher;
using vx::event::EventPhase;
using vx::event::EventType;
using vx::event::InputEvent;

class EventDispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = doc_.CreateElement(TagId::kDiv);
    child_ = doc_.CreateElement(TagId::kP);
    doc_.AppendChild(root_);
    root_->AppendChild(child_);
  }

  DOMEvent MakeEvent(EventType type, vx::dom::Element* target) {
    DOMEvent event{};
    event.input.type = type;
    event.target = target;
    return event;
  }

  Document doc_;
  vx::dom::Element* root_ = nullptr;
  vx::dom::Element* child_ = nullptr;
  EventDispatcher dispatcher_;
};

TEST_F(EventDispatcherTest, DispatchToTarget_NoListeners) {
  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);
}

TEST_F(EventDispatcherTest, DispatchToTarget_SingleListener) {
  int count = 0;
  EventPhase observed_phase = EventPhase::kNone;
  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&](DOMEvent& e) {
        ++count;
        observed_phase = e.phase;
      },
      false);

  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(observed_phase, EventPhase::kTarget);
}

TEST_F(EventDispatcherTest, BubblePhase) {
  int count = 0;
  EventPhase observed_phase = EventPhase::kNone;
  dispatcher_.AddEventListener(
      root_, EventType::kPointerDown,
      [&](DOMEvent& e) {
        ++count;
        observed_phase = e.phase;
      },
      false);

  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(observed_phase, EventPhase::kBubble);
}

TEST_F(EventDispatcherTest, CapturePhase) {
  int count = 0;
  EventPhase observed_phase = EventPhase::kNone;
  dispatcher_.AddEventListener(
      root_, EventType::kPointerDown,
      [&](DOMEvent& e) {
        ++count;
        observed_phase = e.phase;
      },
      true);

  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(observed_phase, EventPhase::kCapture);
}

TEST_F(EventDispatcherTest, CaptureBeforeBubble) {
  std::vector<std::string> log;

  dispatcher_.AddEventListener(
      root_, EventType::kPointerDown,
      [&log](DOMEvent&) { log.push_back("root-capture"); }, true);
  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&log](DOMEvent&) { log.push_back("child-target"); }, false);
  dispatcher_.AddEventListener(
      root_, EventType::kPointerDown,
      [&log](DOMEvent&) { log.push_back("root-bubble"); }, false);

  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);

  ASSERT_EQ(log.size(), 3u);
  EXPECT_EQ(log[0], "root-capture");
  EXPECT_EQ(log[1], "child-target");
  EXPECT_EQ(log[2], "root-bubble");
}

TEST_F(EventDispatcherTest, StopPropagationInCapture) {
  std::vector<std::string> log;

  dispatcher_.AddEventListener(
      root_, EventType::kPointerDown,
      [&log](DOMEvent& e) {
        log.push_back("root-capture");
        e.StopPropagation();
      },
      true);
  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&log](DOMEvent&) { log.push_back("child-target"); }, false);

  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);

  ASSERT_EQ(log.size(), 1u);
  EXPECT_EQ(log[0], "root-capture");
}

TEST_F(EventDispatcherTest, StopPropagationInBubble) {
  std::vector<std::string> log;

  auto* grandchild = doc_.CreateElement(TagId::kSpan);
  child_->AppendChild(grandchild);

  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&log](DOMEvent& e) {
        log.push_back("child-bubble");
        e.StopPropagation();
      },
      false);
  dispatcher_.AddEventListener(
      root_, EventType::kPointerDown,
      [&log](DOMEvent&) { log.push_back("root-bubble"); }, false);

  auto event = MakeEvent(EventType::kPointerDown, grandchild);
  dispatcher_.Dispatch(event);

  ASSERT_EQ(log.size(), 1u);
  EXPECT_EQ(log[0], "child-bubble");
}

TEST_F(EventDispatcherTest, TargetPhase_BothListeners) {
  std::vector<std::string> log;

  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&log](DOMEvent&) { log.push_back("child-capture"); }, true);
  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&log](DOMEvent&) { log.push_back("child-bubble"); }, false);

  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);

  ASSERT_EQ(log.size(), 2u);
  EXPECT_EQ(log[0], "child-capture");
  EXPECT_EQ(log[1], "child-bubble");
}

TEST_F(EventDispatcherTest, WrongEventType_NotFired) {
  int count = 0;
  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&count](DOMEvent&) { ++count; }, false);

  auto event = MakeEvent(EventType::kPointerMove, child_);
  dispatcher_.Dispatch(event);

  EXPECT_EQ(count, 0);
}

TEST_F(EventDispatcherTest, RemoveEventListeners) {
  int count = 0;
  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&count](DOMEvent&) { ++count; }, false);

  dispatcher_.RemoveEventListeners(child_);

  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);

  EXPECT_EQ(count, 0);
}

// ----- Listener token (TASK-20260419-01 B7) -----
//
// AddEventListener returns a u64 token; RemoveEventListenerByToken removes
// only the matching listener while siblings on the same element keep firing.

TEST_F(EventDispatcherTest, AddEventListenerReturnsUniqueTokens) {
  auto t1 = dispatcher_.AddEventListener(
      child_, EventType::kPointerDown, [](DOMEvent&) {}, false);
  auto t2 = dispatcher_.AddEventListener(
      child_, EventType::kPointerDown, [](DOMEvent&) {}, false);
  auto t3 = dispatcher_.AddEventListener(
      root_, EventType::kPointerUp, [](DOMEvent&) {}, false);
  EXPECT_NE(t1, t2);
  EXPECT_NE(t2, t3);
  EXPECT_NE(t1, t3);
  EXPECT_NE(t1, 0u);
  EXPECT_NE(t2, 0u);
  EXPECT_NE(t3, 0u);
}

TEST_F(EventDispatcherTest, RemoveEventListenerByToken_RemovesOnlyOne) {
  int a = 0, b = 0;
  auto token_a = dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&a](DOMEvent&) { ++a; }, false);
  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&b](DOMEvent&) { ++b; }, false);

  dispatcher_.RemoveEventListenerByToken(child_, token_a);

  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);
  EXPECT_EQ(a, 0);  // removed
  EXPECT_EQ(b, 1);  // sibling survived
}

TEST_F(EventDispatcherTest, RemoveEventListenerByToken_UnknownTokenIsNoOp) {
  int count = 0;
  dispatcher_.AddEventListener(
      child_, EventType::kPointerDown,
      [&count](DOMEvent&) { ++count; }, false);

  dispatcher_.RemoveEventListenerByToken(child_, 0xDEADBEEFu);
  // Unknown token must not throw and must not affect existing listener.
  auto event = MakeEvent(EventType::kPointerDown, child_);
  dispatcher_.Dispatch(event);
  EXPECT_EQ(count, 1);
}

}  // namespace
