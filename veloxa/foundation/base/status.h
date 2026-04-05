#ifndef VELOXA_FOUNDATION_BASE_STATUS_H_
#define VELOXA_FOUNDATION_BASE_STATUS_H_

#include <string>
#include <utility>

#include "veloxa/foundation/base/assert.h"
#include "veloxa/foundation/base/types.h"

namespace vx {

enum class StatusCode : u8 {
  kOk = 0,
  kInvalidArgument,
  kOutOfMemory,
  kNotFound,
  kAlreadyExists,
  kInternal,
};

class Status {
 public:
  Status() : code_(StatusCode::kOk) {}
  Status(StatusCode code, std::string message)
      : code_(code), message_(std::move(message)) {}

  static Status Ok() { return Status(); }

  bool ok() const { return code_ == StatusCode::kOk; }
  StatusCode code() const { return code_; }
  const std::string& message() const { return message_; }

 private:
  StatusCode code_;
  std::string message_;
};

template <typename T>
class StatusOr {
 public:
  StatusOr(const T& value) : has_value_(true) { new (&storage_) T(value); }

  StatusOr(T&& value) : has_value_(true) {
    new (&storage_) T(std::move(value));
  }

  StatusOr(Status status) : has_value_(false), status_(std::move(status)) {
    VX_DCHECK(!status_.ok());
  }

  StatusOr(const StatusOr& other) : has_value_(other.has_value_) {
    if (has_value_) {
      new (&storage_) T(other.ValueRef());
    } else {
      status_ = other.status_;
    }
  }

  StatusOr(StatusOr&& other) noexcept : has_value_(other.has_value_) {
    if (has_value_) {
      new (&storage_) T(std::move(other.ValueRef()));
    } else {
      status_ = std::move(other.status_);
    }
  }

  StatusOr& operator=(const StatusOr& other) {
    if (this != &other) {
      Destroy();
      has_value_ = other.has_value_;
      if (has_value_) {
        new (&storage_) T(other.ValueRef());
      } else {
        status_ = other.status_;
      }
    }
    return *this;
  }

  StatusOr& operator=(StatusOr&& other) noexcept {
    if (this != &other) {
      Destroy();
      has_value_ = other.has_value_;
      if (has_value_) {
        new (&storage_) T(std::move(other.ValueRef()));
      } else {
        status_ = std::move(other.status_);
      }
    }
    return *this;
  }

  ~StatusOr() { Destroy(); }

  bool ok() const { return has_value_; }

  const T& value() const {
    VX_CHECK(has_value_) << "Accessing value of error StatusOr";
    return ValueRef();
  }

  T& value() {
    VX_CHECK(has_value_) << "Accessing value of error StatusOr";
    return ValueRef();
  }

  const Status& status() const {
    VX_DCHECK(!has_value_);
    return status_;
  }

 private:
  void Destroy() {
    if (has_value_) {
      ValueRef().~T();
    }
  }

  T& ValueRef() { return *reinterpret_cast<T*>(&storage_); }
  const T& ValueRef() const { return *reinterpret_cast<const T*>(&storage_); }

  bool has_value_;
  union {
    Status status_;
    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;
  };
};

}  // namespace vx

#endif  // VELOXA_FOUNDATION_BASE_STATUS_H_
