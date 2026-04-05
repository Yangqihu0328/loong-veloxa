#ifndef VELOXA_FOUNDATION_MEMORY_ALLOCATOR_H_
#define VELOXA_FOUNDATION_MEMORY_ALLOCATOR_H_

#include <cstddef>
#include <type_traits>

#include "veloxa/foundation/base/types.h"

namespace vx {

template <typename A>
struct IsAllocator {
  template <typename T>
  static auto TestAllocate(int)
      -> decltype(std::declval<T>().Allocate(usize{}, usize{}), std::true_type{});
  template <typename>
  static std::false_type TestAllocate(...);

  template <typename T>
  static auto TestDeallocate(int)
      -> decltype(std::declval<T>().Deallocate(std::declval<void*>(), usize{}),
                  std::true_type{});
  template <typename>
  static std::false_type TestDeallocate(...);

  static constexpr bool value =
      decltype(TestAllocate<A>(0))::value && decltype(TestDeallocate<A>(0))::value;
};

template <typename A>
inline constexpr bool kIsAllocator = IsAllocator<A>::value;

}  // namespace vx

#endif  // VELOXA_FOUNDATION_MEMORY_ALLOCATOR_H_
