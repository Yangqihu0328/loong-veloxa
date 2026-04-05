#include "veloxa/foundation/base/status.h"

#include <gtest/gtest.h>

namespace vx {
namespace {

TEST(StatusTest, DefaultIsOk) {
  Status s;
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kOk);
}

TEST(StatusTest, OkFactory) {
  auto s = Status::Ok();
  EXPECT_TRUE(s.ok());
}

TEST(StatusTest, ErrorStatus) {
  Status s(StatusCode::kNotFound, "item missing");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kNotFound);
  EXPECT_EQ(s.message(), "item missing");
}

TEST(StatusTest, OutOfMemory) {
  Status s(StatusCode::kOutOfMemory, "allocation failed");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kOutOfMemory);
}

TEST(StatusOrTest, HoldsValue) {
  StatusOr<int> result(42);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), 42);
}

TEST(StatusOrTest, HoldsStringValue) {
  StatusOr<std::string> result(std::string("hello"));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "hello");
}

TEST(StatusOrTest, HoldsError) {
  StatusOr<int> result(Status(StatusCode::kNotFound, "not found"));
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kNotFound);
}

TEST(StatusOrTest, MoveValue) {
  std::string original = "test string with enough length to avoid SSO";
  StatusOr<std::string> result(std::move(original));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "test string with enough length to avoid SSO");
}

TEST(StatusOrTest, CopyConstruction) {
  StatusOr<int> a(42);
  StatusOr<int> b(a);
  EXPECT_TRUE(b.ok());
  EXPECT_EQ(b.value(), 42);
}

TEST(StatusOrTest, MoveConstruction) {
  StatusOr<std::string> a(std::string("hello"));
  StatusOr<std::string> b(std::move(a));
  EXPECT_TRUE(b.ok());
  EXPECT_EQ(b.value(), "hello");
}

TEST(StatusOrTest, ValueOnErrorAborts) {
  StatusOr<int> result(Status(StatusCode::kInternal, "fail"));
  EXPECT_DEATH(result.value(), "");
}

}  // namespace
}  // namespace vx
