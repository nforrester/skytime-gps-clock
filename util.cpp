#include "util.h"

#define COMMA ,

template <typename S, typename U>
bool twos_complement_round_trip_test(S const s, U const u)
{
    S const u2s = signed_from_unsigned_twos_complement<U, S>(u);
    U const s2u = unsigned_twos_complement_from_signed<U, S>(s);
    test_assert(s == u2s);
    test_assert(u == s2u);
    return true;
}

template <typename S, typename U>
bool twos_complement_test()
{
    S smax = std::numeric_limits<S>::max();
    S slow = std::numeric_limits<S>::lowest();
    U umax = std::numeric_limits<U>::max();

    U smax_as_u = smax;

    #define T(s, u) test_assert(twos_complement_round_trip_test<S COMMA U>(s, u))
    T(0, 0u);
    T(1, 1u);
    T(50, 50u);
    T(smax, smax_as_u);
    T(slow, smax_as_u+1u);
    T(-1, umax);
    T(-50, umax-49u);
    #undef T

    if (sizeof(U) <= 2)
    {
        U u = 0;
        do
        {
            S u2s = signed_from_unsigned_twos_complement<U, S>(u);
            U u2s2u = unsigned_twos_complement_from_signed<U, S>(u2s);
            test_assert(u == u2s2u);
            ++u;
        } while(u != 0);

        S s = std::numeric_limits<S>::lowest();
        while (true)
        {
            U s2u = unsigned_twos_complement_from_signed<U, S>(s);
            S s2u2s = signed_from_unsigned_twos_complement<U, S>(s2u);
            test_assert(s == s2u2s);
            if (s == std::numeric_limits<S>::max())
            {
                break;
            }
            ++s;
        }
    }

    return true;
}

bool mod_test()
{
    test_assert(mod(-9, 6) == 3);
    test_assert(mod(-8, 6) == 4);
    test_assert(mod(-7, 6) == 5);
    test_assert(mod(-6, 6) == 0);
    test_assert(mod(-5, 6) == 1);
    test_assert(mod(-4, 6) == 2);
    test_assert(mod(-3, 6) == 3);
    test_assert(mod(-2, 6) == 4);
    test_assert(mod(-1, 6) == 5);
    test_assert(mod( 0, 6) == 0);
    test_assert(mod( 1, 6) == 1);
    test_assert(mod( 2, 6) == 2);
    test_assert(mod( 3, 6) == 3);
    test_assert(mod( 4, 6) == 4);
    test_assert(mod( 5, 6) == 5);
    test_assert(mod( 6, 6) == 0);
    test_assert(mod( 7, 6) == 1);
    test_assert(mod( 8, 6) == 2);
    test_assert(mod( 9, 6) == 3);

    return true;
}

bool nwraps_test()
{
    test_assert(nwraps(-9, 6) == -2);
    test_assert(nwraps(-8, 6) == -2);
    test_assert(nwraps(-7, 6) == -2);
    test_assert(nwraps(-6, 6) == -1);
    test_assert(nwraps(-5, 6) == -1);
    test_assert(nwraps(-4, 6) == -1);
    test_assert(nwraps(-3, 6) == -1);
    test_assert(nwraps(-2, 6) == -1);
    test_assert(nwraps(-1, 6) == -1);
    test_assert(nwraps( 0, 6) ==  0);
    test_assert(nwraps( 1, 6) ==  0);
    test_assert(nwraps( 2, 6) ==  0);
    test_assert(nwraps( 3, 6) ==  0);
    test_assert(nwraps( 4, 6) ==  0);
    test_assert(nwraps( 5, 6) ==  0);
    test_assert(nwraps( 6, 6) ==  1);
    test_assert(nwraps( 7, 6) ==  1);
    test_assert(nwraps( 8, 6) ==  1);
    test_assert(nwraps( 9, 6) ==  1);

    return true;
}

template <typename T>
bool endian_converters_round_trip_test(T const x)
{
    uint8_t bytes[sizeof(T)];

    to_big_endian<T>(bytes, x);
    test_assert(from_big_endian<T>(bytes) == x);

    to_little_endian<T>(bytes, x);
    test_assert(from_little_endian<T>(bytes) == x);

    return true;
}

template <typename S, typename U>
bool endian_converters()
{
    S smax = std::numeric_limits<S>::max();
    S slow = std::numeric_limits<S>::lowest();
    U umax = std::numeric_limits<U>::max();

    U smax_as_u = smax;

    test_assert(endian_converters_round_trip_test<S>(0));
    test_assert(endian_converters_round_trip_test<U>(0u));
    test_assert(endian_converters_round_trip_test<S>(1));
    test_assert(endian_converters_round_trip_test<U>(1u));
    test_assert(endian_converters_round_trip_test<S>(50));
    test_assert(endian_converters_round_trip_test<U>(50u));
    test_assert(endian_converters_round_trip_test<S>(-50));
    test_assert(endian_converters_round_trip_test<S>(smax));
    test_assert(endian_converters_round_trip_test<S>(smax));
    test_assert(endian_converters_round_trip_test<S>(slow));
    test_assert(endian_converters_round_trip_test<U>(umax));
    test_assert(endian_converters_round_trip_test<U>(smax_as_u));

    uint64_t x = 0xfedcba9876543210u;
    U xu = static_cast<U>(static_cast<uint64_t>(umax) & x);
    S xp = static_cast<S>(xu & (umax >> 1));
    S xn = -1 * xp;

    test_assert(endian_converters_round_trip_test<U>(xu));
    test_assert(endian_converters_round_trip_test<S>(xp));
    test_assert(endian_converters_round_trip_test<S>(xn));

    uint8_t bytes[sizeof(U)];

    to_big_endian(bytes, xu);
    for (size_t i = 0; i < sizeof(U); ++i)
    {
        test_assert(
            bytes[i] ==
            static_cast<uint8_t>(0xff & (xu >> (8 * (sizeof(U) - 1 - i)))));
    }

    to_big_endian(bytes, xp);
    for (size_t i = 0; i < sizeof(U); ++i)
    {
        test_assert(
            bytes[i] ==
            static_cast<uint8_t>(0xff & (xp >> (8 * (sizeof(U) - 1 - i)))));
    }

    to_little_endian(bytes, xu);
    for (size_t i = 0; i < sizeof(U); ++i)
    {
        test_assert(
            bytes[i] ==
            static_cast<uint8_t>(0xff & (xu >> (8 * i))));
    }

    to_little_endian(bytes, xp);
    for (size_t i = 0; i < sizeof(U); ++i)
    {
        test_assert(
            bytes[i] ==
            static_cast<uint8_t>(0xff & (xp >> (8 * i))));
    }

    return true;
}

bool pack_test()
{
    int8_t i8 = -92;
    uint8_t u8 = 83;
    int16_t i16 = 29340;
    uint16_t u16 = 48203;
    int32_t i32 = -1482953208;
    uint32_t u32 = 1280482521;
    int64_t i64 = 8169285025543689661;
    uint64_t u64 = 8393743792361950692;

    uint8_t bytes[30];
    (Pack<sizeof(bytes)>(bytes, LittleEndian())
        << i8
        << u8
        << i16
        << BigEndian()
        << u16
        << i32
        << u32
        << LittleEndian()
        << i64
        << u64).finalize();

    int8_t ri8;
    uint8_t ru8;
    int16_t ri16;
    uint16_t ru16;
    int32_t ri32;
    uint32_t ru32;
    int64_t ri64;
    uint64_t ru64;

    (Unpack<sizeof(bytes)>(bytes, LittleEndian())
        >> ri8
        >> ru8
        >> ri16
        >> BigEndian()
        >> ru16
        >> ri32
        >> ru32
        >> LittleEndian()
        >> ri64
        >> ru64).finalize();

    test_assert(i8 == ri8);
    test_assert(u8 == ru8);
    test_assert(i16 == ri16);
    test_assert(u16 == ru16);
    test_assert(i32 == ri32);
    test_assert(u32 == ru32);
    test_assert(i64 == ri64);
    test_assert(u64 == ru64);

    int8_t   mi8  = from_little_endian<int8_t  >(&bytes[0]);
    uint8_t  mu8  = from_little_endian<uint8_t >(&bytes[1]);
    int16_t  mi16 = from_little_endian<int16_t >(&bytes[2]);
    uint16_t mu16 = from_big_endian<uint16_t>(&bytes[4]);
    int32_t  mi32 = from_big_endian<int32_t >(&bytes[6]);
    uint32_t mu32 = from_big_endian<uint32_t>(&bytes[10]);
    int64_t  mi64 = from_little_endian<int64_t >(&bytes[14]);
    uint64_t mu64 = from_little_endian<uint64_t>(&bytes[22]);

    test_assert(i8 == mi8);
    test_assert(u8 == mu8);
    test_assert(i16 == mi16);
    test_assert(u16 == mu16);
    test_assert(i32 == mi32);
    test_assert(u32 == mu32);
    test_assert(i64 == mi64);
    test_assert(u64 == mu64);

    return true;
}

bool util_test()
{
    #define T(S, U) test_assert(twos_complement_test<S COMMA U>())
    T(int8_t, uint8_t);
    T(int16_t, uint16_t);
    T(int32_t, uint32_t);
    T(int64_t, uint64_t);
    #undef T

    test_assert(mod_test());
    test_assert(nwraps_test());

    #define T(S, U) test_assert(endian_converters<S COMMA U>())
    T(int8_t, uint8_t);
    T(int16_t, uint16_t);
    T(int32_t, uint32_t);
    T(int64_t, uint64_t);
    #undef T

    test_assert(pack_test());

    return true;
}
