#pragma once

#include <cinttypes>
#include <concepts>
#include <cstdio>

bool util_test();

#define test_assert(x) if (!(x)) { printf("Failed %s at %s:%d\n", #x, __FILE__, __LINE__); return false; }
#define test_assert_signed_eq(x, y) {auto x1 = x; auto y1 = y; if (!(x1 == y1)) { printf("Failed %d == %d at %s:%d\n", x1, y1, __FILE__, __LINE__); return false; }}
#define test_assert_unsigned_eq(x, y) {auto x1 = x; auto y1 = y; if (!(x1 == y1)) { printf("Failed 0x%" PRIx32 " == 0x%" PRIx32 " at %s:%d\n", x1, y1, __FILE__, __LINE__); return false; }}

template <typename T>
requires std::signed_integral<T>
inline T mod(T a, T b)
{
    return (a % b + b) % b;
}

template <typename T>
requires std::signed_integral<T>
inline T nwraps(T a, T b)
{
    return (a - mod(a, b)) / b;
}
