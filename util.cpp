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

    return true;
}
