#include "veloxa/core/image/image_decoder.h"

#include <cstdio>
#include <cstring>

#include <jpeglib.h>
#include <png.h>

#include "veloxa/foundation/containers/vector.h"

namespace vx::image {

namespace {

struct PngReadContext {
  const u8* data;
  usize offset;
  usize len;
};

void PngReadFromMemory(png_structp png, png_bytep out, png_size_t count) {
  auto* ctx = static_cast<PngReadContext*>(png_get_io_ptr(png));
  if (ctx->offset + count > ctx->len) {
    png_error(png, "read past end of buffer");
    return;
  }
  std::memcpy(out, ctx->data + ctx->offset, count);
  ctx->offset += count;
}

StatusOr<gfx::Image> DecodePngFromMemory(const u8* data, usize len) {
  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png) {
    return Status(StatusCode::kInternal, "png_create_read_struct failed");
  }

  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_read_struct(&png, nullptr, nullptr);
    return Status(StatusCode::kInternal, "png_create_info_struct failed");
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, nullptr);
    return Status(StatusCode::kInternal, "PNG decoding error");
  }

  PngReadContext ctx{data, 0, len};
  png_set_read_fn(png, &ctx, PngReadFromMemory);
  png_read_info(png, info);

  u32 width = png_get_image_width(png, info);
  u32 height = png_get_image_height(png, info);

  png_set_expand(png);
  png_set_gray_to_rgb(png);
  png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
  png_set_strip_16(png);
  png_read_update_info(png, info);

  gfx::Image image(width, height);
  Vector<png_bytep> row_ptrs;
  row_ptrs.reserve(height);
  for (u32 y = 0; y < height; ++y) {
    row_ptrs.push_back(
        reinterpret_cast<png_bytep>(image.pixels()) + y * width * 4);
  }
  png_read_image(png, row_ptrs.data());
  png_read_end(png, info);
  png_destroy_read_struct(&png, &info, nullptr);

  return static_cast<gfx::Image&&>(image);
}

StatusOr<gfx::Image> DecodeJpegFromMemory(const u8* data, usize len) {
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, data, static_cast<unsigned long>(len));

  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&cinfo);
    return Status(StatusCode::kInternal, "JPEG header read failed");
  }

  cinfo.out_color_space = JCS_RGB;
  jpeg_start_decompress(&cinfo);

  u32 width = cinfo.output_width;
  u32 height = cinfo.output_height;
  int channels = cinfo.output_components;

  gfx::Image image(width, height);
  Vector<u8> row_buffer;
  row_buffer.resize(static_cast<usize>(width) * channels);

  u32 y = 0;
  while (cinfo.output_scanline < cinfo.output_height) {
    u8* row_ptr = row_buffer.data();
    jpeg_read_scanlines(&cinfo, &row_ptr, 1);

    u32* dest = image.pixels() + static_cast<usize>(y) * width;
    for (u32 x = 0; x < width; ++x) {
      u8 r = row_buffer[x * channels];
      u8 g = row_buffer[x * channels + 1];
      u8 b = row_buffer[x * channels + 2];
      dest[x] = static_cast<u32>(r) | (static_cast<u32>(g) << 8) |
                (static_cast<u32>(b) << 16) | (0xFFu << 24);
    }
    ++y;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  return static_cast<gfx::Image&&>(image);
}

}  // namespace

StatusOr<gfx::Image> DecodeFromMemory(const u8* data, usize len) {
  if (len >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' &&
      data[3] == 'G') {
    return DecodePngFromMemory(data, len);
  }
  if (len >= 2 && data[0] == 0xFF && data[1] == 0xD8) {
    return DecodeJpegFromMemory(data, len);
  }
  return Status(StatusCode::kInvalidArgument, "unsupported image format");
}

StatusOr<gfx::Image> DecodeFromFile(StringView path, usize max_size) {
  char path_buf[4096];
  if (path.size() >= sizeof(path_buf)) {
    return Status(StatusCode::kInvalidArgument, "path too long");
  }
  std::memcpy(path_buf, path.data(), path.size());
  path_buf[path.size()] = '\0';

  FILE* fp = std::fopen(path_buf, "rb");
  if (!fp) {
    return Status(StatusCode::kNotFound, "cannot open image file");
  }

  std::fseek(fp, 0, SEEK_END);
  long file_size = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);

  if (file_size <= 0) {
    std::fclose(fp);
    return Status(StatusCode::kInternal, "empty or unreadable file");
  }

  // 守卫：拒绝超出 max_size 的文件，防止一次 fread 申请超大 buffer 触发
  // ArenaAllocator/Vector OOM abort（嵌入端无 std::bad_alloc 恢复路径）。
  if (static_cast<usize>(file_size) > max_size) {
    std::fclose(fp);
    return Status(StatusCode::kOutOfMemory,
                  "image file exceeds max_size guard");
  }

  Vector<u8> buffer;
  buffer.resize(static_cast<usize>(file_size));
  usize read_count = std::fread(buffer.data(), 1, buffer.size(), fp);
  std::fclose(fp);

  if (read_count != buffer.size()) {
    return Status(StatusCode::kInternal, "failed to read entire file");
  }

  return DecodeFromMemory(buffer.data(), buffer.size());
}

}  // namespace vx::image
