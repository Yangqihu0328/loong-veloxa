#include "veloxa/platform/headless/headless_event_loop.h"

#include <utility>

namespace vx::platform {

HeadlessEventLoop::HeadlessEventLoop() : running_(false), next_timer_id_(1) {}

void HeadlessEventLoop::Run() {
  running_ = true;
  while (running_) {
    PollOnce();
  }
}

void HeadlessEventLoop::Quit() { running_ = false; }

bool HeadlessEventLoop::is_running() const { return running_; }

void HeadlessEventLoop::PostTask(Task task) {
  task_queue_.push_back(std::move(task));
}

EventLoop::TimerId HeadlessEventLoop::SetTimer(vx::u32 interval_ms,
                                               Task callback, bool repeat) {
  TimerId id = next_timer_id_++;
  auto now = std::chrono::steady_clock::now();
  TimerEntry entry;
  entry.id = id;
  entry.interval_ms = interval_ms;
  entry.callback = std::move(callback);
  entry.repeat = repeat;
  entry.next_fire = now + std::chrono::milliseconds(interval_ms);
  entry.cancelled = false;
  timers_.push_back(std::move(entry));
  return id;
}

void HeadlessEventLoop::CancelTimer(TimerId id) {
  for (vx::usize i = 0; i < timers_.size(); ++i) {
    if (timers_[i].id == id) {
      timers_[i].cancelled = true;
      return;
    }
  }
}

void HeadlessEventLoop::PollOnce() {
  ProcessTasks();
  ProcessTimers();
}

void HeadlessEventLoop::ProcessTasks() {
  vx::Vector<Task> local;
  std::swap(local, task_queue_);
  for (vx::usize i = 0; i < local.size(); ++i) {
    local[i]();
  }
}

void HeadlessEventLoop::ProcessTimers() {
  auto now = std::chrono::steady_clock::now();
  for (vx::usize i = 0; i < timers_.size(); ++i) {
    if (timers_[i].cancelled) continue;
    if (now >= timers_[i].next_fire) {
      timers_[i].callback();
      if (timers_[i].repeat) {
        timers_[i].next_fire =
            now + std::chrono::milliseconds(timers_[i].interval_ms);
      } else {
        timers_[i].cancelled = true;
      }
    }
  }

  // Remove cancelled timers
  vx::usize write = 0;
  for (vx::usize read = 0; read < timers_.size(); ++read) {
    if (!timers_[read].cancelled) {
      if (write != read) {
        timers_[write] = std::move(timers_[read]);
      }
      ++write;
    }
  }
  while (timers_.size() > write) {
    timers_.pop_back();
  }
}

}  // namespace vx::platform
