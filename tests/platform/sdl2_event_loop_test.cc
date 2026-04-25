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

class Sdl2EventLoopPumpFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    // Ensure VIDEO subsystem so SDL_PushEvent works.
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    // Drain any leftover events from prior tests.
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
  }
  void TearDown() override {
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
};

TEST_F(Sdl2EventLoopPumpFixture, CallbackReceivesTranslatedMouseMove) {
  Sdl2EventLoop loop;
  std::vector<VxInputEvent> received;
  loop.SetInputCallback(
      [&](const VxInputEvent& e) { received.push_back(e); });

  SDL_Event ev{};
  ev.type = SDL_MOUSEMOTION;
  ev.motion.timestamp = SDL_GetTicks();
  ev.motion.x = 42;
  ev.motion.y = 99;
  ASSERT_EQ(SDL_PushEvent(&ev), 1) << "SDL_PushEvent: " << SDL_GetError();

  loop.PumpInputEvents();
  ASSERT_EQ(received.size(), 1u);
  EXPECT_EQ(received[0].type, VX_EVENT_POINTER_MOVE);
  EXPECT_FLOAT_EQ(received[0].x, 42.0f);
  EXPECT_FLOAT_EQ(received[0].y, 99.0f);
}

TEST_F(Sdl2EventLoopPumpFixture, SdlQuitTriggersLoopQuit) {
  Sdl2EventLoop loop;
  // Simulate: PostTask that posts SDL_QUIT, then Run.
  loop.PostTask([] {
    SDL_Event ev{};
    ev.type = SDL_QUIT;
    SDL_PushEvent(&ev);
  });
  loop.Run();
  EXPECT_FALSE(loop.is_running());
}

TEST_F(Sdl2EventLoopPumpFixture, NoCallbackDropsInputSilently) {
  Sdl2EventLoop loop;
  // No SetInputCallback — events should be drained without crash.
  SDL_Event ev{};
  ev.type = SDL_MOUSEMOTION;
  ev.motion.x = 1;
  ev.motion.y = 2;
  ASSERT_EQ(SDL_PushEvent(&ev), 1);
  loop.PumpInputEvents();
  SUCCEED();
}

TEST_F(Sdl2EventLoopPumpFixture, DetachCallbackByPassingNullptr) {
  Sdl2EventLoop loop;
  int fires = 0;
  loop.SetInputCallback([&](const VxInputEvent&) { ++fires; });
  loop.SetInputCallback(nullptr);
  SDL_Event ev{};
  ev.type = SDL_MOUSEMOTION;
  ASSERT_EQ(SDL_PushEvent(&ev), 1);
  loop.PumpInputEvents();
  EXPECT_EQ(fires, 0);
}

}  // namespace
}  // namespace vx::platform
