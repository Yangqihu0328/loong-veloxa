#include "veloxa/foundation/strings/utf.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace vx {
namespace utf {
namespace {

// --- EncodeUtf8 ---

TEST(EncodeUtf8Test, Ascii) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x41, buf), 1);
  EXPECT_EQ(buf[0], 'A');
}

TEST(EncodeUtf8Test, NullChar) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x00, buf), 1);
  EXPECT_EQ(buf[0], '\0');
}

TEST(EncodeUtf8Test, MaxAscii) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x7F, buf), 1);
  EXPECT_EQ(static_cast<u8>(buf[0]), 0x7F);
}

TEST(EncodeUtf8Test, TwoByte) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x80, buf), 2);
  EXPECT_EQ(static_cast<u8>(buf[0]), 0xC2);
  EXPECT_EQ(static_cast<u8>(buf[1]), 0x80);
}

TEST(EncodeUtf8Test, TwoByteMax) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x7FF, buf), 2);
  EXPECT_EQ(static_cast<u8>(buf[0]), 0xDF);
  EXPECT_EQ(static_cast<u8>(buf[1]), 0xBF);
}

TEST(EncodeUtf8Test, ThreeByte) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x800, buf), 3);
  EXPECT_EQ(static_cast<u8>(buf[0]), 0xE0);
  EXPECT_EQ(static_cast<u8>(buf[1]), 0xA0);
  EXPECT_EQ(static_cast<u8>(buf[2]), 0x80);
}

TEST(EncodeUtf8Test, ThreeByteMax) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0xFFFF, buf), 3);
  EXPECT_EQ(static_cast<u8>(buf[0]), 0xEF);
  EXPECT_EQ(static_cast<u8>(buf[1]), 0xBF);
  EXPECT_EQ(static_cast<u8>(buf[2]), 0xBF);
}

TEST(EncodeUtf8Test, FourByte) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x10000, buf), 4);
  EXPECT_EQ(static_cast<u8>(buf[0]), 0xF0);
  EXPECT_EQ(static_cast<u8>(buf[1]), 0x90);
  EXPECT_EQ(static_cast<u8>(buf[2]), 0x80);
  EXPECT_EQ(static_cast<u8>(buf[3]), 0x80);
}

TEST(EncodeUtf8Test, FourByteMax) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x10FFFF, buf), 4);
  EXPECT_EQ(static_cast<u8>(buf[0]), 0xF4);
  EXPECT_EQ(static_cast<u8>(buf[1]), 0x8F);
  EXPECT_EQ(static_cast<u8>(buf[2]), 0xBF);
  EXPECT_EQ(static_cast<u8>(buf[3]), 0xBF);
}

TEST(EncodeUtf8Test, SurrogateRejected) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0xD800, buf), 0);
  EXPECT_EQ(EncodeUtf8(0xDFFF, buf), 0);
}

TEST(EncodeUtf8Test, OutOfRangeRejected) {
  char buf[4];
  EXPECT_EQ(EncodeUtf8(0x110000, buf), 0);
}

// --- DecodeUtf8 ---

TEST(DecodeUtf8Test, Ascii) {
  usize consumed;
  EXPECT_EQ(DecodeUtf8("A", 1, &consumed), U'A');
  EXPECT_EQ(consumed, 1u);
}

TEST(DecodeUtf8Test, NullByte) {
  const char data[] = {'\0'};
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 1, &consumed), U'\0');
  EXPECT_EQ(consumed, 1u);
}

TEST(DecodeUtf8Test, TwoByte) {
  const char data[] = {'\xC2', '\x80'};
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 2, &consumed), 0x80u);
  EXPECT_EQ(consumed, 2u);
}

TEST(DecodeUtf8Test, ThreeByte) {
  // U+800 = E0 A0 80
  const char data[] = {'\xE0', '\xA0', '\x80'};
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 3, &consumed), 0x800u);
  EXPECT_EQ(consumed, 3u);
}

TEST(DecodeUtf8Test, FourByte) {
  // U+10000 = F0 90 80 80
  const char data[] = {'\xF0', '\x90', '\x80', '\x80'};
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 4, &consumed), 0x10000u);
  EXPECT_EQ(consumed, 4u);
}

TEST(DecodeUtf8Test, MaxCodepoint) {
  // U+10FFFF = F4 8F BF BF
  const char data[] = {'\xF4', '\x8F', '\xBF', '\xBF'};
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 4, &consumed), 0x10FFFFu);
  EXPECT_EQ(consumed, 4u);
}

TEST(DecodeUtf8Test, TruncatedSequence) {
  const char data[] = {'\xC2'};  // 2-byte but only 1 byte available
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 1, &consumed), 0xFFFDu);
}

TEST(DecodeUtf8Test, OverlongTwoByte) {
  // Overlong encoding of U+0 as C0 80
  const char data[] = {'\xC0', '\x80'};
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 2, &consumed), 0xFFFDu);
}

TEST(DecodeUtf8Test, SurrogateRejected) {
  // U+D800 = ED A0 80
  const char data[] = {'\xED', '\xA0', '\x80'};
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 3, &consumed), 0xFFFDu);
}

TEST(DecodeUtf8Test, EmptyInput) {
  usize consumed;
  EXPECT_EQ(DecodeUtf8("", 0, &consumed), 0xFFFDu);
  EXPECT_EQ(consumed, 0u);
}

TEST(DecodeUtf8Test, InvalidLeadByte) {
  const char data[] = {'\xFF'};
  usize consumed;
  EXPECT_EQ(DecodeUtf8(data, 1, &consumed), 0xFFFDu);
  EXPECT_EQ(consumed, 1u);
}

// --- IsValidUtf8 ---

TEST(IsValidUtf8Test, ValidAscii) {
  EXPECT_TRUE(IsValidUtf8("hello", 5));
}

TEST(IsValidUtf8Test, ValidMultibyte) {
  // "日本語" in UTF-8
  const char data[] = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
  EXPECT_TRUE(IsValidUtf8(data, 9));
}

TEST(IsValidUtf8Test, ValidEmoji) {
  // U+1F600 = F0 9F 98 80
  const char data[] = "\xF0\x9F\x98\x80";
  EXPECT_TRUE(IsValidUtf8(data, 4));
}

TEST(IsValidUtf8Test, InvalidContinuation) {
  const char data[] = {'\xC2', '\x00'};
  EXPECT_FALSE(IsValidUtf8(data, 2));
}

TEST(IsValidUtf8Test, InvalidLoneLead) {
  const char data[] = {'\xC2'};
  EXPECT_FALSE(IsValidUtf8(data, 1));
}

TEST(IsValidUtf8Test, EmptyIsValid) {
  EXPECT_TRUE(IsValidUtf8("", 0));
}

// --- Utf8ToUtf16 / Utf16ToUtf8 round-trip ---

TEST(Utf8Utf16Test, AsciiRoundTrip) {
  const char* src = "Hello";
  char16_t u16buf[16];
  usize u16len = Utf8ToUtf16(src, 5, u16buf, 16);
  EXPECT_EQ(u16len, 5u);

  char u8buf[32];
  usize u8len = Utf16ToUtf8(u16buf, u16len, u8buf, 32);
  EXPECT_EQ(u8len, 5u);
  EXPECT_EQ(std::string(u8buf, u8len), "Hello");
}

TEST(Utf8Utf16Test, BmpRoundTrip) {
  // U+4E16 (世) = E4 B8 96
  const char src[] = "\xE4\xB8\x96";
  char16_t u16buf[4];
  usize u16len = Utf8ToUtf16(src, 3, u16buf, 4);
  EXPECT_EQ(u16len, 1u);
  EXPECT_EQ(u16buf[0], 0x4E16);

  char u8buf[8];
  usize u8len = Utf16ToUtf8(u16buf, u16len, u8buf, 8);
  EXPECT_EQ(u8len, 3u);
  EXPECT_EQ(std::memcmp(u8buf, src, 3), 0);
}

TEST(Utf8Utf16Test, SupplementaryRoundTrip) {
  // U+1F600 = F0 9F 98 80
  const char src[] = "\xF0\x9F\x98\x80";
  char16_t u16buf[4];
  usize u16len = Utf8ToUtf16(src, 4, u16buf, 4);
  EXPECT_EQ(u16len, 2u);
  EXPECT_EQ(u16buf[0], 0xD83D);
  EXPECT_EQ(u16buf[1], 0xDE00);

  char u8buf[8];
  usize u8len = Utf16ToUtf8(u16buf, u16len, u8buf, 8);
  EXPECT_EQ(u8len, 4u);
  EXPECT_EQ(std::memcmp(u8buf, src, 4), 0);
}

TEST(Utf8Utf16Test, NullDstCountsOnly) {
  const char* src = "Hello";
  usize u16len = Utf8ToUtf16(src, 5, nullptr, 0);
  EXPECT_EQ(u16len, 5u);
}

// --- Utf8Iterator ---

TEST(Utf8IteratorTest, AsciiIteration) {
  const char* data = "abc";
  std::vector<char32_t> cps;
  Utf8Iterator it(data, 3);
  Utf8Iterator end;
  for (; it != end; ++it) {
    cps.push_back(*it);
  }
  EXPECT_EQ(cps.size(), 3u);
  EXPECT_EQ(cps[0], U'a');
  EXPECT_EQ(cps[1], U'b');
  EXPECT_EQ(cps[2], U'c');
}

TEST(Utf8IteratorTest, MultibyteMixed) {
  // "A世😀" = 41 E4B896 F09F9880
  const char data[] = "A\xE4\xB8\x96\xF0\x9F\x98\x80";
  std::vector<char32_t> cps;
  Utf8Iterator it(data, 8);
  Utf8Iterator end;
  for (; it != end; ++it) {
    cps.push_back(*it);
  }
  EXPECT_EQ(cps.size(), 3u);
  EXPECT_EQ(cps[0], U'A');
  EXPECT_EQ(cps[1], U'\u4E16');
  EXPECT_EQ(cps[2], U'\U0001F600');
}

TEST(Utf8IteratorTest, EmptyString) {
  Utf8Iterator it("", 0);
  Utf8Iterator end;
  EXPECT_FALSE(it != end);
}

}  // namespace
}  // namespace utf
}  // namespace vx
