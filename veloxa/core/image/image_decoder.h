#ifndef VELOXA_CORE_IMAGE_IMAGE_DECODER_H_
#define VELOXA_CORE_IMAGE_IMAGE_DECODER_H_

#include "veloxa/foundation/base/status.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/graphics/image.h"

namespace vx::image {

// 默认上限 256 MiB — 嵌入式 / 自动 HMI 资产合理上限；防御性拒绝来自不可信源的
// 任意大文件（避免一次 fread 申请几 GiB 触发 OOM abort）。
inline constexpr usize kDefaultMaxImageFileSize = 256u * 1024u * 1024u;

StatusOr<gfx::Image> DecodeFromFile(StringView path,
                                    usize max_size = kDefaultMaxImageFileSize);
StatusOr<gfx::Image> DecodeFromMemory(const u8* data, usize len);

}  // namespace vx::image

#endif  // VELOXA_CORE_IMAGE_IMAGE_DECODER_H_
