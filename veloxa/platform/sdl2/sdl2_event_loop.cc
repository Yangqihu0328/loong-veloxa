#include "veloxa/platform/sdl2/sdl2_event_loop.h"

#include <SDL2/SDL.h>

#include <utility>

#include "veloxa/platform/headless/headless_event_loop.h"
#include "veloxa/platform/sdl2/sdl2_input_translate.h"

namespace vx::platform {

Sdl2EventLoop::Sdl2EventLoop()
    : inner_(std::make_unique<HeadlessEventLoop>()), running_(false) {}

Sdl2EventLoop::~Sdl2EventLoop() = default;

void Sdl2EventLoop::Run() {
  running_ = true;
  while (running_) {
    PumpInputEvents();
    inner_->PollOnce();
    if (!running_) break;
    // Small yield to keep idle CPU low — frame pacing is owned upstream by
    // the renderer's target_fps timer.
    SDL_Delay(1);
  }
}

void Sdl2EventLoop::Quit() { running_ = false; }
bool Sdl2EventLoop::is_running() const { return running_; }

void Sdl2EventLoop::PostTask(Task task) { inner_->PostTask(std::move(task)); }

EventLoop::TimerId Sdl2EventLoop::SetTimer(vx::u32 interval_ms, Task callback,
                                           bool repeat) {
  return inner_->SetTimer(interval_ms, std::move(callback), repeat);
}

void Sdl2EventLoop::CancelTimer(TimerId id) { inner_->CancelTimer(id); }

void Sdl2EventLoop::PollOnce() {
  PumpInputEvents();
  inner_->PollOnce();
}

void Sdl2EventLoop::PumpInputEvents() {
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    if (ev.type == SDL_QUIT) {
      // Use self Quit so Run() loop sees running_ flip; inner_ is composed
      // and only exposes task/timer state.
      Quit();
      continue;
    }
    if (!input_cb_) continue;
    ::VxInputEvent translated{};
    if (TranslateSdlEvent(ev, translated)) {
      input_cb_(translated);
    }
  }
}

void Sdl2EventLoop::SetInputCallback(InputEventCallback callback) {
  input_cb_ = std::move(callback);
}

}  // namespace vx::platform
