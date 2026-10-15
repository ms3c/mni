#pragma once

// Overflow-checked 64-bit integer arithmetic. Each function returns true if the exact result does
// not fit in an int64 (out is only valid when it returns false).
//
// The *Portable versions work with any compiler and are always compiled so tests can run them
// everywhere; the plain names use the compiler builtins when there are some.

#include <cstdint>
#include <limits>

namespace mni {

  inline bool addOverflowsPortable(int64_t a, int64_t b, int64_t& out) {
    constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
    constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
    if ((b > 0 && a > kMax - b) || (b < 0 && a < kMin - b)) return true;
    out = a + b;
    return false;
  }

  inline bool subOverflowsPortable(int64_t a, int64_t b, int64_t& out) {
    constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
    constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
    if ((b < 0 && a > kMax + b) || (b > 0 && a < kMin + b)) return true;
    out = a - b;
    return false;
  }

  inline bool mulOverflowsPortable(int64_t a, int64_t b, int64_t& out) {
    constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
    constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
    if (a == 0 || b == 0) {
      out = 0;
      return false;
    }
    // -1 is special: kMin / -1 and kMin * -1 do not fit.
    if (a == -1 || b == -1) {
      const int64_t other = a == -1 ? b : a;
      if (other == kMin) return true;
      out = -other;
      return false;
    }
    if (a > 0 ? (b > 0 ? a > kMax / b : b < kMin / a) : (b > 0 ? a < kMin / b : a < kMax / b))
      return true;
    out = a * b;
    return false;
  }

#if defined(__GNUC__) || defined(__clang__)
  inline bool addOverflows(int64_t a, int64_t b, int64_t& out) {
    return __builtin_add_overflow(a, b, &out);
  }
  inline bool subOverflows(int64_t a, int64_t b, int64_t& out) {
    return __builtin_sub_overflow(a, b, &out);
  }
  inline bool mulOverflows(int64_t a, int64_t b, int64_t& out) {
    return __builtin_mul_overflow(a, b, &out);
  }
#else
  inline bool addOverflows(int64_t a, int64_t b, int64_t& out) {
    return addOverflowsPortable(a, b, out);
  }
  inline bool subOverflows(int64_t a, int64_t b, int64_t& out) {
    return subOverflowsPortable(a, b, out);
  }
  inline bool mulOverflows(int64_t a, int64_t b, int64_t& out) {
    return mulOverflowsPortable(a, b, out);
  }
#endif

}  // namespace mni
