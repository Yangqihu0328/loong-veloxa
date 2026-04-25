#include "veloxa/platform/sdl2/sdl2_event_loop.h"

#include <SDL2/SDL.h>

#include <vector>

#include <gtest/gtest.h>

namespace vx::platform {
namespace {

TEST(Sdl2EventLoop, PostTaskFIFOOrder) {
  Sdl2EventLoop loop;
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

TEST(Sdl2EventLoop, RunAndQuit) {
  Sdl2EventLoop loop;
  int count = 0;
  loop.PostTask([&] {
    ++count;
    loop.Quit();
  });
  loop.Run();
  EXPECT_EQ(count, 1);
  EXPECT_FALSE(loop.is_running());
}

TEST(Sdl2EventLoop, SetTimerOneShot) {
  Sdl2EventLoop loop;
  int fires = 0;
  loop.SetTimer(0, [&] { ++fires; }, /*repeat=*/false);
  // Polling >= 1 time after interval=0 should fire.
  for (int i = 0; i < 5; ++i) loop.PollOnce();
  EXPECT_EQ(fires, 1);
}

TEST(Sdl2EventLoop, CancelTimerBeforeFire) {
  Sdl2EventLoop loop;
  int fires = 0;
  auto id = loop.SetTimer(10000, [&] { ++fires; }, /*repeat=*/true);
  loop.CancelTimer(id);
  loop.PollOnce();
  EXPECT_EQ(fires, 0);
}

}  // namespace
}  // namespace vx::platform
