#pragma once

#include <concepts>
#include <limits>
#include <cstdlib>
#include <cstdint>

bool packing_test();

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

enum class Endian {
    Big,
    Little
};

class Endianness {
public:
    virtual Endian value() const = 0;
};
class BigEndian: public Endianness {
public:
    Endian value() const override { return Endian::Big; }
};
class LittleEndian: public Endianness {
public:
    Endian value() const override { return Endian::Little; }
};

template <size_t buffer_size, size_t idx = 0>
requires (idx <= buffer_size)
class Pack
{
public:
    inline Pack(uint8_t * const buffer, Endianness const & endianness):
        _buffer(buffer), _endianness(endianness.value()) {}
    inline Pack(uint8_t * const buffer, Endian endianness):
        _buffer(buffer), _endianness(endianness) {}

    void constexpr finalize()
    {
        static_assert(idx == buffer_size);
    }

    template <typename T>
    requires std::integral<T>
    inline auto operator<<(T value)
    {
        if (_endianness == Endian::Little)
        {
            to_little_endian<T>(_buffer + idx, value);
        }
        else
        {
            to_big_endian<T>(_buffer + idx, value);
        }
        return Pack<buffer_size, idx + sizeof(T)>(_buffer, _endianness);
    }

    template <typename T>
    requires std::derived_from<T, Endianness>
    inline auto operator<<(T const & endianness)
    {
        return Pack<buffer_size, idx>(_buffer, endianness.value());
    }

private:
    uint8_t * const _buffer;
    Endian _endianness;
};

template <size_t buffer_size, size_t idx = 0>
requires (idx <= buffer_size)
class Unpack
{
public:
    inline Unpack(uint8_t const * const buffer, Endianness const & endianness):
        _buffer(buffer), _endianness(endianness.value()) {}
    inline Unpack(uint8_t const * const buffer, Endian endianness):
        _buffer(buffer), _endianness(endianness) {}

    void constexpr finalize()
    {
        static_assert(idx == buffer_size);
    }

    template <typename T>
    requires std::integral<T>
    inline auto operator>>(T & value)
    {
        if (_endianness == Endian::Little)
        {
            value = from_little_endian<T>(_buffer + idx);
        }
        else
        {
            value = from_big_endian<T>(_buffer + idx);
        }
        return Unpack<buffer_size, idx + sizeof(T)>(_buffer, _endianness);
    }

    template <typename T>
    requires std::derived_from<T, Endianness>
    inline auto operator>>(T const & endianness)
    {
        return Unpack<buffer_size, idx>(_buffer, endianness.value());
    }

private:
    uint8_t const * const _buffer;
    Endian _endianness;
};
