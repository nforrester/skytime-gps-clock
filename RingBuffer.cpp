#include "RingBuffer.h"

#include "util.h"

bool ring_buffer_test()
{
    RingBuffer<uint8_t, 5> buffer;
    test_assert(buffer.empty());
    test_assert(!buffer.full());
    test_assert(buffer.count() == 0u);

    buffer.push(5);
    test_assert(!buffer.empty());
    test_assert(!buffer.full());
    test_assert(buffer.count() == 1u);
    test_assert(buffer.peek(0) == 5);
    buffer.pop(1);
    test_assert(buffer.empty());
    test_assert(!buffer.full());
    test_assert(buffer.count() == 0u);

    buffer.push(7);
    buffer.push(3);
    buffer.push(5);
    buffer.push(2);
    test_assert(!buffer.empty());
    test_assert(!buffer.full());
    buffer.push(1);
    test_assert(!buffer.empty());
    test_assert(buffer.full());
    test_assert(buffer.count() == 5u);
    test_assert(buffer.peek(0) == 7);
    test_assert(buffer.peek(1) == 3);
    test_assert(buffer.peek(2) == 5);
    test_assert(buffer.peek(3) == 2);
    test_assert(buffer.peek(4) == 1);
    buffer.pop(2);
    buffer.push(9);
    buffer.push(6);
    test_assert(!buffer.empty());
    test_assert(buffer.full());
    test_assert(buffer.count() == 5u);
    test_assert(buffer.peek(0) == 5);
    test_assert(buffer.peek(1) == 2);
    test_assert(buffer.peek(2) == 1);
    test_assert(buffer.peek(3) == 9);
    test_assert(buffer.peek(4) == 6);
    buffer.pop(5);
    test_assert(buffer.empty());
    test_assert(!buffer.full());
    test_assert(buffer.count() == 0u);

    return true;
}
