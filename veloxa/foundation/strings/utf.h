#ifndef VELOXA_FOUNDATION_STRINGS_UTF_H_
#define VELOXA_FOUNDATION_STRINGS_UTF_H_

#include "veloxa/foundation/base/types.h"

namespace vx {
namespace utf {

// Encodes a Unicode codepoint as UTF-8. Writes up to 4 bytes to |out|.
// Returns the number of bytes written (1-4), or 0 on error.
int EncodeUtf8(char32_t codepoint, char* out);

// Decodes one UTF-8 codepoint from |data| of length |len|.
// Sets |*bytes_consumed| to the number of bytes consumed.
// Returns the decoded codepoint, or 0xFFFD on error.
char32_t DecodeUtf8(const char* data, usize len, usize* bytes_consumed);

// Returns true if the byte sequence is valid UTF-8.
bool IsValidUtf8(const char* data, usize len);

// Converts UTF-8 to UTF-16. Returns the number of char16_t units written.
// If |dst| is null, returns the number of units needed.
usize Utf8ToUtf16(const char* src, usize src_len, char16_t* dst,
                  usize dst_cap);

// Converts UTF-16 to UTF-8. Returns the number of bytes written.
// If |dst| is null, returns the number of bytes needed.
usize Utf16ToUtf8(const char16_t* src, usize src_len, char* dst,
                  usize dst_cap);

class Utf8Iterator {
 public:
  Utf8Iterator(const char* data, usize size);
  Utf8Iterator();

  char32_t operator*() const;
  Utf8Iterator& operator++();
  bool operator!=(const Utf8Iterator& other) const;

 private:
  const char* data_;
  usize remaining_;
};

}  // namespace utf
}  // namespace vx

#endif  // VELOXA_FOUNDATION_STRINGS_UTF_H_
