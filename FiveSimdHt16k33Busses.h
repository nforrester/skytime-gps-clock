#pragma once

#include <bitset>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "hardware/pio.h"
#pragma GCC diagnostic pop

class FiveSimdHt16k33Busses
{
public:
    static size_t constexpr max_cmd_length = 17;

    FiveSimdHt16k33Busses(PIO pio, uint const clock_pin, uint const first_of_five_consecutive_data_pins);

    void dispatch();

    bool begin_write(
        uint8_t const addr,
        size_t const cmd_length,
        uint8_t const * const cmd0,
        uint8_t const * const cmd1,
        uint8_t const * const cmd2,
        uint8_t const * const cmd3,
        uint8_t const * const cmd4);

    // Returns true if the operation is complete, sets success to true if all acks were received successfully.
    bool try_end_write(bool & success);

    bool blocking_write(
        uint8_t const addr,
        size_t const cmd_length,
        uint8_t const * const cmd0,
        uint8_t const * const cmd1,
        uint8_t const * const cmd2,
        uint8_t const * const cmd3,
        uint8_t const * const cmd4);

    bool begin_read(
        uint8_t const addr,
        size_t const num_bytes_to_read,
        uint8_t const reg0,
        uint8_t const reg1,
        uint8_t const reg2,
        uint8_t const reg3,
        uint8_t const reg4);

    bool try_end_read(
        bool & success,
        uint8_t * const data0,
        uint8_t * const data1,
        uint8_t * const data2,
        uint8_t * const data3,
        uint8_t * const data4);

private:
    static size_t constexpr max_bytes = max_cmd_length + 1;

    PIO _pio;
    uint _sm;

    uint8_t _data_buffer0[max_bytes];
    uint8_t _data_buffer1[max_bytes];
    uint8_t _data_buffer2[max_bytes];
    uint8_t _data_buffer3[max_bytes];
    uint8_t _data_buffer4[max_bytes];

    std::bitset<max_bytes> _acks_to_transmit;
    std::bitset<max_bytes> _acks_expected;

    size_t _cmd_length;

    size_t _bytes_to_transmit;
    size_t _bytes_and_ack_batches_transmitted;
    size_t _bytes_and_ack_batches_received;
    bool _all_acks_match_expectation;

    size_t _nibbles_plus_acks_transmitted;
    size_t _nibbles_plus_acks_received;

    bool _operation_begun;
    bool _operation_ended;

    size_t _bytes_to_read_after_this_operation_is_complete;
    bool _write_preceeding_read_worked;

    void _make_progress_tx();
    void _make_progress_rx();

    void _try_begin_read_part_2();
};
