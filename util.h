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

template <typename T, size_t size>
concept IsWidth = sizeof(T) == size;

template <typename T>
class WidthMatch
{
};

template <typename T>
requires std::integral<T> && IsWidth<T, 8>
class WidthMatch<T>
{
public:
    using u = uint64_t;
    using s = int64_t;
};

template <typename T>
requires std::integral<T> && IsWidth<T, 4>
class WidthMatch<T>
{
public:
    using u = uint32_t;
    using s = int32_t;
};

template <typename T>
requires std::integral<T> && IsWidth<T, 2>
class WidthMatch<T>
{
public:
    using u = uint16_t;
    using s = int16_t;
};

template <typename T>
requires std::integral<T> && IsWidth<T, 1>
class WidthMatch<T>
{
public:
    using u = uint8_t;
    using s = int8_t;
};

template <std::unsigned_integral U>
inline WidthMatch<U>::s signed_difference(U a, U b)
{
    using S = WidthMatch<U>::s;

    if (a >= b)
    {
        return static_cast<S>(a - b);
    }
    else
    {
        return static_cast<S>(b - a) * -1;
    }
}
