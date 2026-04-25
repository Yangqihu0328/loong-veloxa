#ifndef VELOXA_PLATFORM_SDL2_SDL2_EVENT_LOOP_H_
#define VELOXA_PLATFORM_SDL2_SDL2_EVENT_LOOP_H_

#include <functional>
#include <memory>

#include "veloxa/api/veloxa_api.h"
#include "veloxa/platform/event_loop.h"

namespace vx::platform {

class HeadlessEventLoop;

// EventLoop driven by SDL2's event queue.
//
// Per design Q1: SDL input events are delivered via PumpInputEvents(callback)
// — a pull model where the embedder explicitly drains SDL events and routes
// them as VxInputEvents. The loop holds no reference to the View / surface,
// preserving single-direction coupling.
//
// Task queue and timer machinery are delegated to an internal HeadlessEventLoop
// instance (composition, not inheritance) — this keeps the SDL-specific
// surface area focused on input pumping.
class Sdl2EventLoop : public EventLoop {
 public:
  using InputEventCallback = std::function<void(const ::VxInputEvent&)>;

  Sdl2EventLoop();
  ~Sdl2EventLoop() override;

  Sdl2EventLoop(const Sdl2EventLoop&) = delete;
  Sdl2EventLoop& operator=(const Sdl2EventLoop&) = delete;

  // EventLoop overrides.
  void Run() override;
  void Quit() override;
  bool is_running() const override;
  void PostTask(Task task) override;
  TimerId SetTimer(vx::u32 interval_ms, Task callback,
                   bool repeat = false) override;
  void CancelTimer(TimerId id) override;
  void PollOnce() override;

  // SDL-specific: drain pending SDL events, translate input ones, and invoke
  // the previously-set callback for each. SDL_QUIT triggers Quit() on this
  // loop; SDL_WINDOWEVENT is silently dropped for now.
  void PumpInputEvents();

  // Set the callback that receives translated input events from PumpInputEvents.
  // Pass nullptr to detach.
  void SetInputCallback(InputEventCallback callback);

 private:
  std::unique_ptr<HeadlessEventLoop> inner_;
  InputEventCallback input_cb_;
  bool running_;
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_SDL2_SDL2_EVENT_LOOP_H_
