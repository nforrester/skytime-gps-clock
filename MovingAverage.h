#pragma once

#include "RingBuffer.h"

template <typename T, size_t max_elements>
class MovingAverage
{
public:
    MovingAverage(T default_value)
    {
        reset(default_value);
    }

    void reset(T default_value)
    {
        _points.clear();
        _points.push(default_value);
        _moving_total = default_value;
        _value_defaulted = true;
    }

    void add_point(T x)
    {
        if (_points.full() || _value_defaulted)
        {
            _moving_total -= _points.peek(0);
            _points.pop(1);
            _value_defaulted = false;
        }
        _points.push(x);
        _moving_total += x;
    }

    template <typename R>
    R get_current_average() const
    {
        return static_cast<R>(_moving_total) / static_cast<R>(_points.count());
    }

private:
    RingBuffer<T, max_elements> _points;
    T _moving_total;
    bool _value_defaulted;
};
