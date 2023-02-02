#include "FiveSimdHt16k33Busses.h"

#include "five_simd_ht16k33_busses.pio.h"

FiveSimdHt16k33Busses::FiveSimdHt16k33Busses(PIO pio, uint const clock_pin, uint const first_of_five_consecutive_data_pins):
    _pio(pio)
{
    uint offset = pio_add_program(_pio, &five_simd_ht16k33_busses_program);
    _sm = pio_claim_unused_sm(_pio, true);
    five_simd_ht16k33_busses_program_init(_pio, _sm, offset, 50000, clock_pin, first_of_five_consecutive_data_pins);

    _cmd_length = 0;

    _bytes_to_transmit = 0;
    _bytes_and_ack_batches_transmitted = 0;
    _bytes_and_ack_batches_received = 0;
    _all_acks_match_expectation = false;

    _nibbles_plus_acks_transmitted = 0;
    _nibbles_plus_acks_received = 0;

    _operation_begun = true;
    _operation_ended = true;

    _bytes_to_read_after_this_operation_is_complete = 0;
    _write_preceeding_read_worked = false;
}

void FiveSimdHt16k33Busses::dispatch()
{
    if (!_operation_begun)
    {
        if (!five_simd_ht16k33_busses_program_try_begin_command(_pio, _sm, _cmd_length))
        {
            return;
        }
        _operation_begun = true;
    }

    _make_progress_tx();
    _make_progress_rx();
    _try_begin_read_part_2();
}

void FiveSimdHt16k33Busses::_make_progress_tx()
{
    while (_bytes_and_ack_batches_transmitted < _bytes_to_transmit)
    {
        if (_nibbles_plus_acks_transmitted == 0)
        {
            if (!five_simd_ht16k33_busses_program_try_send_nibble_1(
                    _pio,
                    _sm,
                    _data_buffer0[_bytes_and_ack_batches_transmitted],
                    _data_buffer1[_bytes_and_ack_batches_transmitted],
                    _data_buffer2[_bytes_and_ack_batches_transmitted],
                    _data_buffer3[_bytes_and_ack_batches_transmitted],
                    _data_buffer4[_bytes_and_ack_batches_transmitted]))
            {
                return;
            }
            _nibbles_plus_acks_transmitted = 1;
        }
        else if (_nibbles_plus_acks_transmitted == 1)
        {
            if (!five_simd_ht16k33_busses_program_try_send_nibble_2(
                    _pio,
                    _sm,
                    _data_buffer0[_bytes_and_ack_batches_transmitted],
                    _data_buffer1[_bytes_and_ack_batches_transmitted],
                    _data_buffer2[_bytes_and_ack_batches_transmitted],
                    _data_buffer3[_bytes_and_ack_batches_transmitted],
                    _data_buffer4[_bytes_and_ack_batches_transmitted]))
            {
                return;
            }
            _nibbles_plus_acks_transmitted = 2;
        }
        else
        {
            if (!five_simd_ht16k33_busses_program_try_send_acks(
                _pio,
                _sm,
                _acks_to_transmit[_bytes_and_ack_batches_transmitted]))
            {
                return;
            }
            ++_bytes_and_ack_batches_transmitted;
            _nibbles_plus_acks_transmitted = 0;
        }
    }
}

void FiveSimdHt16k33Busses::_make_progress_rx()
{
    while (_bytes_and_ack_batches_received < _bytes_to_transmit)
    {
        if (_nibbles_plus_acks_received == 0)
        {
            if (!five_simd_ht16k33_busses_program_try_get_nibble_1(
                    _pio,
                    _sm,
                    _data_buffer0[_bytes_and_ack_batches_received],
                    _data_buffer1[_bytes_and_ack_batches_received],
                    _data_buffer2[_bytes_and_ack_batches_received],
                    _data_buffer3[_bytes_and_ack_batches_received],
                    _data_buffer4[_bytes_and_ack_batches_received]))
            {
                return;
            }
            _nibbles_plus_acks_received = 1;
        }
        else if (_nibbles_plus_acks_received == 1)
        {
            if (!five_simd_ht16k33_busses_program_try_get_nibble_2(
                    _pio,
                    _sm,
                    _data_buffer0[_bytes_and_ack_batches_received],
                    _data_buffer1[_bytes_and_ack_batches_received],
                    _data_buffer2[_bytes_and_ack_batches_received],
                    _data_buffer3[_bytes_and_ack_batches_received],
                    _data_buffer4[_bytes_and_ack_batches_received]))
            {
                return;
            }
            _nibbles_plus_acks_received = 2;
        }
        else
        {
            bool this_ack_batch_matches_expectation = false;
            if (!five_simd_ht16k33_busses_program_try_get_acks(
                _pio,
                _sm,
                _acks_expected[_bytes_and_ack_batches_received],
                this_ack_batch_matches_expectation))
            {
                return;
            }
            _all_acks_match_expectation = _all_acks_match_expectation && this_ack_batch_matches_expectation;
            ++_bytes_and_ack_batches_received;
            _nibbles_plus_acks_received = 0;
        }
    }
}

bool FiveSimdHt16k33Busses::blocking_write(
    uint8_t const addr,
    size_t const cmd_length,
    uint8_t const * const cmd0,
    uint8_t const * const cmd1,
    uint8_t const * const cmd2,
    uint8_t const * const cmd3,
    uint8_t const * const cmd4)
{
    bool success = false;
    while (!begin_write(addr, cmd_length, cmd0, cmd1, cmd2, cmd3, cmd4))
    {
        dispatch();
    }
    while (!try_end_write(success))
    {
        dispatch();
    }
    return success;
}

bool FiveSimdHt16k33Busses::begin_write(
    uint8_t const addr,
    size_t const cmd_length,
    uint8_t const * const cmd0,
    uint8_t const * const cmd1,
    uint8_t const * const cmd2,
    uint8_t const * const cmd3,
    uint8_t const * const cmd4)
{
    if (!_operation_ended)
    {
        return false;
    }

    bool constexpr write = true;
    uint8_t const addr_with_write = (addr << 1) | (write ? 0 : 1);

    _data_buffer0[0] = addr_with_write;
    _data_buffer1[0] = addr_with_write;
    _data_buffer2[0] = addr_with_write;
    _data_buffer3[0] = addr_with_write;
    _data_buffer4[0] = addr_with_write;
    _acks_to_transmit[0] = false;
    _acks_expected[0] = true;

    for (size_t i = 0; i < cmd_length; ++i)
    {
        _data_buffer0[i+1] = *(cmd0+i);
        _data_buffer1[i+1] = *(cmd1+i);
        _data_buffer2[i+1] = *(cmd2+i);
        _data_buffer3[i+1] = *(cmd3+i);
        _data_buffer4[i+1] = *(cmd4+i);
        _acks_to_transmit[i+1] = false;
        _acks_expected[i+1] = true;
    }

    _cmd_length = cmd_length;

    _bytes_to_transmit = cmd_length + 1;
    _bytes_and_ack_batches_transmitted = 0;
    _bytes_and_ack_batches_received = 0;
    _all_acks_match_expectation = true;

    _nibbles_plus_acks_transmitted = 0;
    _nibbles_plus_acks_received = 0;

    _operation_begun = false;
    _operation_ended = false;

    _bytes_to_read_after_this_operation_is_complete = 0;

    return true;
}

bool FiveSimdHt16k33Busses::try_end_write(bool & success)
{
    if (_operation_ended)
    {
        return false;
    }

    if (_bytes_and_ack_batches_received != _bytes_to_transmit)
    {
        return false;
    }

    success = _all_acks_match_expectation;
    _operation_ended = true;
    return true;
}

bool FiveSimdHt16k33Busses::begin_read(
    uint8_t const addr,
    size_t const num_bytes_to_read,
    uint8_t const reg0,
    uint8_t const reg1,
    uint8_t const reg2,
    uint8_t const reg3,
    uint8_t const reg4)
{
    if (!begin_write(addr, 1, &reg0, &reg1, &reg2, &reg3, &reg4))
    {
        return false;
    }

    _bytes_to_read_after_this_operation_is_complete = num_bytes_to_read;
    return true;
}

void FiveSimdHt16k33Busses::_try_begin_read_part_2()
{
    if (_bytes_to_read_after_this_operation_is_complete == 0)
    {
        return;
    }

    if (!try_end_write(_write_preceeding_read_worked))
    {
        return;
    }

    // This could go wrong if there was interference on the bus during the preceeding write operation,
    // but it saves a separate variable to remember the address.
    uint8_t const addr_with_write = 1 | _data_buffer0[0];

    _data_buffer0[0] = addr_with_write;
    _data_buffer1[0] = addr_with_write;
    _data_buffer2[0] = addr_with_write;
    _data_buffer3[0] = addr_with_write;
    _data_buffer4[0] = addr_with_write;
    _acks_to_transmit[0] = false;
    _acks_expected[0] = true;

    for (size_t i = 0; i < _bytes_to_read_after_this_operation_is_complete; ++i)
    {
        _data_buffer0[i+1] = 0xff;
        _data_buffer1[i+1] = 0xff;
        _data_buffer2[i+1] = 0xff;
        _data_buffer3[i+1] = 0xff;
        _data_buffer4[i+1] = 0xff;
        _acks_to_transmit[i+1] = true;
        _acks_expected[i+1] = true;
    }
    _acks_to_transmit[_bytes_to_read_after_this_operation_is_complete] = false;
    _acks_expected[_bytes_to_read_after_this_operation_is_complete] = false;

    _cmd_length = _bytes_to_read_after_this_operation_is_complete;

    _bytes_to_transmit = _bytes_to_read_after_this_operation_is_complete + 1;
    _bytes_and_ack_batches_transmitted = 0;
    _bytes_and_ack_batches_received = 0;
    _all_acks_match_expectation = true;

    _nibbles_plus_acks_transmitted = 0;
    _nibbles_plus_acks_received = 0;

    _operation_begun = false;
    _operation_ended = false;

    _bytes_to_read_after_this_operation_is_complete = 0;
}

bool FiveSimdHt16k33Busses::try_end_read(
    bool & success,
    uint8_t * const data0,
    uint8_t * const data1,
    uint8_t * const data2,
    uint8_t * const data3,
    uint8_t * const data4)
{
    if (_operation_ended)
    {
        return false;
    }

    if (_bytes_and_ack_batches_received != _bytes_to_transmit)
    {
        return false;
    }

    success = _all_acks_match_expectation && _write_preceeding_read_worked;

    if (success)
    {
        for (size_t i = 0; i < _cmd_length; ++i)
        {
            if (data0 != nullptr) { *(data0+i) = _data_buffer0[i+1]; }
            if (data1 != nullptr) { *(data1+i) = _data_buffer1[i+1]; }
            if (data2 != nullptr) { *(data2+i) = _data_buffer2[i+1]; }
            if (data3 != nullptr) { *(data3+i) = _data_buffer3[i+1]; }
            if (data4 != nullptr) { *(data4+i) = _data_buffer4[i+1]; }
        }
    }

    _operation_ended = true;
    return true;
}
