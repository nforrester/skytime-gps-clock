#pragma once

#include <cstdint>
#include <cstdio>
#include <concepts>
#include <limits>
#include <cinttypes>

bool util_test();

#define test_assert(x) if (!(x)) { printf("Failed %s at %s:%d\n", #x, __FILE__, __LINE__); return false; }
#define test_assert_signed_eq(x, y) {auto x1 = x; auto y1 = y; if (!(x1 == y1)) { printf("Failed %d == %d at %s:%d\n", x1, y1, __FILE__, __LINE__); return false; }}
#define test_assert_unsigned_eq(x, y) {auto x1 = x; auto y1 = y; if (!(x1 == y1)) { printf("Failed 0x%" PRIx32 " == 0x%" PRIx32 " at %s:%d\n", x1, y1, __FILE__, __LINE__); return false; }}


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


template <typename U, typename S>
requires std::same_as<U, typename WidthMatch<S>::u>
inline constexpr S signed_from_unsigned_twos_complement(U twos_complement)
{
    if (twos_complement > std::numeric_limits<S>::max())
    {
        U const u_absolute_value = (~twos_complement) + 1;
        if (u_absolute_value > std::numeric_limits<S>::max())
        {
            return std::numeric_limits<S>::lowest();
        }
        S const s_absolute_value = u_absolute_value;
        return -s_absolute_value;
    }
    S value = twos_complement;
    return value;
}

template <typename U, typename S>
requires std::same_as<U, typename WidthMatch<S>::u>
inline constexpr U unsigned_twos_complement_from_signed(S value)
{
    U const twos_complement = value;
    return twos_complement;
}

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

template <typename T>
requires std::unsigned_integral<T>
inline void to_big_endian(uint8_t * bytes, T value)
{
    for (size_t bytes_idx = 0; bytes_idx < sizeof(T); ++bytes_idx)
    {
        size_t const value_idx = sizeof(T) - bytes_idx - 1;
        bytes[bytes_idx] = (value >> (value_idx * 8)) & 0xff;
    }
}

template <typename T>
requires std::signed_integral<T>
inline void to_big_endian(uint8_t * bytes, T value)
{
    using UT = WidthMatch<T>::u;
    to_big_endian(bytes, unsigned_twos_complement_from_signed<UT, T>(value));
}

template <typename T>
requires std::unsigned_integral<T>
inline T from_big_endian(uint8_t const * const bytes)
{
    T value = 0;
    for (ssize_t value_idx = sizeof(T) - 1; value_idx >= 0; --value_idx)
    {
        size_t const bytes_idx = sizeof(T) - value_idx - 1;
        value <<= 8;
        value |= bytes[bytes_idx];
    }
    return value;
}

template <typename T>
requires std::signed_integral<T>
inline T from_big_endian(uint8_t const * const bytes)
{
    using UT = WidthMatch<T>::u;
    return signed_from_unsigned_twos_complement<UT, T>(from_big_endian<UT>(bytes));
}

template <typename T>
requires std::unsigned_integral<T>
inline void to_little_endian(uint8_t * bytes, T value)
{
    for (size_t bytes_idx = 0; bytes_idx < sizeof(T); ++bytes_idx)
    {
        size_t const value_idx = bytes_idx;
        bytes[bytes_idx] = (value >> (value_idx * 8)) & 0xff;
    }
}

template <typename T>
requires std::signed_integral<T>
inline void to_little_endian(uint8_t * bytes, T value)
{
    using UT = WidthMatch<T>::u;
    to_little_endian(bytes, unsigned_twos_complement_from_signed<UT, T>(value));
}

template <typename T>
requires std::unsigned_integral<T>
inline T from_little_endian(uint8_t const * const bytes)
{
    T value = 0;
    for (ssize_t value_idx = sizeof(T) - 1; value_idx >= 0; --value_idx)
    {
        size_t const bytes_idx = value_idx;
        value <<= 8;
        value |= bytes[bytes_idx];
    }
    return value;
}

template <typename T>
requires std::signed_integral<T>
inline T from_little_endian(uint8_t const * const bytes)
{
    using UT = WidthMatch<T>::u;
    return signed_from_unsigned_twos_complement<UT, T>(from_little_endian<UT>(bytes));
}

