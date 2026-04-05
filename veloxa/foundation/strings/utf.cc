#include "veloxa/foundation/strings/utf.h"

namespace vx {
namespace utf {

int EncodeUtf8(char32_t cp, char* out) {
  if (cp <= 0x7F) {
    out[0] = static_cast<char>(cp);
    return 1;
  }
  if (cp <= 0x7FF) {
    out[0] = static_cast<char>(0xC0 | (cp >> 6));
    out[1] = static_cast<char>(0x80 | (cp & 0x3F));
    return 2;
  }
  if (cp <= 0xFFFF) {
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0;  // surrogates
    out[0] = static_cast<char>(0xE0 | (cp >> 12));
    out[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out[2] = static_cast<char>(0x80 | (cp & 0x3F));
    return 3;
  }
  if (cp <= 0x10FFFF) {
    out[0] = static_cast<char>(0xF0 | (cp >> 18));
    out[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return 4;
  }
  return 0;
}

char32_t DecodeUtf8(const char* data, usize len, usize* bytes_consumed) {
  if (len == 0) {
    *bytes_consumed = 0;
    return 0xFFFD;
  }

  auto b0 = static_cast<u8>(data[0]);

  // 1-byte (ASCII)
  if (b0 <= 0x7F) {
    *bytes_consumed = 1;
    return static_cast<char32_t>(b0);
  }

  // Determine expected sequence length
  usize seq_len;
  char32_t cp;
  if ((b0 & 0xE0) == 0xC0) {
    seq_len = 2;
    cp = b0 & 0x1F;
  } else if ((b0 & 0xF0) == 0xE0) {
    seq_len = 3;
    cp = b0 & 0x0F;
  } else if ((b0 & 0xF8) == 0xF0) {
    seq_len = 4;
    cp = b0 & 0x07;
  } else {
    // Invalid lead byte (continuation byte or 0xFE/0xFF)
    *bytes_consumed = 1;
    return 0xFFFD;
  }

  if (seq_len > len) {
    *bytes_consumed = 1;
    return 0xFFFD;
  }

  for (usize i = 1; i < seq_len; ++i) {
    auto bi = static_cast<u8>(data[i]);
    if ((bi & 0xC0) != 0x80) {
      *bytes_consumed = 1;
      return 0xFFFD;
    }
    cp = (cp << 6) | (bi & 0x3F);
  }

  // Reject overlong encodings
  if (seq_len == 2 && cp < 0x80) {
    *bytes_consumed = seq_len;
    return 0xFFFD;
  }
  if (seq_len == 3 && cp < 0x800) {
    *bytes_consumed = seq_len;
    return 0xFFFD;
  }
  if (seq_len == 4 && cp < 0x10000) {
    *bytes_consumed = seq_len;
    return 0xFFFD;
  }

  // Reject surrogates
  if (cp >= 0xD800 && cp <= 0xDFFF) {
    *bytes_consumed = seq_len;
    return 0xFFFD;
  }

  // Reject out of range
  if (cp > 0x10FFFF) {
    *bytes_consumed = seq_len;
    return 0xFFFD;
  }

  *bytes_consumed = seq_len;
  return cp;
}

bool IsValidUtf8(const char* data, usize len) {
  usize i = 0;
  while (i < len) {
    usize consumed;
    char32_t cp = DecodeUtf8(data + i, len - i, &consumed);
    if (cp == 0xFFFD && !(consumed == 1 && static_cast<u8>(data[i]) <= 0x7F)) {
      // 0xFFFD is a valid codepoint only if decoded from real U+FFFD sequence
      // Check if the bytes actually encode U+FFFD (EF BF BD)
      if (consumed < 3 || static_cast<u8>(data[i]) != 0xEF ||
          static_cast<u8>(data[i + 1]) != 0xBF ||
          static_cast<u8>(data[i + 2]) != 0xBD) {
        return false;
      }
    }
    i += consumed;
  }
  return true;
}

usize Utf8ToUtf16(const char* src, usize src_len, char16_t* dst,
                  usize dst_cap) {
  usize si = 0;
  usize di = 0;
  while (si < src_len) {
    usize consumed;
    char32_t cp = DecodeUtf8(src + si, src_len - si, &consumed);
    si += consumed;

    if (cp <= 0xFFFF) {
      if (dst) {
        if (di >= dst_cap) break;
        dst[di] = static_cast<char16_t>(cp);
      }
      ++di;
    } else {
      // Supplementary: encode as surrogate pair
      cp -= 0x10000;
      if (dst) {
        if (di + 1 >= dst_cap) break;
        dst[di] = static_cast<char16_t>(0xD800 | (cp >> 10));
        dst[di + 1] = static_cast<char16_t>(0xDC00 | (cp & 0x3FF));
      }
      di += 2;
    }
  }
  return di;
}

usize Utf16ToUtf8(const char16_t* src, usize src_len, char* dst,
                  usize dst_cap) {
  usize si = 0;
  usize di = 0;
  while (si < src_len) {
    char32_t cp;
    auto u = static_cast<u16>(src[si]);

    if (u >= 0xD800 && u <= 0xDBFF) {
      // High surrogate
      if (si + 1 < src_len) {
        auto u2 = static_cast<u16>(src[si + 1]);
        if (u2 >= 0xDC00 && u2 <= 0xDFFF) {
          cp = 0x10000 + ((static_cast<char32_t>(u - 0xD800) << 10) |
                          (u2 - 0xDC00));
          si += 2;
        } else {
          cp = 0xFFFD;
          ++si;
        }
      } else {
        cp = 0xFFFD;
        ++si;
      }
    } else if (u >= 0xDC00 && u <= 0xDFFF) {
      cp = 0xFFFD;
      ++si;
    } else {
      cp = u;
      ++si;
    }

    char buf[4];
    int n = EncodeUtf8(cp, buf);
    if (dst) {
      if (di + static_cast<usize>(n) > dst_cap) break;
      for (int j = 0; j < n; ++j) {
        dst[di + static_cast<usize>(j)] = buf[j];
      }
    }
    di += static_cast<usize>(n);
  }
  return di;
}

Utf8Iterator::Utf8Iterator(const char* data, usize size)
    : data_(data), remaining_(size) {}

Utf8Iterator::Utf8Iterator() : data_(nullptr), remaining_(0) {}

char32_t Utf8Iterator::operator*() const {
  usize consumed;
  return DecodeUtf8(data_, remaining_, &consumed);
}

Utf8Iterator& Utf8Iterator::operator++() {
  usize consumed;
  DecodeUtf8(data_, remaining_, &consumed);
  data_ += consumed;
  remaining_ -= consumed;
  return *this;
}

bool Utf8Iterator::operator!=(const Utf8Iterator& other) const {
  return remaining_ != other.remaining_;
}

}  // namespace utf
}  // namespace vx
