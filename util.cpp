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

    return true;
}
