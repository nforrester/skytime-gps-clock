#include "util.h"
#include "time.h"

bool run_tests()
{
    test_assert(util_test());
    test_assert(time_test());

    return true;
}

int main()
{
    bool success = run_tests();

    if (!success)
    {
        return 1;
    }

    return 0;
}
