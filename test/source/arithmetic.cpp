#include <doctest/doctest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "checked_arithmetic.h"

using namespace mni;

namespace {
  constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
  constexpr int64_t kMin = std::numeric_limits<int64_t>::min();

  struct Case {
    int64_t a, b;
    bool overflows;
    int64_t result;  // when it does not overflow
  };

  // Each portable check and the one the interpreter really uses must agree with these.
  template <class Check> void run(Check check, const std::vector<Case>& cases) {
    for (const Case& c : cases) {
      INFO("a = " << c.a << ", b = " << c.b);
      int64_t out = 0;
      CHECK(check(c.a, c.b, out) == c.overflows);
      if (!c.overflows) CHECK(out == c.result);
    }
  }

  const std::vector<Case> kMulCases = {
      {0, 0, false, 0},
      {0, kMin, false, 0},
      {kMax, 0, false, 0},
      {1, kMax, false, kMax},
      {kMin, 1, false, kMin},
      {-1, kMax, false, -kMax},
      {kMax, -1, false, -kMax},
      {-1, kMin, true, 0},
      {kMin, -1, true, 0},
      {kMax, 2, true, 0},
      {2, kMax, true, 0},
      {kMin, 2, true, 0},
      {kMin / 2, 2, false, kMin},
      {kMin / 2, -2, true, 0},
      {kMax / 2, 2, false, kMax - 1},
      {3037000499, 3037000499, false, 9223372030926249001},  // just fits
      {3037000500, 3037000500, true, 0},                     // just does not
      {-3037000499, 3037000499, false, -9223372030926249001},
      {-3037000500, 3037000500, true, 0},
      {-3037000500, -3037000500, true, 0},
      {-3037000499, -3037000499, false, 9223372030926249001},
      {4611686018427387904, 2, true, 0},
      {4611686018427387903, 2, false, 9223372036854775806},
      {-4611686018427387904, 2, false, kMin},
      {-4611686018427387905, 2, true, 0},
  };

  const std::vector<Case> kAddCases = {
      {kMax, 0, false, kMax},      {kMax, 1, true, 0},      {kMin, -1, true, 0},
      {kMin, 0, false, kMin},      {kMax, kMin, false, -1}, {kMax - 1, 1, false, kMax},
      {kMin + 1, -1, false, kMin}, {1, 2, false, 3},
  };

  const std::vector<Case> kSubCases = {
      {kMin, 1, true, 0},         {kMax, -1, true, 0},    {0, kMin, true, 0},
      {-1, kMin, false, kMax},    {kMin, kMin, false, 0}, {kMax, kMax, false, 0},
      {kMin + 1, 1, false, kMin}, {5, 3, false, 2},
  };
}  // namespace

TEST_CASE("multiplication overflow detection is exact") {
  run(mulOverflowsPortable, kMulCases);
  run(mulOverflows, kMulCases);
}

TEST_CASE("addition overflow detection is exact") {
  run(addOverflowsPortable, kAddCases);
  run(addOverflows, kAddCases);
}

TEST_CASE("subtraction overflow detection is exact") {
  run(subOverflowsPortable, kSubCases);
  run(subOverflows, kSubCases);
}

#ifdef __SIZEOF_INT128__
// __extension__ tells -Wpedantic that the compiler's 128-bit integer is used on purpose.
__extension__ typedef __int128 WideInt;

TEST_CASE("portable overflow checks agree with 128-bit arithmetic on many values") {
  std::vector<int64_t> values
      = {0,        1,          -1,         2,           -2,          3,         1000003,
         -1000003, 3037000499, 3037000500, -3037000500, kMax,        kMin,      kMax - 1,
         kMin + 1, kMax / 2,   kMin / 2,   4294967296,  -4294967296, 4294967295};
  uint64_t state = 88172645463325252ULL;  // xorshift, so the test is deterministic
  for (int i = 0; i < 400; ++i) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    values.push_back(static_cast<int64_t>(state));
    values.push_back(static_cast<int64_t>(state >> (state % 40)));
  }
  auto fits = [](WideInt v) { return v >= kMin && v <= kMax; };
  for (int64_t a : values) {
    for (int64_t b : values) {
      int64_t out = 0;
      CHECK(mulOverflowsPortable(a, b, out) == !fits(static_cast<WideInt>(a) * b));
      CHECK(addOverflowsPortable(a, b, out) == !fits(static_cast<WideInt>(a) + b));
      CHECK(subOverflowsPortable(a, b, out) == !fits(static_cast<WideInt>(a) - b));
    }
  }
}
#endif
