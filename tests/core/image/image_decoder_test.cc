#include <gtest/gtest.h>

#include <cstdio>

#include <png.h>

#include "veloxa/core/image/image_decoder.h"
#include "veloxa/graphics/types.h"

namespace vx::image {

static std::string CreateTestPng() {
  char path[256];
  std::snprintf(path, sizeof(path), "/tmp/vx_test_%d.png", getpid());

  FILE* fp = std::fopen(path, "wb");
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                            nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  png_init_io(png, fp);
  png_set_IHDR(png, info, 4, 4, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  u8 row[16];
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      row[j * 4 + 0] = 255;
      row[j * 4 + 1] = 0;
      row[j * 4 + 2] = 0;
      row[j * 4 + 3] = 255;
    }
    png_bytep rp = row;
    png_write_rows(png, &rp, 1);
  }

  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  std::fclose(fp);
  return path;
}

TEST(ImageDecoderTest, DecodePng) {
  auto path = CreateTestPng();
  auto result = DecodeFromFile(StringView(path.c_str()));
  ASSERT_TRUE(result.ok());
  auto& img = result.value();
  EXPECT_EQ(img.width(), 4u);
  EXPECT_EQ(img.height(), 4u);
  EXPECT_TRUE(img.valid());

  u32 expected = gfx::Color::FromRGBA(255, 0, 0, 255).ToRGBA32();
  EXPECT_EQ(img.pixels()[0], expected);
  std::remove(path.c_str());
}

TEST(ImageDecoderTest, DecodeInvalidPath) {
  auto result = DecodeFromFile("/nonexistent/image.png");
  EXPECT_FALSE(result.ok());
}

TEST(ImageDecoderTest, DecodeInvalidData) {
  u8 garbage[] = {1, 2, 3, 4, 5};
  auto result = DecodeFromMemory(garbage, sizeof(garbage));
  EXPECT_FALSE(result.ok());
}

}  // namespace vx::image
