#include "unit_tests.h"
#include "util.h"
#include "time.h"
#include "packing.h"
#include "RingBuffer.h"

bool unit_tests()
{
    test_assert(util_test());
    test_assert(time_test());
    test_assert(packing_test());
    test_assert(ring_buffer_test());

    return true;
}
