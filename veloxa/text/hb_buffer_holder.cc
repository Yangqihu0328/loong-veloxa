// veloxa/text/hb_buffer_holder.cc
//
// See header for contract. TASK-20260424-04 move from
// software_canvas.cc's anonymous namespace (was introduced in
// TASK-20260424-03 Phase 1 D2 to eliminate per-DrawText
// hb_buffer_create/destroy).

#include "veloxa/text/hb_buffer_holder.h"

#include <hb.h>

namespace vx::text {

namespace {

struct HbBufferHolder {
  hb_buffer_t* buf = nullptr;
  HbBufferHolder() : buf(hb_buffer_create()) {}
  ~HbBufferHolder() {
    if (buf) hb_buffer_destroy(buf);
  }
  HbBufferHolder(const HbBufferHolder&) = delete;
  HbBufferHolder& operator=(const HbBufferHolder&) = delete;
};

}  // namespace

hb_buffer_t* AcquireThreadLocalHbBuffer() {
  thread_local HbBufferHolder holder;
  return holder.buf;
}

}  // namespace vx::text
