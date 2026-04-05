#include "veloxa/platform/headless/headless_event_loop.h"

#include <vector>

#include "gtest/gtest.h"

namespace vx::platform {
namespace {

TEST(HeadlessEventLoopTest, PostTaskFIFOOrder) {
  HeadlessEventLoop loop;
  std::vector<int> order;
  loop.PostTask([&] { order.push_back(1); });
  loop.PostTask([&] { order.push_back(2); });
  loop.PostTask([&] { order.push_back(3); });
  loop.PollOnce();

  ASSERT_EQ(order.size(), 3u);
  EXPECT_EQ(order[0], 1);
  EXPECT_EQ(order[1], 2);
  EXPECT_EQ(order[2], 3);
}

TEST(HeadlessEventLoopTest, PollOnceProcessesCurrentQueue) {
  HeadlessEventLoop loop;
  int count = 0;
  loop.PostTask([&] { ++count; });
  loop.PostTask([&] { ++count; });
  loop.PollOnce();
  EXPECT_EQ(count, 2);

  loop.PollOnce();
  EXPECT_EQ(count, 2);
}

TEST(HeadlessEventLoopTest, RunAndQuit) {
  HeadlessEventLoop loop;
  int count = 0;
  loop.PostTask([&] {
    ++count;
    loop.Quit();
  });

  EXPECT_FALSE(loop.is_running());
  loop.Run();
  EXPECT_FALSE(loop.is_running());
  EXPECT_EQ(count, 1);
}

TEST(HeadlessEventLoopTest, SetTimerFiresImmediately) {
  HeadlessEventLoop loop;
  bool fired = false;
  loop.SetTimer(0, [&] { fired = true; });
  loop.PollOnce();
  EXPECT_TRUE(fired);
}

TEST(HeadlessEventLoopTest, SetTimerRepeatFiresMultipleTimes) {
  HeadlessEventLoop loop;
  int count = 0;
  loop.SetTimer(0, [&] { ++count; }, true);
  loop.PollOnce();
  EXPECT_GE(count, 1);
  loop.PollOnce();
  EXPECT_GE(count, 2);
  loop.PollOnce();
  EXPECT_GE(count, 3);
}

TEST(HeadlessEventLoopTest, CancelTimerPreventsFiring) {
  HeadlessEventLoop loop;
  bool fired = false;
  auto id = loop.SetTimer(0, [&] { fired = true; });
  loop.CancelTimer(id);
  loop.PollOnce();
  EXPECT_FALSE(fired);
}

TEST(HeadlessEventLoopTest, NestedPostTask) {
  HeadlessEventLoop loop;
  std::vector<int> order;
  loop.PostTask([&] {
    order.push_back(1);
    loop.PostTask([&] { order.push_back(2); });
  });
  loop.PollOnce();
  ASSERT_EQ(order.size(), 1u);
  EXPECT_EQ(order[0], 1);

  loop.PollOnce();
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[1], 2);
}

TEST(HeadlessEventLoopTest, TimerIdIsUnique) {
  HeadlessEventLoop loop;
  auto id1 = loop.SetTimer(0, [] {});
  auto id2 = loop.SetTimer(0, [] {});
  auto id3 = loop.SetTimer(0, [] {});
  EXPECT_NE(id1, id2);
  EXPECT_NE(id2, id3);
}

TEST(HeadlessEventLoopTest, CancelNonExistentTimerDoesNotCrash) {
  HeadlessEventLoop loop;
  loop.CancelTimer(9999);
  loop.PollOnce();
}

}  // namespace
}  // namespace vx::platform
