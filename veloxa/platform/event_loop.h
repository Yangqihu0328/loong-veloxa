#ifndef VELOXA_PLATFORM_EVENT_LOOP_H_
#define VELOXA_PLATFORM_EVENT_LOOP_H_

#include <functional>

#include "veloxa/foundation/base/types.h"

namespace vx::platform {

class EventLoop {
 public:
  using Task = std::function<void()>;
  using TimerId = vx::u32;

  virtual ~EventLoop() = default;
  virtual void Run() = 0;
  virtual void Quit() = 0;
  virtual bool is_running() const = 0;
  virtual void PostTask(Task task) = 0;
  virtual TimerId SetTimer(vx::u32 interval_ms, Task callback,
                           bool repeat = false) = 0;
  virtual void CancelTimer(TimerId id) = 0;
  virtual void PollOnce() = 0;
};

}  // namespace vx::platform

#endif  // VELOXA_PLATFORM_EVENT_LOOP_H_
