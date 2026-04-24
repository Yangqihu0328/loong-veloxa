// veloxa/foundation/base/hash.h
//
// Byte-level hashing primitives. Introduced by TASK-20260424-04 for the
// DrawText hb_shape result cache; suitable for short-lived, per-process
// fingerprinting of small inputs (UI text, identifiers, etc.).
//
// NOT suitable for:
//   - Cryptographic use (authentication, integrity, password hashing)
//   - Adversarial-input resistance (no keying; deterministic output)
//   - Persistent hash tables across ABI / architecture boundaries
//
// Algorithm: FNV-1a 64-bit.
//   h0 = 0xCBF29CE484222325 (FNV offset basis)
//   hi = (h_{i-1} XOR byte_i) * 0x100000001B3 (FNV prime)
//
// Contract:
//   - HashBytesU64(nullptr, 0) == HashBytesU64(any_ptr, 0) == offset basis
//   - Pure function; deterministic given (bytes, len)
//   - len in bytes, not code units
//
// Performance: one iadd + ixor + imul per byte; ~1 cycle/byte on modern x86.

#ifndef VELOXA_FOUNDATION_BASE_HASH_H_
#define VELOXA_FOUNDATION_BASE_HASH_H_

#include "veloxa/foundation/base/types.h"

namespace vx {

inline u64 HashBytesU64(const u8* data, usize len) {
  u64 h = 0xCBF29CE484222325ULL;
  for (usize i = 0; i < len; ++i) {
    h ^= static_cast<u64>(data[i]);
    h *= 0x100000001B3ULL;
  }
  return h;
}

}  // namespace vx

#endif  // VELOXA_FOUNDATION_BASE_HASH_H_
