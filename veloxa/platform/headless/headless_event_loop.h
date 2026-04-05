#ifndef VELOXA_PLATFORM_HEADLESS_HEADLESS_EVENT_LOOP_H_
#define VELOXA_PLATFORM_HEADLESS_HEADLESS_EVENT_LOOP_H_

#include <chrono>

#include "veloxa/foundation/containers/vector.h"
#include "veloxa/platform/event_loop.h"

namespace vx::platform {

class HeadlessEventLoop : public EventLoop {
 public:
  HeadlessEventLoop();
  ~HeadlessEventLoop() override = default;

  void Run() override;
  void Quit() override;
  bool is_running() const override;
  void PostTask(Task task) override;
  TimerId SetTimer(vx::u32 interval_ms, Task callback,
                   bool repeat = false) override;
  void CancelTimer(TimerId id) override;
  void PollOnce() override;

 private:
  struct TimerEntry {
    TimerId id;
    vx::u32 interval_ms;
    Task callback;
    bool repeat;
    std::chrono::steady_clock::time_point next_fire;
    bool cancelled;
  };

  void ProcessTasks();
  void ProcessTimers();

  vx::Vector<Task> task_queue_;
  vx::Vector<TimerEntry> timers_;
  bool running_;
  TimerId next_timer_id_;
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_HEADLESS_HEADLESS_EVENT_LOOP_H_
