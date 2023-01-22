#include "unit_tests.h"

int main()
{
    bool success = unit_tests();

    if (!success)
    {
        return 1;
    }

    return 0;
}
