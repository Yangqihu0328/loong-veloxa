#ifndef VELOXA_CORE_IMAGE_IMAGE_DECODER_H_
#define VELOXA_CORE_IMAGE_IMAGE_DECODER_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/image.h"

namespace vx::image {

StatusOr<gfx::Image> DecodeFromFile(StringView path);
StatusOr<gfx::Image> DecodeFromMemory(const u8* data, usize len);

}  // namespace vx::image

#endif  // VELOXA_CORE_IMAGE_IMAGE_DECODER_H_
