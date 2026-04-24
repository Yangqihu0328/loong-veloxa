// veloxa/text/font_handle.h
//
// FontHandle type alias + sentinel. Extracted from font_manager.h by
// TASK-20260424-04 so that shape_cache.h can reference FontHandle without
// pulling in the full FontManager API (avoids a circular include between
// font_manager.h and shape_cache.h once FontManager owns a ShapeCache).

#ifndef VELOXA_TEXT_FONT_HANDLE_H_
#define VELOXA_TEXT_FONT_HANDLE_H_

#include "veloxa/foundation/base/types.h"

namespace vx::text {

using FontHandle = u32;
static constexpr FontHandle kInvalidFont = 0;

}  // namespace vx::text

#endif  // VELOXA_TEXT_FONT_HANDLE_H_
