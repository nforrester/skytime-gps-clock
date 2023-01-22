#include "util.h"

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

bool util_test()
{
    test_assert(mod_test());
    test_assert(nwraps_test());

    return true;
}
