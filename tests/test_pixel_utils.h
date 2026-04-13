#ifndef VELOXA_TESTS_TEST_PIXEL_UTILS_H_
#define VELOXA_TESTS_TEST_PIXEL_UTILS_H_

#include "veloxa/foundation/base/types.h"

namespace vx {
namespace test {

// RGBA32 与 SoftwareCanvas / gfx::Color::ToRGBA32 一致：R 低字节 … A 高字节
inline u8 PixelR(u32 rgba) { return static_cast<u8>(rgba & 0xFFu); }
inline u8 PixelG(u32 rgba) { return static_cast<u8>((rgba >> 8) & 0xFFu); }
inline u8 PixelB(u32 rgba) { return static_cast<u8>((rgba >> 16) & 0xFFu); }
inline u8 PixelA(u32 rgba) { return static_cast<u8>((rgba >> 24) & 0xFFu); }

}  // namespace test
}  // namespace vx

#endif  // VELOXA_TESTS_TEST_PIXEL_UTILS_H_
