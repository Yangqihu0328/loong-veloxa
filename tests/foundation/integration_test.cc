#include "veloxa/foundation/base/span.h"
#include "veloxa/foundation/containers/hash_map.h"
#include "veloxa/foundation/containers/vector.h"
#include "veloxa/foundation/memory/arena_allocator.h"
#include "veloxa/foundation/memory/pool_allocator.h"
#include "veloxa/foundation/strings/interned_string.h"
#include "veloxa/foundation/strings/string.h"
#include "veloxa/foundation/strings/string_view.h"
#include "veloxa/foundation/strings/utf.h"
#include "veloxa/foundation/log/log.h"

#include <gtest/gtest.h>

#include <string>

namespace vx {
namespace {

// Test 1: ArenaAllocator + Vector
// Create ArenaAllocator, create Vector using arena, push_back many elements,
// verify data integrity, then arena.Reset() — all memory freed at once.
TEST(IntegrationTest, ArenaWithVector) {
  ArenaAllocator arena(4096);
  Vector<int, ArenaAllocator> vec(&arena);
  for (int i = 0; i < 1000; ++i) {
    vec.push_back(i);
  }
  EXPECT_EQ(vec.size(), 1000u);
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(vec[i], i);
  }
  // Arena manages all memory — no individual frees needed
  EXPECT_GT(arena.bytes_allocated(), 0u);
}

// Test 2: PoolAllocator + custom struct allocation
// Allocate/deallocate structs from pool, verify reuse
TEST(IntegrationTest, PoolWithStructs) {
  struct Widget {
    int id;
    float x;
    float y;
  };
  PoolAllocator pool(sizeof(Widget), 32);
  void* p1 = pool.Allocate(sizeof(Widget));
  auto* w = new (p1) Widget{42, 1.0F, 2.0F};
  EXPECT_EQ(w->id, 42);
  w->~Widget();
  pool.Deallocate(p1, sizeof(Widget));

  void* p2 = pool.Allocate(sizeof(Widget));
  EXPECT_EQ(p1, p2);  // reuse
  pool.Deallocate(p2, sizeof(Widget));
}

// Test 3: HashMap with String keys and values (simulating CSS property table)
TEST(IntegrationTest, HashMapStringProperties) {
  HashMap<std::string, std::string> props;
  props.Insert("color", "red");
  props.Insert("font-size", "14px");
  props.Insert("display", "flex");
  props.Insert("margin", "10px 20px");

  ASSERT_TRUE(props.Contains("color"));
  EXPECT_EQ(*props.Find("color"), "red");
  EXPECT_EQ(*props.Find("font-size"), "14px");
  EXPECT_FALSE(props.Contains("padding"));
}

// Test 4: String + UTF-8 Chinese text
TEST(IntegrationTest, StringWithUtf8Chinese) {
  String s("你好世界");  // 4 Chinese chars = 12 UTF-8 bytes
  EXPECT_EQ(s.size(), 12u);
  EXPECT_TRUE(utf::IsValidUtf8(s.data(), s.size()));

  StringView sv = s.view();
  EXPECT_TRUE(sv.starts_with("\xe4\xbd\xa0"));  // 你

  // Count codepoints via iterator
  int count = 0;
  for (auto it = utf::Utf8Iterator(s.data(), s.size()); it != utf::Utf8Iterator();
       ++it) {
    ++count;
  }
  EXPECT_EQ(count, 4);
}

// Test 5: InternedString in HashMap
TEST(IntegrationTest, InternedStringAsKey) {
  InternedString::ClearInternedStrings();

  auto key1 = InternedString::Intern("width");
  auto key2 = InternedString::Intern("height");
  auto key3 = InternedString::Intern("width");

  EXPECT_EQ(key1, key3);
  EXPECT_NE(key1, key2);

  // Use string view from interned string
  EXPECT_EQ(key1.view(), StringView("width"));

  InternedString::ClearInternedStrings();
}

// Test 6: Vector of Strings
TEST(IntegrationTest, VectorOfStrings) {
  Vector<String> names;
  names.push_back(String("Alice"));
  names.push_back(String("Bob"));
  names.push_back(String("Charlie has a very long name that exceeds SSO"));

  EXPECT_EQ(names.size(), 3u);
  EXPECT_EQ(names[0].view(), StringView("Alice"));
  EXPECT_EQ(names[2].view(),
            StringView("Charlie has a very long name that exceeds SSO"));
}

// Test 7: Log + String formatting
TEST(IntegrationTest, LogWithStringData) {
  struct TestSink : public LogSink {
    std::string last_msg;
    void Write(LogLevel /*level*/, const char* /*file*/, int /*line*/,
               const char* msg) override {
      last_msg = msg;
    }
  };
  TestSink sink;
  SetLogSink(&sink);

  String name("Veloxa");
  VX_LOG_INFO("Engine: %s, version: %d.%d", name.c_str(), 0, 1);
  EXPECT_EQ(sink.last_msg, "Engine: Veloxa, version: 0.1");

  SetLogSink(nullptr);
}

// Test 8: Span over Vector data
TEST(IntegrationTest, SpanOverVector) {
  Vector<int> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  Span<int> full(data.data(), data.size());
  auto first_half = full.subspan(0, 5);
  auto second_half = full.subspan(5);

  EXPECT_EQ(first_half.size(), 5u);
  EXPECT_EQ(second_half.size(), 5u);
  EXPECT_EQ(first_half[0], 1);
  EXPECT_EQ(second_half[0], 6);
}

}  // namespace
}  // namespace vx
