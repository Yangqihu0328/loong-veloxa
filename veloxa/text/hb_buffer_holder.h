// veloxa/text/hb_buffer_holder.h
//
// Thread-local hb_buffer_t reuse. Extracted from software_canvas.cc by
// TASK-20260424-04 so that FontManager::ShapeOrLookup can reuse the same
// buffer as SoftwareCanvas::DrawText on the warm path (eliminates an
// otherwise unnecessary second hb_buffer_create/destroy pair per miss).
//
// Thread-safety: one hb_buffer_t per thread, owned by a thread_local
// holder whose destructor runs at thread exit. Callers MUST call
// hb_buffer_reset before reuse.

#ifndef VELOXA_TEXT_HB_BUFFER_HOLDER_H_
#define VELOXA_TEXT_HB_BUFFER_HOLDER_H_

struct hb_buffer_t;

namespace vx::text {

// Returns the current thread's reusable hb_buffer_t, creating it lazily
// on first call. Never nullptr in practice (hb_buffer_create cannot fail
// for normal inputs); returns nullptr only under OOM.
hb_buffer_t* AcquireThreadLocalHbBuffer();

}  // namespace vx::text

#endif  // VELOXA_TEXT_HB_BUFFER_HOLDER_H_
