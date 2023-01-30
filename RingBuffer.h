#pragma once

#include <cstddef>

bool ring_buffer_test();

template <typename T, size_t max_elements>
class RingBuffer
{
public:
    inline bool empty() const
    {
        return _r_idx == _w_idx;
    }

    inline bool full() const
    {
        return (_w_idx + 1) % _data_len == _r_idx;
    }

    inline size_t count() const
    {
        size_t unwrapped_w_idx = _w_idx;
        if (_w_idx < _r_idx)
        {
            unwrapped_w_idx += _data_len;
        }
        return unwrapped_w_idx - _r_idx;
    }

    inline T const & peek(size_t const n) const
    {
        return _data[(_r_idx + n) % _data_len];
    }

    inline void pop(size_t const n)
    {
        _r_idx = (_r_idx + n) % _data_len;
    }

    inline void push(T const & x)
    {
        _data[_w_idx] = x;
        _w_idx = (_w_idx + 1) % _data_len;
    }

    inline void clear()
    {
        _r_idx = 0;
        _w_idx = 0;
    }

private:
    static constexpr size_t _data_len = max_elements + 1;
    T _data[_data_len];
    size_t _r_idx = 0;
    size_t _w_idx = 0;
};

